/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
 */
#pragma once

#include "coroutine.h"
#include <stack>
#include <queue>
#include <memory>
#include <functional>
#include <future>
#include <mutex>
#include "then_future.h"

struct AwaitTasksToFinish
{
	void add(func::movable_function<void ()> func)
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			tasks_to_finish.push(std::move(func));
		}
		waiter.notify_one();
	}
	void finish_all()
	{
		while (run_single_task());
	}
	// will return true if it actually ran a task
	// will return false if no task was available
	bool run_single_task()
	{
		func::movable_function<void ()> task = get_next_task();
		if (!task) return false;
		task();
		return true;
	}
	// will return after it has run a task
	void run_single_task_blocking()
	{
		while (!run_single_task()) wait_for_task();
	}

private:
	func::movable_function<void ()> get_next_task()
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (tasks_to_finish.empty()) return {};
		func::movable_function<void ()> result = std::move(tasks_to_finish.front());
		tasks_to_finish.pop();
		return result;
	}
	void wait_for_task() const
	{
		std::unique_lock<std::mutex> lock(mutex);
		waiter.wait(lock, [this]{ return !tasks_to_finish.empty(); });
	}

	mutable std::condition_variable waiter;
	mutable std::mutex mutex;
	// possible to optimize here by using a lockless queue
	std::queue<func::movable_function<void ()> > tasks_to_finish;
};

// coroutines that have used await will add themselves to this list when they
// are ready to continue running. that means that you are responsible for
// continuing these tasks. the benefit of that is that this will work with your
// existing threading framework. just regularly call
// await_tasks_to_finish.run_single_task()
extern AwaitTasksToFinish await_tasks_to_finish;

struct Awaiter;
namespace detail
{
struct ActiveCoroutine
{
	friend struct ::Awaiter;
	template<typename T, typename Func>
	ActiveCoroutine(then_promise<T> promise, Func && func)
		: heap_stuff(std::make_shared<HeapStuff>(std::move(promise), std::forward<Func>(func)))
	{
	}

	static ActiveCoroutine * get_current_coroutine()
	{
		if (active_coroutines.empty()) return nullptr;
		else return &active_coroutines.top();
	}

	void operator()()
	{
		active_coroutines.push(*this);
		heap_stuff->run_again.reset();
		heap_stuff->coroutine();
		active_coroutines.pop();
		if (heap_stuff->run_again.signal())
		{
			await_tasks_to_finish.add(*this);
		}
	}

	void yield()
	{
		heap_stuff->yielder->yield();
	}

private:
	struct HeapStuff
	{
		template<typename T, typename Func>
		HeapStuff(then_promise<T> && promise, Func && func)
			: coroutine(CoroStartFunction<T, typename std::decay<Func>::type>(std::move(promise), std::forward<Func>(func), yielder))
		{
		}

		coro::coroutine::self * yielder = nullptr;
		coro::coroutine coroutine;
		two_thread_gate run_again;

		template<typename T, typename Func>
		struct CoroStartFunction
		{
			CoroStartFunction(then_promise<T> && promise, Func function, coro::coroutine::self *& yielder)
				: yielder(yielder), promise(std::move(promise)), function(std::move(function))
			{
			}

			void operator()(coro::coroutine::self & self)
			{
				yielder = &self;
				detail::set_promise_value(this->promise, this->function);
			}

			coro::coroutine::self *& yielder;
			then_promise<T> promise;
			Func function;
		};
	};

	std::shared_ptr<HeapStuff> heap_stuff;
	static thread_local std::stack<ActiveCoroutine, std::vector<ActiveCoroutine> > active_coroutines;
};
}

struct Awaiter
{
	template<typename Future>
	auto operator->*(Future && f) const -> decltype(f.get())
	{
		detail::ActiveCoroutine * active_coroutine = detail::ActiveCoroutine::get_current_coroutine();
		if (!active_coroutine)
		{
			// not sure what to do in this case. if you call await from outside of a coroutine I can't
			// simply switch out of the current stack. there is nothing to switch to. instead I could
			// either crash or block until the result is available. since it's probably an error if
			// await blocks, I decided to crash. if you want to block instead you can use await_or_block()
			throw std::runtime_error("you can only use 'await' inside of functions called from resumable(). the reason is that I have to switch out of the current context when you call await and I don't know what to switch to from the main context");
			return f.get();
		}
		detail::ActiveCoroutine coro = *active_coroutine;
		// here is the idea: in the future's .then() callback
		// we add ourself to the await_tasks_to_finish list
		auto finish = f.then([coro](Future & f) mutable -> decltype(f.get())
		{
			if (coro.heap_stuff->run_again.signal())
			{
				await_tasks_to_finish.add(std::move(coro));
			}
			return f.get();
		});
		// now we yield, so that when the above callback triggers,
		// we'll continue here
		coro.yield();
		return finish.get();
	}
};

#define await ::Awaiter()->*

// will return true if it is valid to use the await macro in the current
// context. it is only valid to use the await macro in functions that have been
// started using the resumable() function below
inline bool can_await()
{
	return detail::ActiveCoroutine::get_current_coroutine();
}

template<typename Future>
inline auto await_or_block(Future && future) -> decltype(future.get())
{
	return can_await() ? await future : future.get();
}

template<typename Func>
auto resumable(Func && func) -> then_future<decltype(func())>
{
	typedef decltype(func()) result_type;
	then_promise<result_type> promise;
	then_future<result_type> future = promise.get_future();
	// create a coroutine with the function and start it immediately
	// we don't need to store it because if it never calls await,
	// it will finish immediately. if it does call await, it will add
	// itself to the await_tasks_to_finish list. so in either case its
	// lifetime is taken care of
	detail::ActiveCoroutine(std::move(promise), std::forward<Func>(func))();
	return future;
}
