
.global go_shim
# Calling conventions for args
#  Windows:  rcx  rdx  r8   r9             
#  Linux:    rdi  rsi  rdx  rcx  r8  r9    callee-save: rbp, rbx, r12-r15
#
go_shim:
    # args: p1, argc, argv

    mov     $0, %rdi
    mov     %edx, %edi      # Win arg 2 -> argc -> Linux arg 1
    mov     %r8, %rsi       # Win arg 3 -> argv -> Linux arg 2
    jmpq    *%rcx           # Win arg 1 -> entry point

.global syscall_shim
syscall_shim:
    pushq   %rbp
    mov     %rsp, %rbp
    sub     $0x80, %rsp

    mov     %rbx,0x58(%rsp)
    mov     %r12,0x60(%rsp)
    mov     %r13,0x68(%rsp)
    mov     %r14,0x70(%rsp)
    mov     %r15,0x78(%rsp)

    mov     %rdx, %r8       # Linux arg 3 -> Win arg 3
    mov     %rcx, %r9       # Linux arg 4 -> Win arg 4
    mov     %rdi, %rcx      # Linux arg 1 -> Win arg 1
    mov     %rsi, %rdx      # Linux arg 2 -> Win arg 2
    callq   syscall_handler

    mov     0x58(%rsp),%rbx
    mov     0x60(%rsp),%r12
    mov     0x68(%rsp),%r13
    mov     0x70(%rsp),%r14
    mov     0x78(%rsp),%r15

    add     $0x80, %rsp
    popq    %rbp
    retq

