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

_go:
	movq $5, %rdi
	movq $_testFunction_ret, -8(%rsp)
	subq $16, %rsp #allocate space for arguments and return address
	jmp $_testFunction
_testFunction_ret:
	movq %rax, %rdi
	call print # runtime system call
	retq
_testFunction:
	addq $4, %rdi
	movq %rdi, %rax
	retq


