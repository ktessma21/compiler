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
	movq $3, %rdi
	movq $5, %rsi
	movq $7, %rdx
	movq $9, %rcx
	movq $11, %r8
	movq $13, %r9
	movq $15, -16(%rsp)
	movq $17, -24(%rsp)
	movq $_testFunction_ret, -8(%rsp)
	subq $24, %rsp
	jmp _testFunction
_testFunction_ret:
	retq
_testFunction:
	subq $64, %rsp #Allocate locals
	movq %rdi, 0(%rsp)
	movq %rsi, 8(%rsp)
	movq %rdx, 16(%rsp)
	movq %rcx, 24(%rsp)
	movq %r8, 32(%rsp)
	movq %r9, 40(%rsp)
	movq 72(%rsp), %rdi
	movq %rdi, 48(%rsp)
	movq 64(%rsp), %rdi
	movq %rdi, 56(%rsp)
	movq 0(%rsp), %rdi
	call print
	movq 8(%rsp), %rdi
	call print
	movq 16(%rsp), %rdi
	call print
	movq 24(%rsp), %rdi
	call print
	movq 32(%rsp), %rdi
	call print
	movq 40(%rsp), %rdi
	call print
	movq 48(%rsp), %rdi
	call print
	movq 56(%rsp), %rdi
	call print
	addq $128, %rsp #Deallocate locals
	retq

