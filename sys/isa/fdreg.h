/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)fdreg.h	7.1 (Berkeley) 5/9/91
 * $FreeBSD$
 */

/*
 * AT floppy controller registers and bitfields
 */

/* uses NEC765 controller */
#include <dev/ic/nec765.h>

/* registers */
#define	FDOUT	2	/* Digital Output Register (W) */
#define	FDO_FDSEL	0x03	/*  floppy device select */
#define	FDO_FRST	0x04	/*  floppy controller reset */
#define	FDO_FDMAEN	0x08	/*  enable floppy DMA and Interrupt */
#define	FDO_MOEN0	0x10	/*  motor enable drive 0 */
#define	FDO_MOEN1	0x20	/*  motor enable drive 1 */
#define	FDO_MOEN2	0x40	/*  motor enable drive 2 */
#define	FDO_MOEN3	0x80	/*  motor enable drive 3 */

#define	FDSTS	4	/* NEC 765 Main Status Register (R) */
#define	FDDATA	5	/* NEC 765 Data Register (R/W) */
#define	FDCTL	7	/* Control Register (W) */

/*
 * The definitions for FDC_500KBPS etc. have been moved out to <sys/fdcio.h>
 * since they need to be visible in userland.  They cover the lower two bits
 * of FDCTL when used for output.
 */
/*
 * this is the secret PIO data port (offset from base)
 */
#define FDC_YE_DATAPORT 6

#define	FDIN	7	/* Digital Input Register (R) */
#define	FDI_DCHG	0x80	/* diskette has been changed */
				/* requires drive and motor being selected */
				/* is cleared by any step pulse to drive */

/*
 * Timeout value for the PIO loops to wait until the FDC main status
 * register matches our expextations (request for master, direction
 * bit).  This is the number of cycles to loop while waiting, with a
 * 1-microsecond (in theory) DELAY() in each cycle.  In particular on
 * slower hardware, it could take a fair amount more to execute.  Of
 * course, as soon as the FDC main status register indicates the correct
 * bits are set, the loop will terminate, so this is merely a safety
 * measure to avoid looping forever in case of broken hardware.
 */
#define	FDSTS_TIMEOUT	200
