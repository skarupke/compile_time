.code

switch_to_context proc
	; store NT_TIB stack info members
	push qword ptr gs:[8]
	push qword ptr gs:[16]
	; store rbx and r12 to r15 on the stack. these will be restored
	; after we switch back
	push rbx
	push rbp
	push rdi
	push rsi
	push r12
	push r13
	push r14
	push r15
	; start at -24 instead of -16 because of alignment
	movaps [rsp - 24], xmm6
	movaps [rsp - 40], xmm7
	movaps [rsp - 56], xmm8
	movaps [rsp - 72], xmm9
	movaps [rsp - 88], xmm10
	movaps [rsp - 104], xmm11
	movaps [rsp - 120], xmm12
	movaps [rsp - 136], xmm13
	movaps [rsp - 152], xmm14
	movaps [rsp - 168], xmm15
	mov qword ptr [rcx], rsp ; store stack pointer
        ; fall through
switch_to_context endp

context_switch_point proc
	; set up the other guy's stack pointers
	mov rsp, rdx
	; and we are now in the other context
	; restore registers
	; start at -168 instead of -160 because of alignment
	movaps xmm15, [rsp - 168]
	movaps xmm14, [rsp - 152]
	movaps xmm13, [rsp - 136]
	movaps xmm12, [rsp - 120]
	movaps xmm11, [rsp - 104]
	movaps xmm10, [rsp - 88]
	movaps xmm9, [rsp - 72]
	movaps xmm8, [rsp - 56]
	movaps xmm7, [rsp - 40]
	movaps xmm6, [rsp - 24]
	pop r15
	pop r14
	pop r13
	pop r12
	pop rsi
	pop rdi
	pop rbp
	pop rbx
	; restore NT_TIB stack info members. this is needed because _chkstk will
	; use these members to step the stack. so we need to make sure that it, and
	; any other functions that use the stack information, get correct values
	pop qword ptr gs:[16]
	pop qword ptr gs:[8]
	ret ; go to whichever code is used by the other stack
context_switch_point endp

callable_context_start proc
	mov rcx, rdi ; function_argument
	call rbp ; function
	mov rdx, [rbx] ; caller_stack_top
        jmp context_switch_point
callable_context_start endp

end 
