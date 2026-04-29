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
   subq $16, %rsp # Allocate spills
   movq $5, %rdi
   movq $1, %rsi
   movq %rdi, 0(%rsp)
   movq %rsi, -8(%rsp)
   salq $1, %rdi
   addq $1, %rdi
   call print # runtime system call
   movq $1, %rsi
   movq 0(%rsp), %rdi
   movq $0, %rdx
_loop:
   cmpq $0, %rdi
   je _else
   imulq %rdi, %rsi
   subq $1, %rdi
   cmpq $1, %rdi
   setle %dl
   movzbq %dl, %rdx
   cmpq $0, %rdx
   je _loop
   movq %rsi, %rdi
   salq $1, %rdi
   addq $1, %rdi
   call print # runtime system call
   addq $16, %rsp # Free stack locations
   retq
_else:
   movq $1, %rdi
   salq $1, %rdi
   addq $1, %rdi
   call print # runtime system call
   addq $16, %rsp # Free stack locations
   retq

