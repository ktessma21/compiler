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
	subq $8, %rsp #Allocate locals
	movq %r12, 0(%rsp)
	movq $0, %r12
_startLoop:
	cmpq $5, %r12
	jle _loopBody
	jmp exitLoop #goto
_loopBody:
	movq $5, %rdi
	movq $_testFunction_ret, -8(%rsp)
	subq $16, %rsp #allocate space for arguments and return address
	jmp _testFunction
_testFunction_ret:
	movq %rax, %rdi
	call print # runtime system call
	addq $2, %r12
	jmp startLoop #goto
_exitLoop:
	movq 0(%rsp), %r12
	addq $8, %rsp #Deallocate locals
	retq
_testFunction:
	movq %rdi, %rax
	retq

