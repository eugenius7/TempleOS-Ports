
.section .init
.org 0x000
.global _super_start
.global _tos_syscall
.global main
.global __uClibc_main
.global _tos_hlt
.global _end
_super_start:
    # %rdi is argc from loader
    # %rsi is argv from loader
    jmp     uclibc_main_launch

    .ascii "jdw"

.org 0x008
version:
    # qword [1]
    # First version number is filled in by make_program.py
    .long  0
    # Second version number is set here:
    .long  0x1
syscall_address:
    # qword [2]
    # Filled in by loader (at runtime):
    .quad   0
fini_address:
    # qword [3]
    # Set by linker:
    .quad   fini_end
end_address:
    # qword [4]
    # Set by linker:
    .quad   _end
load_size:
    # qword [5]
    # Set by make_program.py:
    .quad  0
total_size:
    # qword [6]
    # Set by make_program.py:
    .quad  0
reserved:
    # qword[7]
    # Not used:
    .quad  0
init_proc:
    retq
fini_proc:
    retq

.section .fini
.align 4
fini_end:

.text
_tos_syscall:
    mov     %rdx, %rcx      # syscall arg 3 ->  Linux arg 4
    mov     %rsi, %rdx      # syscall arg 2 ->  Linux arg 3
    mov     %rdi, %rsi      # syscall arg 1 ->  Linux arg 2
    mov     %rax, %rdi      # syscall number -> Linux arg 1
    jmpq    *syscall_address(%rip)

_tos_hlt:
    mov     $99999, %rax
    jmpq    *syscall_address(%rip)

uclibc_main_launch:
    // check for double launch;
    // if version == 0, this is the second time the program was launched
    mov     version(%rip), %rax
    cmp     $0, %rax
    jz      _tos_hlt
    mov     $0, %rax
    mov     %rax, version(%rip)

    // launch uclibc start
    mov     %rsi, %rdx              # arg 3 (argv) from loader
    mov     %rdi, %rsi              # arg 2 (argc) from loader
    lea     main(%rip), %rdi        # arg 1 location of main
    lea     init_proc(%rip), %rcx   # arg 4 app_init
    lea     fini_proc(%rip), %r8    # arg 5 app_fini
    mov     $0, %r9                 # arg 6 rtld_fini
    pushq   %r9                     # arg 7 stack_end
    call    __uClibc_main
    // should not return
    call    _tos_hlt


