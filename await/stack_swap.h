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

#include <cstddef>

namespace stack
{
struct stack_context
{
	stack_context(void * stack, size_t stack_size, void (*function)(void *), void * function_argument);
	void switch_into();
	void switch_out_of();

	void reset(void * stack, size_t stack_size, void (*function)(void *), void * function_argument);

private:
	void * caller_stack_top;
	void * my_stack_top;
#ifdef CORO_LINUX_ABI_DEBUGGING_HELP
	void ** rbp_on_stack;
#endif

	// intentionally left unimplemented. there is a this pointer stored on the
	// stack and as such not even moving makes sense, because then that this
	// pointer would point to the old address
	stack_context(const stack_context &);
	stack_context & operator=(const stack_context &);
	stack_context(stack_context &&);
	stack_context & operator=(stack_context &&);
};
} // end namespace stack
