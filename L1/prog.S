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

_initialize_array:
	subq $48, %rsp #Allocate locals
	movq %rdi, %rdi
	movq %rdi, 0(%rsp)
	movq %rsi, %rdi
	movq %rdi, 32(%rsp)
	movq 0(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 16(%rsp)
	movq $0, %rdi
	movq %rdi, 24(%rsp)
	movq $0, %rdi
	movq %rdi, 40(%rsp)
_call_label0:
	movq 40(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 40(%rsp)
	movq 40(%rsp), %rdi
	salq $1, %rdi
	movq %rdi, 40(%rsp)
	movq 40(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 40(%rsp)
	movq 40(%rsp), %rdi
	addq $1, %rdi
	movq %rdi, 40(%rsp)
	movq 16(%rsp), %rdi
	movq 40(%rsp), %rsi
	movq %rsi, 0(%rdi)
	movq 40(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 40(%rsp)
	movq 40(%rsp), %rdi
	subq $1, %rdi
	movq %rdi, 40(%rsp)
	movq 40(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 40(%rsp)
	movq 40(%rsp), %rdi
	sarq $1, %rdi
	movq %rdi, 40(%rsp)
	movq 40(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 40(%rsp)
	movq 40(%rsp), %rdi
	addq $1, %rdi
	movq %rdi, 40(%rsp)
	movq 16(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 16(%rsp)
	movq 16(%rsp), %rdi
	addq $8, %rdi
	movq %rdi, 16(%rsp)
	movq 24(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 24(%rsp)
	movq 24(%rsp), %rdi
	addq $1, %rdi
	movq %rdi, 24(%rsp)
	movq 24(%rsp), %rdi
	movq 32(%rsp), %rsi
	cmpq %rdi, %rsi
	setg  %dil
	movzbq %dil, %rdi
	movq %rdi, 8(%rsp)
	movq 8(%rsp), %rdi
	cmpq $1, %rdi
	je _call_label0
	jmp _call_label1 #goto
_call_label1:
	addq $48, %rsp
	retq
_flip_array:
	subq $80, %rsp #Allocate locals
	movq %rdi, %rdi
	movq %rdi, 48(%rsp)
	movq %rsi, %rdi
	movq %rdi, 56(%rsp)
	movq %rdx, %rdi
	movq %rdi, 40(%rsp)
	movq 48(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 0(%rsp)
	movq 56(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 8(%rsp)
	movq $0, %rdi
	movq %rdi, 32(%rsp)
	movq 40(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 40(%rsp)
	movq 40(%rsp), %rdi
	subq $1, %rdi
	movq %rdi, 40(%rsp)
	movq 40(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 40(%rsp)
	movq 40(%rsp), %rdi
	sarq $1, %rdi
	movq %rdi, 40(%rsp)
	movq 40(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 24(%rsp)
	movq 24(%rsp), %rdi
	subq $1, %rdi
	movq %rdi, 24(%rsp)
	movq 24(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 24(%rsp)
	movq 24(%rsp), %rdi
	imulq $8, %rdi
	movq %rdi, 24(%rsp)
	movq 8(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 8(%rsp)
	movq 8(%rsp), %rdi
	movq 24(%rsp), %rsi
	addq %rsi, %rdi
	movq %rdi, 8(%rsp)
	movq 56(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 64(%rsp)
	movq 64(%rsp), %rdi
	subq $8, %rdi
	movq %rdi, 64(%rsp)
_call_label_ciao_2:
	movq 0(%rsp), %rdi
	movq 0(%rdi), %rdi
	movq %rdi, 72(%rsp)
	movq 8(%rsp), %rdi
	movq 72(%rsp), %rsi
	movq %rsi, 0(%rdi)
	movq 0(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 0(%rsp)
	movq 0(%rsp), %rdi
	addq $8, %rdi
	movq %rdi, 0(%rsp)
	movq 8(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 8(%rsp)
	movq 8(%rsp), %rdi
	subq $8, %rdi
	movq %rdi, 8(%rsp)
	movq 32(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 32(%rsp)
	movq 32(%rsp), %rdi
	addq $1, %rdi
	movq %rdi, 32(%rsp)
	movq 32(%rsp), %rdi
	movq 40(%rsp), %rsi
	cmpq %rdi, %rsi
	setg  %dil
	movzbq %dil, %rdi
	movq %rdi, 16(%rsp)
	movq 16(%rsp), %rdi
	cmpq $1, %rdi
	je _call_label_ciao_2
	jmp _call_label_ciao_3 #goto
_call_label_ciao_3:
	addq $80, %rsp
	retq
_main:
	subq $48, %rsp #Allocate locals
	movq $1, %rdi
	movq %rdi, 16(%rsp)
	movq $20, %rdi
	movq %rdi, 24(%rsp)
	movq 24(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 24(%rsp)
	movq 24(%rsp), %rdi
	salq $1, %rdi
	movq %rdi, 24(%rsp)
	movq 24(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 24(%rsp)
	movq 24(%rsp), %rdi
	addq $1, %rdi
	movq %rdi, 24(%rsp)
	movq 24(%rsp), %rdi
	movq %rdi, %rdi
	movq 16(%rsp), %rsi
	movq %rsi, %rsi
	call allocate
	movq %rax, %rdi
	movq %rdi, 0(%rsp)
	movq 0(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 32(%rsp)
	movq 32(%rsp), %rdi
	addq $8, %rdi
	movq %rdi, 32(%rsp)
	movq 32(%rsp), %rdi
	movq %rdi, %rdi
	movq 24(%rsp), %rsi
	movq %rsi, %rsi
	movq $_call_label_ciao__ciao_4, -8(%rsp)
	subq $8, %rsp
	jmp _initialize_array
_call_label_ciao__ciao_4:
	movq 24(%rsp), %rdi
	movq %rdi, %rdi
	movq 16(%rsp), %rsi
	movq %rsi, %rsi
	call allocate
	movq %rax, %rdi
	movq %rdi, 8(%rsp)
	movq 8(%rsp), %rdi
	movq %rdi, %rdi
	movq %rdi, 40(%rsp)
	movq 40(%rsp), %rdi
	addq $8, %rdi
	movq %rdi, 40(%rsp)
	movq 32(%rsp), %rdi
	movq %rdi, %rdi
	movq 40(%rsp), %rsi
	movq %rsi, %rsi
	movq 24(%rsp), %rdx
	movq %rdx, %rdx
	movq $_call_label_ciao__ciao_5, -8(%rsp)
	subq $8, %rsp
	jmp _flip_array
_call_label_ciao__ciao_5:
	movq 0(%rsp), %rdi
	movq %rdi, %rdi
	call print
	movq 8(%rsp), %rdi
	movq %rdi, %rdi
	call print
	addq $48, %rsp
	retq

