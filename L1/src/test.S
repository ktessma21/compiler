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
   subq $8, %rsp      # Allocate extra arguments on the stack as well as the return address 
   jmp _testFunction        # Call to @testFunction
_testFunction_ret:
   movq %rax, %rdi
   call print # runtime system call
   retq


_testFunction:
   addq $4, %rdi
   movq %rdi, %rax
   retq

