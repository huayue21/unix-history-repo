/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * This code is derived from software contributed to Berkeley by
 * Computer Consoles Inc.
 */

#if defined(LIBC_SCCS) && !defined(lint)
	.asciz "@(#)bcmp.s	1.3 (Berkeley) %G%"
#endif /* LIBC_SCCS and not lint */

/* bcmp(s1, s2, n) */
/* compare exactly 'n' */
#include "DEFS.h"

ENTRY(bcmp, 0)
	movl	4(fp),r0
	movl	8(fp),r1
	mnegl	12(fp),r2
	jeql	3f
1:
	cmpb	(r0),(r1)
	jneq	2f
	incl	r0
	incl	r1
	aoblss	$0,r2,1b
3:
	clrl	r0
	ret
2:
	movl	$1,r0
	ret
