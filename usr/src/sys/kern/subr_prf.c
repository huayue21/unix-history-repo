/*-
 * Copyright (c) 1986, 1988, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)subr_prf.c	7.22 (Berkeley) %G%
 */

#include "param.h"
#include "systm.h"
#include "buf.h"
#include "conf.h"
#include "reboot.h"
#include "msgbuf.h"
#include "proc.h"
#include "ioctl.h"
#include "vnode.h"
#include "file.h"
#include "tty.h"
#include "tprintf.h"
#include "syslog.h"
#include "malloc.h"

/*
 * Note that stdarg.h and the ANSI style va_start macro is used for both
 * ANSI and traditional C compilers.
 */
#include <machine/stdarg.h>

#ifdef KADB
#include "machine/kdbparam.h"
#endif

#define TOCONS	0x01
#define TOTTY	0x02
#define TOLOG	0x04

struct	tty *constty;			/* pointer to console "window" tty */

#ifdef KADB
extern	cngetc();			/* standard console getc */
int	(*v_getc)() = cngetc;		/* "" getc from virtual console */
extern	cnpoll();
int	(*v_poll)() = cnpoll;		/* kdb hook to enable input polling */
#endif
extern	cnputc();			/* standard console putc */
int	(*v_putc)() = cnputc;          	/* routine to putc on virtual console */

static void logpri __P((int));
static void putchar __P((int, int, struct tty *));
static void kprintf __P((const char *, int, struct tty *, ...));
static void kprintn __P((u_long, int));

extern	cnputc();			/* standard console putc */
extern	struct tty cons;		/* standard console tty */
struct	tty *constty;			/* pointer to console "window" tty */
int	(*v_console)() = cnputc;	/* routine to putc on virtual console */

/*
 * Variable panicstr contains argument to first call to panic; used
 * as flag to indicate that the kernel has already called panic.
 */
char	*panicstr;

/*
 * Panic is called on unresolvable fatal errors.  It prints "panic: mesg",
 * and then reboots.  If we are called twice, then we avoid trying to sync
 * the disks as this often leads to recursive panics.
 */
void
panic(msg)
	char *msg;
{
	int bootopt = RB_AUTOBOOT | RB_DUMP;
	int s;

	if (panicstr)
		bootopt |= RB_NOSYNC;
	else
		panicstr = msg;
	printf("panic: %s\n", msg);
#ifdef KGDB
	kgdb_panic();
#endif
#ifdef KADB
	if (boothowto & RB_KDB) {
		s = splnet();		/* below kdb pri */
		setsoftkdb();
		splx(s);
	}
#endif
	boot(bootopt);
}

/*
 * Warn that a system table is full.
 */
void
tablefull(tab)
	char *tab;
{

	log(LOG_ERR, "%s: table is full\n", tab);
}

/*
 * Uprintf prints to the controlling terminal for the current process.
 * It may block if the tty queue is overfull.  No message is printed if
 * the queue does not clear in a reasonable time.
 */
void
#ifdef __STDC__
uprintf(const char *fmt, ...)
#else
uprintf(fmt /*, va_alist */)
	char *fmt;
#endif
{
	register struct proc *p = curproc;
	va_list ap;

	va_start(ap, fmt);
	if (p->p_flag & SCTTY && p->p_session->s_ttyvp)
		kprintf(fmt, TOTTY, p->p_session->s_ttyp, ap);
	va_end(ap);
}

tpr_t
tprintf_open(p)
	register struct proc *p;
{
	if (p->p_flag & SCTTY && p->p_session->s_ttyvp) {
		SESSHOLD(p->p_session);
		return ((tpr_t) p->p_session);
	}
	return ((tpr_t) NULL);
}

void
tprintf_close(sess)
	tpr_t sess;
{
	if (sess)
		SESSRELE((struct session *) sess);
}

/*
 * tprintf prints on the controlling terminal associated
 * with the given session.
 */
void
#ifdef __STDC__
tprintf(tpr_t tpr, const char *fmt, ...)
#else
tprintf(tpr, fmt /*, va_alist */)
	tpr_t tpr;
	char *fmt;
