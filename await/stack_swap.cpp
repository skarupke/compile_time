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
#include "stack_swap.h"
#include <cstring>
#include <utility>

namespace stack
{
extern "C" void switch_to_context(void ** old_stack_top, const void * new_stack_top);
extern "C" void callable_context_start();
#ifndef _WIN64
#	ifdef CORO_LINUX_ABI_DEBUGGING_HELP
extern "C" void switch_to_callable_context(void ** old_stack_top, const void * new_stack_top, void * rbp_on_stack);
extern "C" void context_switch_point();
#	endif
asm
(
"switch_to_context:\n\t"
	"pushq %rbp\n\t"
	"movq %rsp, %rbp\n\t"
	// store rbx and r12 to r15 on the stack. these will be restored
	// after we switch back
	"pushq %rbx\n\t"
	"pushq %r12\n\t"
	"pushq %r13\n\t"
	"pushq %r14\n\t"
	"pushq %r15\n\t"
	"movq %rsp, (%rdi)\n\t" // store stack pointer
	// set up the other guy's stack pointer
"context_switch_point:\n\t"
	"movq %rsi, %rsp\n\t"
	// and we are now in the other context
	// restore registers
	"popq %r15\n\t"
	"popq %r14\n\t"
	"popq %r13\n\t"
	"popq %r12\n\t"
	"popq %rbx\n\t"
	"popq %rbp\n\t"
	"retq\n\t" // go to whichever code is used by the other stack
);
#	ifdef CORO_LINUX_ABI_DEBUGGING_HELP
asm
(
"switch_to_callable_context:\n\t"
	"pushq %rbp\n\t"
	"movq %rsp, %rbp\n\t"
	// store rbx and r12 to r15 on the stack. these will be restored
	// after we jump back
	"pushq %rbx\n\t"
	"pushq %r12\n\t"
	"pushq %r13\n\t"
	"pushq %r14\n\t"
	"pushq %r15\n\t"
	"movq %rsp, (%rdi)\n\t" // store stack pointer
	// set up the other guy's stack pointer to make
	// debugging easier
	"movq %rbp, (%rdx)\n\t"
	"jmp context_switch_point\n\t"
);
#	endif

asm
(
"callable_context_start:\n\t"
#	ifdef CORO_LINUX_ABI_DEBUGGING_HELP
	"popq %rbp\n\t"		// these pop rbp push rbp instructions don't do anything but they make the
	"pushq (%rsp)\n\t"	// debugger less confused. without these the call stack breaks inside coroutines
						// unfortunately even with these I can't get callstacks to work for more than one
						// layer of coroutines.
#	endif
	"movq %r13, %rdi\n\t" // function_argument
	"callq *%r12\n\t" // function
	"movq (%rbx), %rsi\n\t" // caller_stack_top
	"jmp context_switch_point\n\t"
);
#endif

void stack_context::switch_into()
{
#ifdef CORO_LINUX_ABI_DEBUGGING_HELP
	switch_to_callable_context(&caller_stack_top, my_stack_top, rbp_on_stack);
#else
	switch_to_context(&caller_stack_top, my_stack_top);
#endif
}
void stack_context::switch_out_of()
{
	switch_to_context(&my_stack_top, caller_stack_top);
}

static void * ensure_alignment(void * stack, size_t stack_size)
{
	static const size_t CONTEXT_STACK_ALIGNMENT = 16;
	unsigned char * stack_top = static_cast<unsigned char *>(stack) + stack_size;
	// if the user gave me a non-aligned stack, just cut a couple bytes off from the top
	return stack_top - reinterpret_cast<size_t>(stack_top) % CONTEXT_STACK_ALIGNMENT;
}

void stack_context::reset(void * stack, size_t stack_size, void (*function)(void *), void * function_argument)
{
	unsigned char * math_stack = static_cast<unsigned char *>(ensure_alignment(stack, stack_size));
#ifdef _WIN64
	my_stack_top = math_stack - sizeof(void *) // space for return address (initial call)
							- sizeof(void *) * 2 // space for stack info
							- sizeof(void *) * 4 // space for arguments
							- sizeof(void *) * 8 // space for non-volatile integer registers
							//- sizeof(void *) * 2 * 10 // space for non-volatile xmm registers
							//- sizeof(void *) // stack alignment
							;
	void ** initial_stack = static_cast<void **>(my_stack_top);
	// initial_stack[11] to initial_stack[14] are space for arguments. I won't
	// use that space but the calling convention says it has to be there
	initial_stack[10] = &callable_context_start;
	initial_stack[9] = math_stack;
	initial_stack[8] = stack;
	initial_stack[7] = &caller_stack_top; // initial rbx
	initial_stack[6] = function; // initial rbp
	initial_stack[5] = function_argument; // initial rdi
	initial_stack[4] = nullptr; // initial rsi
	initial_stack[3] = nullptr; // initial r12
	initial_stack[2] = nullptr; // initial r13
	initial_stack[1] = nullptr; // initial r14
	initial_stack[0] = nullptr; // initial r15
	initial_stack[-1] = nullptr; // stack alignment
	initial_stack[-3] = initial_stack[-2] = nullptr; // initial xmm6
	initial_stack[-5] = initial_stack[-4] = nullptr; // initial xmm7
	initial_stack[-7] = initial_stack[-6] = nullptr; // initial xmm8
	initial_stack[-9] = initial_stack[-8] = nullptr; // initial xmm9
	initial_stack[-11] = initial_stack[-10] = nullptr; // initial xmm10
	initial_stack[-13] = initial_stack[-12] = nullptr; // initial xmm11
	initial_stack[-15] = initial_stack[-14] = nullptr; // initial xmm12
	initial_stack[-17] = initial_stack[-16] = nullptr; // initial xmm13
	initial_stack[-19] = initial_stack[-18] = nullptr; // initial xmm14
	initial_stack[-21] = initial_stack[-20] = nullptr; // initial xmm15
#else
#	ifdef CORO_LINUX_ABI_DEBUGGING_HELP
	my_stack_top = math_stack - sizeof(void *) * 9;
	void ** initial_stack = static_cast<void **>(my_stack_top);
	// store the return address here to make the debuggers life easier
	initial_stack[8] = reinterpret_cast<void *>(&context_switch_point);
	initial_stack[7] = nullptr; // will store rbp here to make the debuggers life easier
	rbp_on_stack = &initial_stack[7];
#	else
	my_stack_top = math_stack - sizeof(void *) * 7;
	void ** initial_stack = static_cast<void **>(my_stack_top);
#	endif
	initial_stack[6] = reinterpret_cast<void *>(&callable_context_start);
	initial_stack[5] = &initial_stack[7]; // initial rbp
	initial_stack[4] = &caller_stack_top; // initial rbx
	initial_stack[3] = reinterpret_cast<void *>(function); // initial r12
	initial_stack[2] = function_argument; // initial r13
	initial_stack[1] = nullptr; // initial r14
	initial_stack[0] = nullptr; // initial r15
#endif
}


stack_context::stack_context(void * stack, size_t stack_size, void (*function)(void *), void * function_argument)
	: caller_stack_top(nullptr), my_stack_top(nullptr)
#ifdef CORO_LINUX_ABI_DEBUGGING_HELP
	, rbp_on_stack(nullptr)
#endif
{
	reset(stack, stack_size, function, function_argument);
}

}


#ifndef DISABLE_GTEST
#include <gtest/gtest.h>

namespace
{
	struct exception_test_info
	{
		stack::stack_context * context;
		int * to_set;
	};
	void exception_call(void * arg)
	{
		exception_test_info * info = static_cast<exception_test_info *>(arg);
		try
		{
			info->context->switch_out_of();
		}
		catch(int i)
		{
			*info->to_set = i;
		}
	}
}

TEST(stack_swap, exceptions)
{
	unsigned char local_stack[64*1024];
	exception_test_info info;

	stack::stack_context context(local_stack, sizeof(local_stack), &exception_call, &info);
	info.context = &context;
	int inner_set = 0;
	info.to_set = &inner_set;
	int outer_set = 0;
	try
	{
		context.switch_into();
		throw 5;
	}
	catch(int i)
	{
		outer_set = i;
	}
	EXPECT_EQ(0, inner_set);
	EXPECT_EQ(5, outer_set);
}
#endif

