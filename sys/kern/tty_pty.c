/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)tty_pty.c	8.4 (Berkeley) 2/20/95
 * $Id: tty_pty.c,v 1.47 1997/09/16 11:43:32 bde Exp $
 */

/*
 * Pseudo-teletype Driver
 * (Actually two drivers, requiring two entries in 'cdevsw')
 */
#include "pty.h"		/* XXX */

#include <sys/param.h>
#include <sys/systm.h>
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
#include <sys/ioctl_compat.h>
#endif
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>

#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#ifdef notyet
static void ptyattach __P((int n));
#endif
static void ptsstart __P((struct tty *tp));
static void ptcwakeup __P((struct tty *tp, int flag));

static	d_open_t	ptsopen;
static	d_close_t	ptsclose;
static	d_read_t	ptsread;
static	d_write_t	ptswrite;
static	d_ioctl_t	ptyioctl;
static	d_stop_t	ptsstop;
static	d_devtotty_t	ptydevtotty;
static	d_open_t	ptcopen;
static	d_close_t	ptcclose;
static	d_read_t	ptcread;
static	d_write_t	ptcwrite;
static	d_poll_t	ptcpoll;

#define CDEV_MAJOR_S 5
#define CDEV_MAJOR_C 6
static struct cdevsw pts_cdevsw = 
	{ ptsopen,	ptsclose,	ptsread,	ptswrite,	/*5*/
	  ptyioctl,	ptsstop,	nullreset,	ptydevtotty,/* ttyp */
	  ttpoll,	nommap,		NULL,	"pts",	NULL,	-1 };

static struct cdevsw ptc_cdevsw = 
	{ ptcopen,	ptcclose,	ptcread,	ptcwrite,	/*6*/
	  ptyioctl,	nullstop,	nullreset,	ptydevtotty,/* ptyp */
	  ptcpoll,	nommap,		NULL,	"ptc",	NULL,	-1 };


#if NPTY == 1
#undef NPTY
#define	NPTY	32		/* crude XXX */
#warning	You have only one pty defined, redefining to 32.
#endif

#ifdef DEVFS
#define MAXUNITS (8 * 32)
static	void	*devfs_token_pts[MAXUNITS];
static	void	*devfs_token_ptc[MAXUNITS];
static  const	char jnames[] = "pqrsPQRS";
#if NPTY > MAXUNITS
#undef NPTY
#define NPTY MAXUNITS
#warning	Can't have more than 256 pty's with DEVFS defined.
#endif
#endif

#define BUFSIZ 100		/* Chunk size iomoved to/from user */

/*
 * pts == /dev/tty[pqrsPQRS][0123456789abcdefghijklmnopqrstuv]
 * ptc == /dev/pty[pqrsPQRS][0123456789abcdefghijklmnopqrstuv]
 */
static struct	tty pt_tty[NPTY];	/* XXX */
static struct	pt_ioctl {
	int	pt_flags;
	struct	selinfo pt_selr, pt_selw;
	u_char	pt_send;
	u_char	pt_ucntl;
} pt_ioctl[NPTY];		/* XXX */
static int	npty = NPTY;		/* for pstat -t */

#define	PF_PKT		0x08		/* packet mode */
#define	PF_STOPPED	0x10		/* user told stopped */
#define	PF_REMOTE	0x20		/* remote and flow controlled input */
#define	PF_NOSTOP	0x40
#define PF_UCNTL	0x80		/* user control mode */

#ifdef notyet
/*
 * Establish n (or default if n is 1) ptys in the system.
 *
 * XXX cdevsw & pstat require the array `pty[]' to be an array
 */
static void
ptyattach(n)
	int n;
{
	char *mem;
	register u_long ntb;
#define	DEFAULT_NPTY	32

	/* maybe should allow 0 => none? */
	if (n <= 1)
		n = DEFAULT_NPTY;
	ntb = n * sizeof(struct tty);
	mem = malloc(ntb + ALIGNBYTES + n * sizeof(struct pt_ioctl),
	    M_DEVBUF, M_WAITOK);
	pt_tty = (struct tty *)mem;
	mem = (char *)ALIGN(mem + ntb);
	pt_ioctl = (struct pt_ioctl *)mem;
	npty = n;
}
#endif

