.globl isr_wrapper

# sections break this I dunno
#.section bss
.align 16
fxsave_store: .space 512

# sections break this I dunno
#.section text
.align 4

isr_wrapper:
	push %rax
	mov $0x20, %al
	outb %al, $0x20
	push %rcx
	push %rdx
	push %rbx
	push %rbp
	push %rsi
	push %rdi
	push %r8
	push %r9
	push %r10
	push %r11
	push %r12
	push %r13
	push %r14
	push %r15

	lea fxsave_store(%rip), %rax
	fxsave (%rax)

	# movups could be movaps if I can verify alignment
	#subq $128, %rsp    # Allocate space for XMM registers (16 bytes each * 8 registers)
	#movups %xmm0, (%rsp)
	#movups %xmm1, 16(%rsp)
	#movups %xmm2, 32(%rsp)
	#movups %xmm3, 48(%rsp)
	#movups %xmm4, 64(%rsp)
	#movups %xmm5, 80(%rsp)
	#movups %xmm6, 96(%rsp)
	#movups %xmm7, 112(%rsp)

	cld
	push %rbp
	mov %rsp, %rbp
	call my_irq_isr
	mov %rbp, %rsp
	pop %rbp

	#movups (%rsp), %xmm0
	#movups 16(%rsp), %xmm1
	#movups 32(%rsp), %xmm2
	#movups 48(%rsp), %xmm3
	#movups 64(%rsp), %xmm4
	#movups 80(%rsp), %xmm5
	#movups 96(%rsp), %xmm6
	#movups 112(%rsp), %xmm7
	#addq $128, %rsp

	lea fxsave_store(%rip), %rax
	fxrstor (%rax)

	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rdi
	pop %rsi
	pop %rbp
	pop %rbx
	pop %rdx
	pop %rcx
	pop %rax
	iretq
