	.text
	.globl go

go:
	# save callee-saved registers
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

	call _go

	# restore callee-saved registers and return
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	retq

_fill_seq:
	movq $1, %rsi
	call allocate
	movq %rax, %rdi
	addq $8, %rdi
	movq 0(%rax), %rcx
	movq %rcx, %rdx
_loop:
	cmpq $0, %rcx
	je _done
	movq %rdx, %rsi
	subq %rcx, %rsi
	imulq $2, %rsi
	addq $1, %rsi
	movq %rsi, 0(%rdi)
	subq $1, %rcx
	addq $8, %rdi
	jmp _loop #goto
_done:
	retq
_go:
	movq $13, %rdi
	movq $_fill_ret, 8(%rsp)
	subq $8, %rsp
	jmp _fill_seq
_fill_ret:
	movq %rax, %rdi
	call print
	retq