/*ARGSUSED*/
static	int
ptsopen(dev, flag, devtype, p)
	dev_t dev;
	int flag, devtype;
	struct proc *p;
{
	register struct tty *tp;
	int error;

	if (minor(dev) >= npty)
		return (ENXIO);
	tp = &pt_tty[minor(dev)];
	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);		/* Set up default chars */
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		ttsetwater(tp);		/* would be done in xxparam() */
	} else if (tp->t_state&TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return (EBUSY);
	if (tp->t_oproc)			/* Ctrlr still around. */
		(void)(*linesw[tp->t_line].l_modem)(tp, 1);
	while ((tp->t_state & TS_CARR_ON) == 0) {
		if (flag&FNONBLOCK)
			break;
		error = ttysleep(tp, TSA_CARR_ON(tp), TTIPRI | PCATCH,
				 "ptsopn", 0);
		if (error)
			return (error);
	}
	error = (*linesw[tp->t_line].l_open)(dev, tp);
	if (error == 0)
		ptcwakeup(tp, FREAD|FWRITE);
	return (error);
}

static	int
ptsclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register struct tty *tp;
	int err;

	tp = &pt_tty[minor(dev)];
	err = (*linesw[tp->t_line].l_close)(tp, flag);
	ptsstop(tp, FREAD|FWRITE);
	(void) ttyclose(tp);
	return (err);
}

static	int
ptsread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct proc *p = curproc;
	register struct tty *tp = &pt_tty[minor(dev)];
	register struct pt_ioctl *pti = &pt_ioctl[minor(dev)];
	int error = 0;

again:
	if (pti->pt_flags & PF_REMOTE) {
		while (isbackground(p, tp)) {
			if ((p->p_sigignore & sigmask(SIGTTIN)) ||
			    (p->p_sigmask & sigmask(SIGTTIN)) ||
			    p->p_pgrp->pg_jobc == 0 ||
			    p->p_flag & P_PPWAIT)
				return (EIO);
			pgsignal(p->p_pgrp, SIGTTIN, 1);
			error = ttysleep(tp, &lbolt, TTIPRI | PCATCH, "ptsbg",
					 0);
			if (error)
				return (error);
		}
		if (tp->t_canq.c_cc == 0) {
			if (flag & IO_NDELAY)
				return (EWOULDBLOCK);
			error = ttysleep(tp, TSA_PTS_READ(tp), TTIPRI | PCATCH,
					 "ptsin", 0);
			if (error)
				return (error);
			goto again;
		}
		while (tp->t_canq.c_cc > 1 && uio->uio_resid > 0)
			if (ureadc(getc(&tp->t_canq), uio) < 0) {
				error = EFAULT;
				break;
			}
		if (tp->t_canq.c_cc == 1)
			(void) getc(&tp->t_canq);
		if (tp->t_canq.c_cc)
			return (error);
	} else
		if (tp->t_oproc)
			error = (*linesw[tp->t_line].l_read)(tp, uio, flag);
	ptcwakeup(tp, FWRITE);
	return (error);
}

/*
 * Write to pseudo-tty.
 * Wakeups of controlling tty will happen
 * indirectly, when tty driver calls ptsstart.
 */
static	int
ptswrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp;

	tp = &pt_tty[minor(dev)];
	if (tp->t_oproc == 0)
		return (EIO);
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

/*
 * Start output on pseudo-tty.
 * Wake up process selecting or sleeping for input from controlling tty.
 */
static void
ptsstart(tp)
	struct tty *tp;
{
	register struct pt_ioctl *pti = &pt_ioctl[minor(tp->t_dev)];

	if (tp->t_state & TS_TTSTOP)
		return;
	if (pti->pt_flags & PF_STOPPED) {
		pti->pt_flags &= ~PF_STOPPED;
		pti->pt_send = TIOCPKT_START;
	}
	ptcwakeup(tp, FREAD);
}