#endif
{
	register struct session *sess = (struct session *)tpr;
	struct tty *tp = NULL;
	int flags = TOLOG;
	va_list ap;

	logpri(LOG_INFO);
	if (sess && sess->s_ttyvp && ttycheckoutq(sess->s_ttyp, 0)) {
		flags |= TOTTY;
		tp = sess->s_ttyp;
	}
	va_start(ap, fmt);
	kprintf(fmt, flags, tp, ap);
	va_end(ap);
	logwakeup();
}

/*
 * Ttyprintf displays a message on a tty; it should be used only by
 * the tty driver, or anything that knows the underlying tty will not
 * be revoke(2)'d away.  Other callers should use tprintf.
 */
void
#ifdef __STDC__
ttyprintf(struct tty *tp, const char *fmt, ...)
#else
ttyprintf(tp, fmt /*, va_alist */)
	struct tty *tp;
	char *fmt;
#endif
{
	va_list ap;

	va_start(ap, fmt);
	kprintf(fmt, TOTTY, tp, ap);
	va_end(ap);
}

extern	int log_open;

/*
 * Log writes to the log buffer, and guarantees not to sleep (so can be
 * called by interrupt routines).  If there is no process reading the
 * log yet, it writes to the console also.
 */
void
#ifdef __STDC__
log(int level, const char *fmt, ...)
#else
log(level, fmt /*, va_alist */)
	int level;
	char *fmt;
#endif
{
	register s = splhigh();
	va_list ap;

	logpri(level);
	va_start(ap, fmt);
	kprintf(fmt, TOLOG, NULL, ap);
	splx(s);
	if (!log_open)
		kprintf(fmt, TOCONS, NULL, ap);
	va_end(ap);
	logwakeup();
}

static void
logpri(level)
	int level;
{

	putchar('<', TOLOG, NULL);
	kprintn((u_long)level, 10, TOLOG, NULL);
	putchar('>', TOLOG, NULL);
}

void
#ifdef __STDC__
addlog(const char *fmt, ...)
#else
addlog(fmt /*, va_alist */)
	char *fmt;
#endif
{
	register s = splhigh();
	va_list ap;

	va_start(ap, fmt);
	kprintf(fmt, TOLOG, NULL, ap);
	splx(s);
	if (!log_open)
		kprintf(fmt, TOCONS, NULL, ap);
	va_end(ap);
	logwakeup();
}

int	consintr = 1;			/* ok to handle console interrupts? */

void
#ifdef __STDC__
printf(const char *fmt, ...)
#else
printf(fmt /*, va_alist */)
	char *fmt;
#endif
{
	register int savintr;
	va_list ap;

	savintr = consintr;		/* disable interrupts */
	consintr = 0;
	va_start(ap, fmt);
	kprintf(fmt, TOCONS | TOLOG, NULL, ap);
	va_end(ap);
	if (!panicstr)
		logwakeup();
	consintr = savintr;		/* reenable interrupts */
}

/*
 * Scaled down version of printf(3).
 *
 * Two additional formats:
 *
 * The format %b is supported to decode error registers.
 * Its usage is:
 *
 *	kprintf("reg=%b\n", regval, "<base><arg>*");
 *
 * where <base> is the output base expressed as a control character, e.g.
 * \10 gives octal; \20 gives hex.  Each arg is a sequence of characters,
 * the first of which gives the bit number to be inspected (origin 1), and
 * the next characters (up to a control character, i.e. a character <= 32),
 * give the name of the register.  Thus:
 *
 *	kprintf("reg=%b\n", 3, "\10\2BITTWO\1BITONE\n");
 *
 * would produce output:
 *
 *	reg=3<BITTWO,BITONE>
 *
 * The format %r is supposed to pass an additional format string and argument
 * list recursively.
 * Its usage is:
 *
 * fn(otherstuff, fmt [, arg1, ... ])
 *	char *fmt;
 *	u_int arg1, ...;
 *
 *	kprintf("prefix: %r, other stuff\n", fmt, ap);
 */
