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
	movq $5, %rdi
	movq $5, %rsi
	call allocate # runtime system call
	movq %rax, %r12
	movq %r12, %rdi
	movq $_printData_ret, -8(%rsp)
	subq $8, %rsp #allocate space for return address only
	jmp _printData# callee jump
_printData_ret:
	movq 0(%rsp), %r12
	addq $8, %rsp #Deallocate locals
	retq
_printData:
	subq $16, %rsp #Allocate locals
	movq %r12, 0(%rsp)
	movq %r13, 8(%rsp)
	movq %rdi, %r12
	movq 0(%r12), %r13
	movq %r13, %rdi
	salq $1, %rdi
	addq $1, %rdi
	call print # runtime system call
	movq 0(%rsp), %r12
	movq 8(%rsp), %r13
	addq $24, %rsp #Deallocate locals
	retq