static void
ptcwakeup(tp, flag)
	struct tty *tp;
	int flag;
{
	struct pt_ioctl *pti = &pt_ioctl[minor(tp->t_dev)];

	if (flag & FREAD) {
		selwakeup(&pti->pt_selr);
		wakeup(TSA_PTC_READ(tp));
	}
	if (flag & FWRITE) {
		selwakeup(&pti->pt_selw);
		wakeup(TSA_PTC_WRITE(tp));
	}
}

static	int
ptcopen(dev, flag, devtype, p)
	dev_t dev;
	int flag, devtype;
	struct proc *p;
{
	register struct tty *tp;
	struct pt_ioctl *pti;

	if (minor(dev) >= npty)
		return (ENXIO);
	tp = &pt_tty[minor(dev)];
	if (tp->t_oproc)
		return (EIO);
	tp->t_oproc = ptsstart;
#ifdef sun4c
	tp->t_stop = ptsstop;
#endif
	(void)(*linesw[tp->t_line].l_modem)(tp, 1);
	tp->t_lflag &= ~EXTPROC;
	pti = &pt_ioctl[minor(dev)];
	pti->pt_flags = 0;
	pti->pt_send = 0;
	pti->pt_ucntl = 0;
	return (0);
}

static	int
ptcclose(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{
	register struct tty *tp;

	tp = &pt_tty[minor(dev)];
	(void)(*linesw[tp->t_line].l_modem)(tp, 0);

	/*
	 * XXX MDMBUF makes no sense for ptys but would inhibit the above
	 * l_modem().  CLOCAL makes sense but isn't supported.   Special
	 * l_modem()s that ignore carrier drop make no sense for ptys but
	 * may be in use because other parts of the line discipline make
	 * sense for ptys.  Recover by doing everything that a normal
	 * ttymodem() would have done except for sending a SIGHUP.
	 */
	if (tp->t_state & TS_ISOPEN) {
		tp->t_state &= ~(TS_CARR_ON | TS_CONNECTED);
		tp->t_state |= TS_ZOMBIE;
		ttyflush(tp, FREAD | FWRITE);
	}

	tp->t_oproc = 0;		/* mark closed */
	return (0);
}

static	int
ptcread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp = &pt_tty[minor(dev)];
	struct pt_ioctl *pti = &pt_ioctl[minor(dev)];
	char buf[BUFSIZ];
	int error = 0, cc;

	/*
	 * We want to block until the slave
	 * is open, and there's something to read;
	 * but if we lost the slave or we're NBIO,
	 * then return the appropriate error instead.
	 */
	for (;;) {
		if (tp->t_state&TS_ISOPEN) {
			if (pti->pt_flags&PF_PKT && pti->pt_send) {
				error = ureadc((int)pti->pt_send, uio);
				if (error)
					return (error);
				if (pti->pt_send & TIOCPKT_IOCTL) {
					cc = min(uio->uio_resid,
						sizeof(tp->t_termios));
					uiomove((caddr_t)&tp->t_termios, cc,
						uio);
				}
				pti->pt_send = 0;
				return (0);
			}
			if (pti->pt_flags&PF_UCNTL && pti->pt_ucntl) {
				error = ureadc((int)pti->pt_ucntl, uio);
				if (error)
					return (error);
				pti->pt_ucntl = 0;
				return (0);
			}
			if (tp->t_outq.c_cc && (tp->t_state&TS_TTSTOP) == 0)
				break;
		}
		if ((tp->t_state & TS_CONNECTED) == 0)
			return (0);	/* EOF */
		if (flag & IO_NDELAY)
			return (EWOULDBLOCK);
		error = tsleep(TSA_PTC_READ(tp), TTIPRI | PCATCH, "ptcin", 0);
		if (error)
			return (error);
	}
	if (pti->pt_flags & (PF_PKT|PF_UCNTL))
		error = ureadc(0, uio);
	while (uio->uio_resid > 0 && error == 0) {
		cc = q_to_b(&tp->t_outq, buf, min(uio->uio_resid, BUFSIZ));
		if (cc <= 0)
			break;
		error = uiomove(buf, cc, uio);
	}
	ttwwakeup(tp);
	return (error);
}

