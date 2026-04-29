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

	call _main

	# restore callee-saved registers and return
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	retq

_main:
	subq $16, %rsp #Allocate locals
	movq $5, %rsi
	movq $_my_label1, %rdi
	movq $3, %rdi
	movq %rdi, 8(%rsp)
	movq 8(%rsp), %rdi
	addq %rdi, %rsi
	cmpq $8, %rsi
	je _my_label1
_my_label2:
	subq $4, %rsi
	imulq $3, %rsi
	movq %rsi, %rdi
	salq $1, %rdi
	inc %rdi
	call print
	movq 8(%rsp), %rdi
lea (%rdi, %rax, 4), %rdi
	movq $5, %rdi
	movq $7, %rsi
	call allocate
	movq %rax, %rdi
	movq %rdi, 0(%rsp)
	movq 8(%rsp), %rdi
	movq %rdi, %rdi
	salq $1, %rdi
	inc %rdi
	call print
	movq 8(%rsp), %rdi
	cmpq $2, %rdi
	setg %dil
	movzbq %dil, %rdi
	movq %rdi, %rdi
	salq $1, %rdi
	inc %rdi
	call print
	movq 0(%rsp), %rdi
	movq %rdi, %rdi
	call print
	addq $16, %rsp
	retq
_my_label1:
	movq 8(%rsp), %rdi
	inc %rdi
	movq %rdi, 8(%rsp)
	jmp _my_label2 #goto
	addq $16, %rsp
	retq

