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
#include "coroutine.h"

namespace coro
{
coroutine::self::self(coroutine & owner)
	: owner(&owner)
{
}
coroutine::coroutine(coroutine_stack stack)
	: func()
	, stack(std::move(stack))
	, stack_context(this->stack.get(), this->stack.size(), &coroutine_start, this)
	, run_state(UNINITIALIZED)
{
}
coroutine::coroutine(func::movable_function<void (self &)> func, coroutine_stack stack)
	: func(std::move(func))
	, stack(std::move(stack))
	, stack_context(this->stack.get(), this->stack.size(), &coroutine_start, this)
	, run_state(NOT_STARTED)
{
}
coroutine::~coroutine()
{
	CORO_ASSERT(run_state != RUNNING);
}

void coroutine::reset(func::movable_function<void (self &)> func)
{
	CORO_ASSERT(run_state != RUNNING);
	stack_context.reset(stack.get(), stack.size(), &coroutine_start, this);
	run_state = NOT_STARTED;
	this->func = std::move(func);
}
}


#ifndef DISABLE_GTEST
#include <gtest/gtest.h>

TEST(coroutine, simple)
{
	using namespace coro;
	int called = 0;
	coroutine test([&called](coroutine::self & self)
	{
		++called;
		self.yield();
		++called;
	});
	EXPECT_TRUE(bool(test));
	test();
	EXPECT_EQ(1, called);
	EXPECT_TRUE(bool(test));
	test();
	EXPECT_EQ(2, called);
	EXPECT_FALSE(bool(test));
}

TEST(coroutine, call_from_within)
{
	using namespace coro;
	std::vector<int> pushed;
	coroutine call_from_within_test([&pushed](coroutine::self & self)
	{
		coroutine inner([&pushed](coroutine::self & self)
		{
			for (int i = 0; i < 3; ++i)
			{
				pushed.push_back(1);
				self.yield();
			}
		});
		for (int i = 0; i < 3; ++i)
		{
			pushed.push_back(2);
			while (inner)
			{
				inner();
				self.yield();
			}
		}
	});

	while (call_from_within_test)
	{
		call_from_within_test();
	}
	ASSERT_EQ(6, pushed.size());
	EXPECT_EQ(2, pushed[0]);
	EXPECT_EQ(1, pushed[1]);
	EXPECT_EQ(1, pushed[2]);
	EXPECT_EQ(1, pushed[3]);
	EXPECT_EQ(2, pushed[4]);
	EXPECT_EQ(2, pushed[5]);
}

TEST(coroutine, reset)
{
	using namespace coro;
	coroutine empty;
	ASSERT_FALSE(bool(empty));

	int count = 0;
	auto to_run = [&count](coroutine::self & self)
	{
		++count;
		self.yield();
		++count;
		self.yield();
		++count;
	};
	coroutine c(to_run);
	c();
	c();
	c();
	c.reset(to_run);
	while (c) c();
	ASSERT_EQ(6, count);
}

TEST(coroutine, never_called)
{
	using namespace coro;
	coroutine([](coroutine::self &){});
}

#ifndef CORO_NO_EXCEPTIONS
TEST(coroutine, exception)
{
	using namespace coro;
	coroutine thrower([](coroutine::self &)
	{
		throw 10;
	});
	bool thrown = false;
	try
	{
		thrower();
	}
	catch(int i)
	{
		EXPECT_EQ(10, i);
		thrown = true;
	}
	EXPECT_TRUE(thrown);
}
#endif

#endif