static	void
ptsstop(tp, flush)
	register struct tty *tp;
	int flush;
{
	struct pt_ioctl *pti = &pt_ioctl[minor(tp->t_dev)];
	int flag;

	/* note: FLUSHREAD and FLUSHWRITE already ok */
	if (flush == 0) {
		flush = TIOCPKT_STOP;
		pti->pt_flags |= PF_STOPPED;
	} else
		pti->pt_flags &= ~PF_STOPPED;
	pti->pt_send |= flush;
	/* change of perspective */
	flag = 0;
	if (flush & FREAD)
		flag |= FWRITE;
	if (flush & FWRITE)
		flag |= FREAD;
	ptcwakeup(tp, flag);
}

static	int
ptcpoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	register struct tty *tp = &pt_tty[minor(dev)];
	struct pt_ioctl *pti = &pt_ioctl[minor(dev)];
	int revents = 0;
	int s;

	if ((tp->t_state & TS_CONNECTED) == 0)
		return (seltrue(dev, events, p) | POLLHUP);

	/*
	 * Need to block timeouts (ttrstart).
	 */
	s = spltty();

	if (events & (POLLIN | POLLRDNORM))
		if ((tp->t_state & TS_ISOPEN) &&
		    ((tp->t_outq.c_cc && (tp->t_state & TS_TTSTOP) == 0) ||
		     ((pti->pt_flags & PF_PKT) && pti->pt_send) ||
		     ((pti->pt_flags & PF_UCNTL) && pti->pt_ucntl)))
			revents |= events & (POLLIN | POLLRDNORM);

	if (events & (POLLOUT | POLLWRNORM))
		if (tp->t_state & TS_ISOPEN &&
		    ((pti->pt_flags & PF_REMOTE) ?
		     (tp->t_canq.c_cc == 0) : 
		     ((tp->t_rawq.c_cc + tp->t_canq.c_cc < TTYHOG - 2) ||
		      (tp->t_canq.c_cc == 0 && (tp->t_iflag & ICANON)))))
			revents |= events & (POLLOUT | POLLWRNORM);

	if (events & POLLHUP)
		if ((tp->t_state & TS_CARR_ON) == 0)
			revents |= POLLHUP;

	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(p, &pti->pt_selr);

		if (events & (POLLOUT | POLLWRNORM)) 
			selrecord(p, &pti->pt_selw);
	}
	splx(s);

	return (revents);
}

