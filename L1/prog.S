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
   subq $8, %rsp # Allocate spills
   movq $5, 0(%rsp)
   addq $2, 0(%rsp)
   movq 0(%rsp), %rdi
   call print # runtime system call
   addq $8, %rsp # Free stack locations
   retq

