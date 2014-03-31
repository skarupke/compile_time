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
#include "await.h"

AwaitTasksToFinish await_tasks_to_finish;

namespace detail
{
thread_local std::stack<ActiveCoroutine, std::vector<ActiveCoroutine> > ActiveCoroutine::active_coroutines;
}


#ifndef DISABLE_GTEST
#include <gtest/gtest.h>
namespace
{
TEST(resumable, no_await)
{
	ASSERT_EQ(5, resumable([]{ return 5; }).get());
}
TEST(resumable, async)
{
	then_future<int> background_future = custom_async([]{ return 5; });
	auto future = resumable([&background_future]
	{
		return await background_future;
	});
	await_tasks_to_finish.run_single_task_blocking();
	ASSERT_EQ(5, future.get());
}
TEST(resumable, await_twice)
{
	then_future<int> future_a = custom_async([]{ return 5; });
	then_future<int> future_b = custom_async([]{ return 5; });
	bool finished = false;
	auto future = resumable([&finished, &future_a, &future_b]
	{
		int result = await future_a + await future_b;
		finished = true;
		return result;
	});
	while (!finished) await_tasks_to_finish.run_single_task_blocking();
	ASSERT_EQ(10, future.get());
}
struct immediate_future
{
	template<typename Func>
	immediate_future(Func && func)
		: result(func())
	{
	}
	template<typename Func>
	immediate_future then(Func && func)
	{
		return immediate_future([&]{ return func(*this); });
	}
	int get() const
	{
		return result;
	}

private:
	int result;
};
TEST(resumable, finish_immediately)
{
	auto future = resumable([]
	{
		return await immediate_future([]{ return 5; });
	});
	await_tasks_to_finish.run_single_task_blocking();
	ASSERT_EQ(5, future.get());
}
TEST(resumable, operator_precedence)
{
	bool finished = false;
	auto future = resumable([&finished]
	{
		int result = await immediate_future([]{ return 5; }) * await immediate_future([]{ return 5; });
		finished = true;
		return result;
	});
	while (!finished) await_tasks_to_finish.run_single_task_blocking();
	ASSERT_EQ(25, future.get());
}

struct wait_for_other_thread
{
	wait_for_other_thread()
		: promise(), future(promise.get_future())
	{
	}
	void signal_ready()
	{
		promise.set_value();
	}
	void wait()
	{
		future.wait();
	}

private:
	std::promise<void> promise;
	std::future<void> future;
};

struct bad_timing_future
{
	struct background_thread_func
	{
		void operator()() const
		{
			a->wait();
			await_tasks_to_finish.finish_all();
			b->signal_ready();
		}

		std::shared_ptr<wait_for_other_thread> a;
		std::shared_ptr<wait_for_other_thread> b;
	};

	bad_timing_future(func::movable_function<int ()> func)
		: result(func()), a(std::make_shared<wait_for_other_thread>()), b(std::make_shared<wait_for_other_thread>())
		, background_thread(background_thread_func{a, b})
	{
	}
	bad_timing_future(bad_timing_future &&) = default;
	bad_timing_future & operator=(bad_timing_future &&) = default;
	~bad_timing_future()
	{
		if (background_thread.joinable())
			background_thread.join();
	}

	template<typename Func>
	bad_timing_future then(Func && func)
	{
		return bad_timing_future([&]{ return func(*this); });
	}
	int get()
	{
		// this will cause await_tasks_to_finish.finish_all() to run. this would break
		// the current coroutine if I didn't add it in after_yield
		a->signal_ready();
		b->wait();
		return result;
	}

private:
	int result;
	std::shared_ptr<wait_for_other_thread> a;
	std::shared_ptr<wait_for_other_thread> b;
	std::thread background_thread;
};

TEST(resumable, bad_timing)
{
	auto future = resumable([]
	{
		return await bad_timing_future([]{ return 5; });
	});
	await_tasks_to_finish.run_single_task_blocking();
	ASSERT_EQ(5, future.get());
}
TEST(resumable, twice_bad_timing)
{
	bool finished = false;
	auto future = resumable([&finished]
	{
		int result = await bad_timing_future([]{ return 5; }) + await bad_timing_future([]{ return 5; });
		finished = true;
		return result;
	});
	while (!finished) await_tasks_to_finish.run_single_task_blocking();
	ASSERT_EQ(10, future.get());
}
TEST(resumable, exception)
{
	auto future = resumable([]
	{
		throw 5;
	});
	ASSERT_THROW(future.get(), int);
	bool finished = false;
	future = resumable([&finished]
	{
		await custom_async([]{});
		finished = true;
		throw 5;
	});
	while (!finished) await_tasks_to_finish.run_single_task_blocking();
	ASSERT_THROW(future.get(), int);
}
TEST(resumable, await_exception)
{
	bool finished = false;
	then_future<void> future = resumable([]
	{
		ASSERT_THROW(await custom_async([]{ throw 5; }), int);
		await custom_async([]{ throw 6.0f; });
	}).then([&finished](then_future<void> & future)
	{
		finished = true;
		future.get();
	});
	while (!finished) await_tasks_to_finish.run_single_task_blocking();
	ASSERT_THROW(future.get(), float);
}

struct MovableFunctor
{
	MovableFunctor(int & finished, int num_iterations)
		: finished(finished), async_count(new int(num_iterations))
	{
	}

	void operator()()
	{
		if (*async_count)
		{
			--*async_count;
			await resumable(std::move(*this));
			++finished;
		}
	}

	int & finished;
	std::unique_ptr<int> async_count;
};
static_assert(!std::is_copy_constructible<MovableFunctor>::value && !std::is_copy_assignable<MovableFunctor>::value, "can't be copy constructible for my test below");
// this test is mainly here to guarantee that my code will compile with
// functors that are movable only
TEST(resumable, movable_functor)
{
	int finished = 0;
	int num_iterations = 5;
	then_future<void> future = resumable(MovableFunctor(finished, num_iterations));
	while (finished != num_iterations) await_tasks_to_finish.run_single_task_blocking();
	future.get();
}
TEST(resumable, await_or_block)
{
	ASSERT_EQ(5, await_or_block(custom_async([]{ return 5; })));
	then_future<int> future = resumable([]{ return await_or_block(custom_async([]{ return 5; })); });
	await_tasks_to_finish.run_single_task_blocking();
	ASSERT_EQ(5, future.get());
}
}

#endif