static	int
ptcwrite(dev, uio, flag)
	dev_t dev;
	register struct uio *uio;
	int flag;
{
	register struct tty *tp = &pt_tty[minor(dev)];
	register u_char *cp = 0;
	register int cc = 0;
	u_char locbuf[BUFSIZ];
	int cnt = 0;
	struct pt_ioctl *pti = &pt_ioctl[minor(dev)];
	int error = 0;

again:
	if ((tp->t_state&TS_ISOPEN) == 0)
		goto block;
	if (pti->pt_flags & PF_REMOTE) {
		if (tp->t_canq.c_cc)
			goto block;
		while ((uio->uio_resid > 0 || cc > 0) &&
		       tp->t_canq.c_cc < TTYHOG - 1) {
			if (cc == 0) {
				cc = min(uio->uio_resid, BUFSIZ);
				cc = min(cc, TTYHOG - 1 - tp->t_canq.c_cc);
				cp = locbuf;
				error = uiomove((caddr_t)cp, cc, uio);
				if (error)
					return (error);
				/* check again for safety */
				if ((tp->t_state & TS_ISOPEN) == 0) {
					/* adjust as usual */
					uio->uio_resid += cc;
					return (EIO);
				}
			}
			if (cc > 0) {
				cc = b_to_q((char *)cp, cc, &tp->t_canq);
				/*
				 * XXX we don't guarantee that the canq size
				 * is >= TTYHOG, so the above b_to_q() may
				 * leave some bytes uncopied.  However, space
				 * is guaranteed for the null terminator if
				 * we don't fail here since (TTYHOG - 1) is
				 * not a multiple of CBSIZE.
				 */
				if (cc > 0)
					break;
			}
		}
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		(void) putc(0, &tp->t_canq);
		ttwakeup(tp);
		wakeup(TSA_PTS_READ(tp));
		return (0);
	}
	while (uio->uio_resid > 0 || cc > 0) {
		if (cc == 0) {
			cc = min(uio->uio_resid, BUFSIZ);
			cp = locbuf;
			error = uiomove((caddr_t)cp, cc, uio);
			if (error)
				return (error);
			/* check again for safety */
			if ((tp->t_state & TS_ISOPEN) == 0) {
				/* adjust for data copied in but not written */
				uio->uio_resid += cc;
				return (EIO);
			}
		}
		while (cc > 0) {
			if ((tp->t_rawq.c_cc + tp->t_canq.c_cc) >= TTYHOG - 2 &&
			   (tp->t_canq.c_cc > 0 || !(tp->t_iflag&ICANON))) {
				wakeup(TSA_HUP_OR_INPUT(tp));
				goto block;
			}
			(*linesw[tp->t_line].l_rint)(*cp++, tp);
			cnt++;
			cc--;
		}
		cc = 0;
	}
	return (0);
block:
	/*
	 * Come here to wait for slave to open, for space
	 * in outq, or space in rawq, or an empty canq.
	 */
	if ((tp->t_state & TS_CONNECTED) == 0) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		return (EIO);
	}
	if (flag & IO_NDELAY) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		if (cnt == 0)
			return (EWOULDBLOCK);
		return (0);
	}
	error = tsleep(TSA_PTC_WRITE(tp), TTOPRI | PCATCH, "ptcout", 0);
	if (error) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		return (error);
	}
	goto again;
}

static	struct tty *
ptydevtotty(dev)
	dev_t		dev;
{
	if (minor(dev) >= npty)
		return (NULL);

	return &pt_tty[minor(dev)];
}