static void
kprintf(fmt, flags, ttyp, ap)
	register char *fmt;
	int flags;
	struct tty *ttyp;
	va_list ap;
{
	register char *p;
	register int ch, n;
	unsigned long ul;
	int lflag, set;

	for (;;) {
		while ((ch = *fmt++) != '%') {
			if (ch == '\0')
				return;
			putchar(ch, flags, ttyp);
		}
		lflag = 0;
reswitch:	switch (ch = *fmt++) {
		case 'l':
			lflag = 1;
			goto reswitch;
		case 'b':
			ul = va_arg(ap, int);
			p = va_arg(ap, char *);
			kprintn(ul, *p++);

			if (!ul)
				break;

			for (set = 0; n = *p++;) {
				if (ul & (1 << (n - 1))) {
					putchar(set ? ',' : '<', flags, ttyp);
					for (; (n = *p) > ' '; ++p)
						putchar(n, flags, ttyp);
					set = 1;
				} else
					for (; *p > ' '; ++p);
			}
			if (set)
				putchar('>', flags, ttyp);
			break;
		case 'c':
			putchar(va_arg(ap, int), flags, ttyp);
			break;
		case 'r':
			p = va_arg(ap, char *);
			kprintf(p, flags, ttyp, va_arg(ap, va_list));
			break;
		case 's':
			p = va_arg(ap, char *);
			while (ch = *p++)
				putchar(ch, flags, ttyp);
			break;
		case 'D':
			lflag = 1;
			/* FALLTHROUGH */
		case 'd':
			ul = lflag ?
			    va_arg(ap, long) : va_arg(ap, int);
			if ((long)ul < 0) {
				putchar('-', flags, ttyp);
				ul = -(long)ul;
			}
			kprintn(ul, 10);
			break;
		case 'O':
			lflag = 1;
			/* FALLTHROUGH */
		case 'o':
			ul = lflag ?
			    va_arg(ap, u_long) : va_arg(ap, u_int);
			kprintn(ul, 8);
			break;
		case 'U':
			lflag = 1;
			/* FALLTHROUGH */
		case 'u':
			ul = lflag ?
			    va_arg(ap, u_long) : va_arg(ap, u_int);
			kprintn(ul, 10);
			break;
		case 'X':
			lflag = 1;
			/* FALLTHROUGH */
		case 'x':
			ul = lflag ?
			    va_arg(ap, u_long) : va_arg(ap, u_int);
			kprintn(ul, 16);
			break;
		default:
			putchar('%', flags, ttyp);
			if (lflag)
				putchar('l', flags, ttyp);
			putchar(ch, flags, ttyp);
		}
	}
	va_end(ap);
}

static void
kprintn(ul, base)
	u_long ul;
	int base;
{
					/* hold a long in base 8 */
	char *p, buf[(sizeof(long) * NBBY >> 3) + 1];

	p = buf;
	do {
		*p++ = "0123456789abcdef"[ul % base];
	} while (ul /= base);
	do {
		putchar(*--p);
	} while (p > buf);
}

/*
 * Print a character on console or users terminal.  If destination is
 * the console then the last MSGBUFS characters are saved in msgbuf for
 * inspection later.
 */
static void
putchar(c, flags, ttyp)
	register int c;
	int flags;
	struct tty *ttyp;
{
	extern int msgbufmapped;
	register struct msgbuf *mbp;

	if (panicstr)
		constty = NULL;
	if ((flags & TOCONS) && ttyp == NULL && constty) {
		ttyp = constty;
		flags |= TOTTY;
	}
	if ((flags & TOCONS) && panicstr == 0 && tp == 0 && constty) {
		tp = constty;
		flags |= TOTTY;
	}
	if ((flags & TOTTY) && ttyp && tputchar(c, ttyp) < 0 &&
	    (flags & TOCONS) && ttyp == constty)
		constty = NULL;
	if ((flags & TOLOG) &&
	    c != '\0' && c != '\r' && c != 0177 && msgbufmapped) {
 		mbp = msgbufp;
		if (mbp->msg_magic != MSG_MAGIC) {
			register int i;

			mbp->msg_magic = MSG_MAGIC;
			mbp->msg_bufx = mbp->msg_bufr = 0;
			for (i = 0; i < MSG_BSIZE; i++)
				mbp->msg_bufc[i] = 0;
		}
		mbp->msg_bufc[mbp->msg_bufx++] = c;
		if (mbp->msg_bufx < 0 || mbp->msg_bufx >= MSG_BSIZE)
			mbp->msg_bufx = 0;
	}
		(*v_console)(c);
}
