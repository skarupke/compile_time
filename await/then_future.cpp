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
#include "then_future.h"

#ifndef DISABLE_GTEST
#include <gtest/gtest.h>
static std::atomic<size_t> num_allocations(0);
static std::atomic<size_t> num_frees(0);
//#define TEST_MEMORY_LEAKS
#ifdef TEST_MEMORY_LEAKS
void * operator new(size_t size)
{
	++num_allocations;
	return malloc(size);
}
void * operator new[](size_t size)
{
	++num_allocations;
	return malloc(size);
}
void operator delete(void * ptr) noexcept
{
	if (ptr)
	{
		++num_frees;
		free(ptr);
	}
}
void operator delete[](void * ptr) noexcept
{
	if (ptr)
	{
		++num_frees;
		free(ptr);
	}
}
#endif
namespace
{
struct ScopedAssertNoLeaks
{
	ScopedAssertNoLeaks()
		: num_allocations_before(num_allocations), num_frees_before(num_frees)
	{
	}
	~ScopedAssertNoLeaks()
	{
		[&]{ ASSERT_EQ(num_allocations - num_allocations_before, num_frees - num_frees_before); }();
	}

private:
	size_t num_allocations_before;
	size_t num_frees_before;
};

TEST(then_future, simple)
{
	ScopedAssertNoLeaks assert_no_leaks;
	then_promise<int> promise;
	then_future<int> future = promise.get_future();
	promise.set_value(5);
	ASSERT_EQ(5, future.get());
}
TEST(then_future, ref)
{
	ScopedAssertNoLeaks assert_no_leaks;
	std::unique_ptr<int> result(new int(5));
	then_promise<int &> promise;
	then_future<int &> future = promise.get_future();
	promise.set_value(*result);
	ASSERT_EQ(result.get(), &future.get());
}
TEST(then_future, void)
{
	ScopedAssertNoLeaks assert_no_leaks;
	then_promise<void> promise;
	then_future<void> future = promise.get_future();
	promise.set_value();
	future.get();
}
TEST(then_future, then)
{
	ScopedAssertNoLeaks assert_no_leaks;
	then_promise<int> promise;
	then_future<int> future = promise.get_future().then([](then_future<int> & a){ return a.get() + 5; });
	promise.set_value(5);
	ASSERT_EQ(10, future.get());
}
TEST(then_future, wait)
{
	ScopedAssertNoLeaks assert_no_leaks;
	then_promise<int> promise;
	then_future<int> future = promise.get_future();
	std::thread run_delayed([&]
	{
		promise.set_value(5);
	});
	ASSERT_EQ(5, future.get());
	run_delayed.join();
}
TEST(then_future, async_void)
{
	ScopedAssertNoLeaks assert_no_leaks;
	bool ran = false;
	bool ran2 = false;
	custom_async([&]
	{
		ran = true;
	}).then([&](then_future<void> &)
	{
		ASSERT_TRUE(ran);
		ran2 = true;
	}).get();
	ASSERT_TRUE(ran2);
}
TEST(then_future, discard_future)
{
	ScopedAssertNoLeaks assert_no_leaks;
	then_promise<void> promise;
	bool ran = false;
	promise.get_future().then([&ran](then_future<void> &){ ran = true; });
	promise.set_value();
	ASSERT_TRUE(ran);
}
TEST(then_future, discard_promise)
{
	ScopedAssertNoLeaks assert_no_leaks;
	then_future<void> future = then_promise<void>().get_future();
	bool thrown = false;
	try
	{
		future.get();
	}
	catch(const std::future_error & error)
	{
		thrown = true;
		ASSERT_EQ(std::future_errc::broken_promise, error.code());
	}
	ASSERT_TRUE(thrown);
}
TEST(then_future, discard_both)
{
	ScopedAssertNoLeaks assert_no_leaks;
	then_promise<void>().get_future().then([](then_future<void> &){});
}
TEST(then_future, discard_async)
{
	ScopedAssertNoLeaks assert_no_leaks;
	bool ran1 = false;
	custom_async([&ran1]
	{
		ran1 = true;
	});
	ASSERT_TRUE(ran1);
	bool ran2 = false;
	custom_async([]{}).then([&ran2](then_future<void> &)
	{
		ran2 = true;
	});
	ASSERT_TRUE(ran2);
}
TEST(then_future, set_before_get_future)
{
	ScopedAssertNoLeaks assert_no_leaks;
	then_promise<void> promise;
	promise.set_value();
	bool ran = false;
	promise.get_future().then([&ran](then_future<void> &){ ran = true; });
	ASSERT_TRUE(ran);
}

struct MovableFunctor
{
	MovableFunctor()
		: a(new int(5))
	{
	}

	int operator()(then_future<void> &)
	{
		return *a;
	}

	std::unique_ptr<int> a;
};
// this test is here mainly to ensure that the code
// compiles with functors that are movable only
TEST(then_future, movable)
{
	ScopedAssertNoLeaks assert_no_leaks;
	then_promise<void> promise;
	then_future<int> future = promise.get_future().then(MovableFunctor());
	promise.set_value();
	ASSERT_EQ(5, future.get());
}
TEST(then_future, valid)
{
	ScopedAssertNoLeaks assert_no_leaks;
	then_promise<void> promise;
	then_future<void> future;
	ASSERT_FALSE(future.valid());
	future = promise.get_future();
	ASSERT_TRUE(future.valid());
	then_future<void> then = future.then([](then_future<void> &){});
	ASSERT_FALSE(future.valid());
	ASSERT_TRUE(then.valid());
}
}
#endif
