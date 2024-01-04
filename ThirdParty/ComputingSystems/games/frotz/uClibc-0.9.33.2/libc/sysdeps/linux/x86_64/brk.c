/* brk system call for Linux/x86_64.
   Copyright (C) 1995, 1996, 2000 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>

/* This must be initialized data because commons can't have aliases.  */
void *__curbrk attribute_hidden = 0;

int brk(void *addr)
{
	void *newbrk;

	__asm__ (".global _tos_syscall\ncallq _tos_syscall\n"
		: "=a" (newbrk)
		: "0" (__NR_brk), "D" (addr)
		: "r11", "rcx"
	);

	__curbrk = newbrk;

	if (newbrk < addr) {
		__set_errno(ENOMEM);
		return -1;
	}

	return 0;
}
libc_hidden_def(brk)
