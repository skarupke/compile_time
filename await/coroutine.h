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

#include "stack_swap.h"
#include "function.hpp"
#ifndef CORO_NO_EXCEPTIONS
#	include <exception>
#	include <stdexcept>
#endif
#ifndef CORO_ASSERT
#	include <cassert>
#	define CORO_ASSERT(cond) assert(cond)
#endif

#ifndef CORO_DEFAULT_STACK_SIZE
#	define CORO_DEFAULT_STACK_SIZE 64 * 1024
#endif

#ifndef CORO_ALLOCATE_BYTES
#	define CORO_ALLOCATE_BYTES(size) new unsigned char[size]
#endif
#ifndef CORO_FREE_BYTES
#	define CORO_FREE_BYTES(memory, size) delete[] memory
#endif

namespace coro
{
struct coroutine_stack
{
	coroutine_stack(unsigned char * begin, unsigned char * end)
		: begin(begin), size_(end - begin), need_to_delete(false)
	{
	}
	explicit coroutine_stack(size_t size)
		: begin(CORO_ALLOCATE_BYTES(size)), size_(size), need_to_delete(true)
	{
	}
	coroutine_stack(coroutine_stack && other)
		: begin(other.begin), size_(other.size_), need_to_delete(other.need_to_delete)
	{
		other.begin = nullptr;
		other.size_ = 0;
		other.need_to_delete = false;
	}

	~coroutine_stack()
	{
		if (need_to_delete)
		{
			CORO_FREE_BYTES(begin, size_);
		}
	}

	unsigned char * get() const
	{
		return begin;
	}

	size_t size() const
	{
		return size_;
	}

private:
	coroutine_stack(const coroutine_stack &); // intentionally not implemented
	coroutine_stack & operator=(const coroutine_stack &); // intentionally not implemented
	coroutine_stack & operator=(coroutine_stack &&); // intentionally not implemented
	unsigned char * begin;
	size_t size_ : 63;
	bool need_to_delete : 1;
};
static_assert(sizeof(coroutine_stack) == sizeof(void *) * 2, "expect size to be begin and end pointer");

struct coroutine
{
	struct self
	{
		void yield()
		{
			owner->stack_context.switch_out_of();
		}

	private:
		self(coroutine & owner);
		coroutine * owner;
		friend struct coroutine;
	};

	explicit coroutine(coroutine_stack stack = coroutine_stack(CORO_DEFAULT_STACK_SIZE));
	coroutine(func::movable_function<void (self &)> func, coroutine_stack stack = coroutine_stack(CORO_DEFAULT_STACK_SIZE));
	~coroutine();

	void reset(func::movable_function<void (self &)> func);

#ifdef CORO_NO_EXCEPTIONS
	void operator()()
	{
		if (!*this)
		{
			CORO_ASSERT(bool(*this));
			return;
		}
		stack_context.switch_into(); // will continue here if yielded or returned
	}
#else
	void operator()()
	{
		if (!*this)
		{
			CORO_ASSERT(bool(*this));
			throw std::logic_error("You tried to call a coroutine in an invalid state");
		}
		stack_context.switch_into(); // will continue here if yielded or returned
		if (exception) std::rethrow_exception(std::move(exception));
	}
#endif
	// will return true if you can call this coroutine
	operator bool() const
	{
		return run_state == NOT_STARTED || run_state == RUNNING;
	}

private:
	// intentionally not implemented
	coroutine(const coroutine &);
	coroutine & operator=(const coroutine &);
	coroutine(coroutine &&);
	coroutine & operator=(coroutine &&);

	func::movable_function<void (self &)> func;
	coroutine_stack stack;
	stack::stack_context stack_context;
#	ifndef CORO_NO_EXCEPTIONS
		std::exception_ptr exception;
#	endif
	enum RunState
	{
		NOT_STARTED,
		RUNNING,
		FINISHED,
		UNINITIALIZED
	} run_state;

	// this is the function that the coroutine will start off in
#ifdef CORO_NO_EXCEPTIONS
	static void coroutine_start(void * ptr)
	{
		coroutine & this_ = *static_cast<coroutine *>(ptr);
		this_.run_state = RUNNING;
		self self_(this_);
		this_.func(self_);
		this_.run_state = FINISHED;
	}
#else
	static void coroutine_start(void * ptr)
	{
		coroutine & this_ = *static_cast<coroutine *>(ptr);
		this_.run_state = RUNNING;
		try
		{
			self self_(this_);
			this_.func(self_);
		}
		catch(...)
		{
			this_.exception = std::current_exception();
		}
		this_.run_state = FINISHED;
	}
#endif
};

}
