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

_fib:
	subq $16, %rsp #Allocate locals
	cmpq $1, %rdi
	jl _recurse
	movq $1, %rax
	addq $16, %rsp
	retq
_recurse:
	movq %rdi, %rsi
	movq %rsi, 0(%rsp)
	subq $1, %rdi
	movq $_f_n1, -8(%rsp)
	subq $8, %rsp
	jmp _fib
_f_n1:
	movq %rax, %rdi
	movq %rdi, 8(%rsp)
	movq 0(%rsp), %rdi
	movq %rdi, %rdi
	subq $2, %rdi
	movq $_f_n2, -8(%rsp)
	subq $8, %rsp
	jmp _fib
_f_n2:
	movq 8(%rsp), %rdi
	addq %rdi, %rax
	movq 0(%rsp), %rdi
	movq %rdi, %rdi
	addq $16, %rsp
	retq
_go:
	movq $20, %rdi
	movq $_fib_ret, -8(%rsp)
	subq $8, %rsp
	jmp _fib
_fib_ret:
	movq %rax, %rdi
	salq $1, %rdi
	addq $1, %rdi
	call print
	retq

