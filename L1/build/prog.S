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


_foo:
   sarq $1, %rdi
   sarq $1, %rsi
   sarq $1, %rcx
   movq %rdi, %rax
   imulq %rsi, %rax
   imulq %rcx, %rax
   salq $1, %rax
   addq $1, %rax
   retq


_main:
   movq $5, %rdi
   movq $7, %rsi
   movq $11, %rcx
   movq $_main_ret, -8(%rsp)
   subq $8, %rsp      # Allocate extra arguments on the stack as well as the return address 
   jmp _foo        # Call to @foo
_main_ret:
   movq %rax, %rdi
   call print # runtime system call
   retq

