/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)sqrt.s	5.1 (Berkeley) %G%
 */

/*
 * sqrt(x) 
 * returns the square root of x correctly rounded according
 * to the rounding mode.
 */

	.text
	.globl	_sqrt

_sqrt:
	fsqrtd	sp@(4),fp0
	fmoved	fp0,sp@-
	movel	sp@+,d0
	movel	sp@+,d1
	rts
