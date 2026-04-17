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

	call _testing

	# restore callee-saved registers and return
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	retq

_testing:
	movq $5, %rdi
	movq $85, %rsi
	movq $_myfunc_ret, -8(%rsp)
	call allocate
_myfunc_ret:
	movq $5, 16(%rax)
	movq %rax, %rdi
	movq $_rett, -8(%rsp)
	subq $8, %rsp
	jmp _printArray
_rett:
	retq
_printArray:
	subq $24, %rsp #Allocate locals
	movq %rdi, %rsi
	andq $1, %rsi
	cmpq $0, %rsi
	je _isarray
	jmp _notarray #goto
_notarray:
	movq $0, %rax
	addq $24, %rsp #Deallocate locals
	retq
_isarray:
	movq $0, 16(%rsp)
	movq %rdi, 8(%rsp)
	movq 0(%rdi), %rdi
	movq %rdi, 0(%rsp)
_forloop:
	movq 0(%rsp), %rsi
	movq 16(%rsp), %rdi
	cmpq %rdi, %rsi
	jl _continue
	jmp _done #goto
_continue:
	movq 8(%rsp), %rdi
	movq 16(%rsp), %rsi
	movq $_ret, -8(%rsp)
	subq $8, %rsp
	jmp _printArrayElement
_ret:
	movq 16(%rsp), %rdi
	inc %rdi
	movq %rdi, 16(%rsp)
	jmp _forloop #goto
_done:
	movq $1, %rax
	addq $24, %rsp #Deallocate locals
	retq
_printArrayElement:
	subq $24, %rsp #Allocate locals
	movq %rdi, 8(%rsp)
	movq %rsi, 16(%rsp)
	movq 16(%rsp), %rdi
	salq $1, %rdi
	addq $1, %rdi
	call print
	movq 16(%rsp), %rdi
	imulq $8, %rdi
	addq $8, %rdi
	salq $1, %rdi
	addq $1, %rdi
	movq %rdi, 0(%rsp)
	call print
	movq 0(%rsp), %rdi
	sarq $1, %rdi
	movq %rdi, 0(%rsp)
	movq 0(%rsp), %rsi
	movq 8(%rsp), %rdi
	addq %rsi, %rdi
	movq %rdi, 8(%rsp)
	movq 8(%rsp), %rdi
	movq 0(%rdi), %rdi
	movq %rdi, %rsi
	andq $1, %rsi
	movq %rdi, %rdi
	cmpq $0, %rsi
	je _isarr
	jmp _notarr #goto
_notarr:
	call print
	jmp _fnc_ret #goto
_isarr:
	movq $_fnc_ret, -8(%rsp)
	subq $8, %rsp
	jmp _printArray
_fnc_ret:
	addq $24, %rsp #Deallocate locals
	retq