/*ARGSUSED*/
static	int
ptyioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct tty *tp = &pt_tty[minor(dev)];
	register struct pt_ioctl *pti = &pt_ioctl[minor(dev)];
	register u_char *cc = tp->t_cc;
	int stop, error;

	/*
	 * IF CONTROLLER STTY THEN MUST FLUSH TO PREVENT A HANG.
	 * ttywflush(tp) will hang if there are characters in the outq.
	 */
	if (cmd == TIOCEXT) {
		/*
		 * When the EXTPROC bit is being toggled, we need
		 * to send an TIOCPKT_IOCTL if the packet driver
		 * is turned on.
		 */
		if (*(int *)data) {
			if (pti->pt_flags & PF_PKT) {
				pti->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp, FREAD);
			}
			tp->t_lflag |= EXTPROC;
		} else {
			if ((tp->t_lflag & EXTPROC) &&
			    (pti->pt_flags & PF_PKT)) {
				pti->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp, FREAD);
			}
			tp->t_lflag &= ~EXTPROC;
		}
		return(0);
	} else
	if (cdevsw[major(dev)]->d_open == ptcopen)
		switch (cmd) {

		case TIOCGPGRP:
			/*
			 * We aviod calling ttioctl on the controller since,
			 * in that case, tp must be the controlling terminal.
			 */
			*(int *)data = tp->t_pgrp ? tp->t_pgrp->pg_id : 0;
			return (0);

		case TIOCPKT:
			if (*(int *)data) {
				if (pti->pt_flags & PF_UCNTL)
					return (EINVAL);
				pti->pt_flags |= PF_PKT;
			} else
				pti->pt_flags &= ~PF_PKT;
			return (0);

		case TIOCUCNTL:
			if (*(int *)data) {
				if (pti->pt_flags & PF_PKT)
					return (EINVAL);
				pti->pt_flags |= PF_UCNTL;
			} else
				pti->pt_flags &= ~PF_UCNTL;
			return (0);

		case TIOCREMOTE:
			if (*(int *)data)
				pti->pt_flags |= PF_REMOTE;
			else
				pti->pt_flags &= ~PF_REMOTE;
			ttyflush(tp, FREAD|FWRITE);
			return (0);

#ifdef COMPAT_43
		case TIOCSETP:
		case TIOCSETN:
#endif
		case TIOCSETD:
		case TIOCSETA:
		case TIOCSETAW:
		case TIOCSETAF:
			ndflush(&tp->t_outq, tp->t_outq.c_cc);
			break;

		case TIOCSIG:
			if (*(unsigned int *)data >= NSIG ||
			    *(unsigned int *)data == 0)
				return(EINVAL);
			if ((tp->t_lflag&NOFLSH) == 0)
				ttyflush(tp, FREAD|FWRITE);
			pgsignal(tp->t_pgrp, *(unsigned int *)data, 1);
			if ((*(unsigned int *)data == SIGINFO) &&
			    ((tp->t_lflag&NOKERNINFO) == 0))
				ttyinfo(tp);
			return(0);
		}
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error == ENOIOCTL)
		 error = ttioctl(tp, cmd, data, flag);
	if (error == ENOIOCTL) {
		if (pti->pt_flags & PF_UCNTL &&
		    (cmd & ~0xff) == UIOCCMD(0)) {
			if (cmd & 0xff) {
				pti->pt_ucntl = (u_char)cmd;
				ptcwakeup(tp, FREAD);
			}
			return (0);
		}
		error = ENOTTY;
	}
	/*
	 * If external processing and packet mode send ioctl packet.
	 */
	if ((tp->t_lflag&EXTPROC) && (pti->pt_flags & PF_PKT)) {
		switch(cmd) {
		case TIOCSETA:
		case TIOCSETAW:
		case TIOCSETAF:
#ifdef COMPAT_43
		case TIOCSETP:
		case TIOCSETN:
#endif
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
		case TIOCSETC:
		case TIOCSLTC:
		case TIOCLBIS:
		case TIOCLBIC:
		case TIOCLSET:
#endif
			pti->pt_send |= TIOCPKT_IOCTL;
			ptcwakeup(tp, FREAD);
		default:
			break;
		}
	}
	stop = (tp->t_iflag & IXON) && CCEQ(cc[VSTOP], CTRL('s'))
		&& CCEQ(cc[VSTART], CTRL('q'));
	if (pti->pt_flags & PF_NOSTOP) {
		if (stop) {
			pti->pt_send &= ~TIOCPKT_NOSTOP;
			pti->pt_send |= TIOCPKT_DOSTOP;
			pti->pt_flags &= ~PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	} else {
		if (!stop) {
			pti->pt_send &= ~TIOCPKT_DOSTOP;
			pti->pt_send |= TIOCPKT_NOSTOP;
			pti->pt_flags |= PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	}
	return (error);
}

static ptc_devsw_installed = 0;

static void ptc_drvinit __P((void *unused));
static void
ptc_drvinit(unused)
	void *unused;
{
#ifdef DEVFS
	int i,j,k;
#endif
	dev_t dev;

	if( ! ptc_devsw_installed ) {
		dev = makedev(CDEV_MAJOR_S, 0);
		cdevsw_add(&dev, &pts_cdevsw, NULL);
		dev = makedev(CDEV_MAJOR_C, 0);
		cdevsw_add(&dev, &ptc_cdevsw, NULL);
		ptc_devsw_installed = 1;
#ifdef DEVFS
		for ( i = 0 ; i<NPTY ; i++ ) {
			j = i / 32;
			k = i % 32;
			devfs_token_pts[i] = 
				devfs_add_devswf(&pts_cdevsw,i,
						DV_CHR,0,0,0666,
						"tty%c%n",jnames[j],k);
			devfs_token_ptc[i] =
				devfs_add_devswf(&ptc_cdevsw,i,
						DV_CHR,0,0,0666,
						"pty%c%n",jnames[j],k);
		}
#endif
    	}
}

SYSINIT(ptcdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR_C,ptc_drvinit,NULL)
