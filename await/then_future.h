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

#include <type_traits>
#include "function.hpp"
#include <exception>
#include <memory>
#include <future>
#include <atomic>

template<typename T>
struct then_promise;
template<typename T>
struct then_future;

namespace detail
{
struct two_thread_gate
{
	two_thread_gate()
		: count(0)
	{
	}
	void reset()
	{
		count = 0;
	}
	// will return true when the second thread signals
	bool signal()
	{
		return ++count == 2;
	}

private:
	std::atomic<int> count;
};

template<typename T>
struct continuation
{
	void run(then_future<T> & self)
	{
		if (signal.signal()) next(self);
	}

	template<typename Func>
	void set(then_future<T> & self, Func && value)
	{
		next = std::forward<Func>(value);
		run(self);
	}

private:
	func::movable_function<void (then_future<T> &)> next;
	two_thread_gate signal;
};

template<typename T>
struct future_shared_state
{
	std::future<T> future;
	continuation<T> cont;
};

template<typename Promise, typename Func>
void set_promise_value(Promise & promise, Func && func)
{
	try
	{
		promise.set_value(func());
	}
	catch(...)	// no need for special handling of the case that set_value()
				// throws because set_exception() will throw the same exception
	{
		promise.set_exception(std::current_exception());
	}
}
template<template <typename> class promise, typename Func>
void set_promise_value(promise<void> & p, Func && func)
{
	try
	{
		func();
		p.set_value();
	}
	catch(...)	// no need for special handling of the case that set_value()
				// throws because set_exception() will throw the same exception
	{
		p.set_exception(std::current_exception());
	}
}
template<typename T>
struct base_promise
{
	base_promise() = default;
	explicit base_promise(std::shared_ptr<future_shared_state<T> > state)
		: future_state(std::move(state))
	{
		future_state->future = promise.get_future();
	}

	void set_exception(std::exception_ptr ptr)
	{
		promise.set_exception(std::move(ptr));
		run_continuation();
	}

	then_future<T> get_future()
	{
		future_state = std::make_shared<future_shared_state<T> >();
		future_state->future = promise.get_future();
		then_future<T> result(future_state);
		if (need_to_run_continuation) future_state->cont.run(result);
		return result;
	}

protected:
	void run_continuation()
	{
		if (future_state)
		{
			then_future<T> copy(future_state);
			future_state->cont.run(copy);
		}
		else
		{
			need_to_run_continuation = true;
		}
	}

	std::promise<T> promise;
private:
	std::shared_ptr<future_shared_state<T> > future_state;
	bool need_to_run_continuation = false;
};
template<typename T>
struct async_call_in_destructor
{
	template<typename Func>
	struct CallFunc
	{
		void operator()()
		{
			set_promise_value(promise, func);
		}

		then_promise<T> promise;
		Func func;
	};

	template<typename Func>
	async_call_in_destructor(std::shared_ptr<future_shared_state<T> > state, Func && func)
		: thread(CallFunc<typename std::decay<Func>::type>{then_promise<T>(state), std::forward<Func>(func)})
	{
	}

	void operator()()
	{
		if (thread.joinable()) thread.join();
	}

private:
	std::thread thread;
};
template<typename T, typename U, typename Func>
struct continuation_shared_state : future_shared_state<T>
{
	struct Caller
	{
		void operator()(then_future<U> & previous)
		{
			then_promise<T> local_promise = std::move(promise);
			detail::set_promise_value(local_promise, [&]{ return self->func(previous); });
		}

		then_promise<T> promise;
		std::shared_ptr<continuation_shared_state> self;
	};

	static std::shared_ptr<future_shared_state<T> > make(then_future<U> previous, Func func)
	{
		std::shared_ptr<continuation_shared_state> result = std::make_shared<continuation_shared_state>(std::move(func));
		then_promise<T> promise(result);
		previous.state->cont.set(previous, Caller{std::move(promise), result});
		return result;
	}

	continuation_shared_state(Func && func)
		: func(std::move(func))
	{
	}

	Func func;
};
}

template<typename T>
struct then_future
{
	template<typename, typename, typename>
	friend struct detail::continuation_shared_state;

	typedef detail::future_shared_state<T> shared_state;
	then_future() = default;
	then_future(then_future &&) = default;
	then_future & operator=(then_future &&) = default;
	then_future(std::shared_ptr<shared_state> state, func::movable_function<void ()> call_in_destructor = {})
		: state(std::move(state)), call_in_destructor(std::move(call_in_destructor))
	{
	}
	~then_future()
	{
		if (call_in_destructor) call_in_destructor();
	}

	bool valid() const
	{
		return bool(state);
	}
	T get()
	{
		return state->future.get();
	}
	void wait() const
	{
		state->future.wait();
	}
	template<typename Rep, typename Period>
	std::future_status wait_for(const std::chrono::duration<Rep, Period> & duration)
	{
		return state->future.wait_for(duration);
	}
	template<typename Clock, typename Duration>
	std::future_status wait_until(const std::chrono::time_point<Clock, Duration> & time_point)
	{
		return state->future.wait_until(time_point);
	}

	template<typename Func>
	auto then(Func && func) -> then_future<decltype(func(*this))>
	{
		return { detail::continuation_shared_state<decltype(func(*this)), T, typename std::decay<Func>::type>::make(std::move(state), std::forward<Func>(func)), std::move(call_in_destructor) };
	}

private:
	std::shared_ptr<shared_state> state;
	func::movable_function<void ()> call_in_destructor;
};

template<typename T>
struct then_promise : detail::base_promise<T>
{
	using detail::base_promise<T>::base_promise;

	void set_value(T && value)
	{
		this->promise.set_value(std::move(value));
		this->run_continuation();
	}
	void set_value(const T & value)
	{
		this->promise.set_value(value);
		this->run_continuation();
	}
};

template<typename T>
struct then_promise<T &> : detail::base_promise<T &>
{
	using detail::base_promise<T &>::base_promise;

	void set_value(T & value)
	{
		this->promise.set_value(value);
		this->run_continuation();
	}
};

template<>
struct then_promise<void> : detail::base_promise<void>
{
	using detail::base_promise<void>::base_promise;

	void set_value()
	{
		this->promise.set_value();
		this->run_continuation();
	}
};


template<typename Func>
auto custom_async(Func && func) -> then_future<decltype(func())>
{
	typedef decltype(func()) Result;
	std::shared_ptr<detail::future_shared_state<Result> > state = std::make_shared<detail::future_shared_state<Result> >();
	return
	{
		state,
		detail::async_call_in_destructor<Result>(state, std::forward<Func>(func))
	};
}
