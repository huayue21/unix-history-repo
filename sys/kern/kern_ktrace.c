/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.
 * Copyright (c) 2005 Robert N. M. Watson
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
 *	@(#)kern_ktrace.c	8.2 (Berkeley) 9/23/93
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ktrace.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ktrace.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/sysproto.h>

#include <security/mac/mac_framework.h>

/*
 * The ktrace facility allows the tracing of certain key events in user space
 * processes, such as system calls, signal delivery, context switches, and
 * user generated events using utrace(2).  It works by streaming event
 * records and data to a vnode associated with the process using the
 * ktrace(2) system call.  In general, records can be written directly from
 * the context that generates the event.  One important exception to this is
 * during a context switch, where sleeping is not permitted.  To handle this
 * case, trace events are generated using in-kernel ktr_request records, and
 * then delivered to disk at a convenient moment -- either immediately, the
 * next traceable event, at system call return, or at process exit.
 *
 * When dealing with multiple threads or processes writing to the same event
 * log, ordering guarantees are weak: specifically, if an event has multiple
 * records (i.e., system call enter and return), they may be interlaced with
 * records from another event.  Process and thread ID information is provided
 * in the record, and user applications can de-interlace events if required.
 */

static MALLOC_DEFINE(M_KTRACE, "KTRACE", "KTRACE");

#ifdef KTRACE

#ifndef KTRACE_REQUEST_POOL
#define	KTRACE_REQUEST_POOL	100
#endif

struct ktr_request {
	struct	ktr_header ktr_header;
	void	*ktr_buffer;
	union {
		struct	ktr_syscall ktr_syscall;
		struct	ktr_sysret ktr_sysret;
		struct	ktr_genio ktr_genio;
		struct	ktr_psig ktr_psig;
		struct	ktr_csw ktr_csw;
	} ktr_data;
	STAILQ_ENTRY(ktr_request) ktr_list;
};

static int data_lengths[] = {
	0,					/* none */
	offsetof(struct ktr_syscall, ktr_args),	/* KTR_SYSCALL */
	sizeof(struct ktr_sysret),		/* KTR_SYSRET */
	0,					/* KTR_NAMEI */
	sizeof(struct ktr_genio),		/* KTR_GENIO */
	sizeof(struct ktr_psig),		/* KTR_PSIG */
	sizeof(struct ktr_csw),			/* KTR_CSW */
	0,					/* KTR_USER */
	0,					/* KTR_STRUCT */
	0,					/* KTR_SYSCTL */
};

static STAILQ_HEAD(, ktr_request) ktr_free;

static SYSCTL_NODE(_kern, OID_AUTO, ktrace, CTLFLAG_RD, 0, "KTRACE options");

static u_int ktr_requestpool = KTRACE_REQUEST_POOL;
TUNABLE_INT("kern.ktrace.request_pool", &ktr_requestpool);

static u_int ktr_geniosize = PAGE_SIZE;
TUNABLE_INT("kern.ktrace.genio_size", &ktr_geniosize);
SYSCTL_UINT(_kern_ktrace, OID_AUTO, genio_size, CTLFLAG_RW, &ktr_geniosize,
    0, "Maximum size of genio event payload");

static int print_message = 1;
struct mtx ktrace_mtx;
static struct sx ktrace_sx;

static void ktrace_init(void *dummy);
static int sysctl_kern_ktrace_request_pool(SYSCTL_HANDLER_ARGS);
static u_int ktrace_resize_pool(u_int newsize);
static struct ktr_request *ktr_getrequest(int type);
static void ktr_submitrequest(struct thread *td, struct ktr_request *req);
static void ktr_freerequest(struct ktr_request *req);
static void ktr_writerequest(struct thread *td, struct ktr_request *req);
static int ktrcanset(struct thread *,struct proc *);
static int ktrsetchildren(struct thread *,struct proc *,int,int,struct vnode *);
static int ktrops(struct thread *,struct proc *,int,int,struct vnode *);

/*
 * ktrace itself generates events, such as context switches, which we do not
 * wish to trace.  Maintain a flag, TDP_INKTRACE, on each thread to determine
 * whether or not it is in a region where tracing of events should be
 * suppressed.
 */
static void
ktrace_enter(struct thread *td)
{

	KASSERT(!(td->td_pflags & TDP_INKTRACE), ("ktrace_enter: flag set"));
	td->td_pflags |= TDP_INKTRACE;
}

static void
ktrace_exit(struct thread *td)
{

	KASSERT(td->td_pflags & TDP_INKTRACE, ("ktrace_exit: flag not set"));
	td->td_pflags &= ~TDP_INKTRACE;
}

static void
ktrace_assert(struct thread *td)
{

	KASSERT(td->td_pflags & TDP_INKTRACE, ("ktrace_assert: flag not set"));
}

static void
ktrace_init(void *dummy)
{
	struct ktr_request *req;
	int i;

	mtx_init(&ktrace_mtx, "ktrace", NULL, MTX_DEF | MTX_QUIET);
	sx_init(&ktrace_sx, "ktrace_sx");
	STAILQ_INIT(&ktr_free);
	for (i = 0; i < ktr_requestpool; i++) {
		req = malloc(sizeof(struct ktr_request), M_KTRACE, M_WAITOK);
		STAILQ_INSERT_HEAD(&ktr_free, req, ktr_list);
	}
}
SYSINIT(ktrace_init, SI_SUB_KTRACE, SI_ORDER_ANY, ktrace_init, NULL);

static int
sysctl_kern_ktrace_request_pool(SYSCTL_HANDLER_ARGS)
{
	struct thread *td;
	u_int newsize, oldsize, wantsize;
	int error;

	/* Handle easy read-only case first to avoid warnings from GCC. */
	if (!req->newptr) {
		mtx_lock(&ktrace_mtx);
		oldsize = ktr_requestpool;
		mtx_unlock(&ktrace_mtx);
		return (SYSCTL_OUT(req, &oldsize, sizeof(u_int)));
	}

	error = SYSCTL_IN(req, &wantsize, sizeof(u_int));
	if (error)
		return (error);
	td = curthread;
	ktrace_enter(td);
	mtx_lock(&ktrace_mtx);
	oldsize = ktr_requestpool;
	newsize = ktrace_resize_pool(wantsize);
	mtx_unlock(&ktrace_mtx);
	ktrace_exit(td);
	error = SYSCTL_OUT(req, &oldsize, sizeof(u_int));
	if (error)
		return (error);
	if (wantsize > oldsize && newsize < wantsize)
		return (ENOSPC);
	return (0);
}
SYSCTL_PROC(_kern_ktrace, OID_AUTO, request_pool, CTLTYPE_UINT|CTLFLAG_RW,
    &ktr_requestpool, 0, sysctl_kern_ktrace_request_pool, "IU", "");

static u_int
ktrace_resize_pool(u_int newsize)
{
	struct ktr_request *req;
	int bound;

	mtx_assert(&ktrace_mtx, MA_OWNED);
	print_message = 1;
	bound = newsize - ktr_requestpool;
	if (bound == 0)
		return (ktr_requestpool);
	if (bound < 0)
		/* Shrink pool down to newsize if possible. */
		while (bound++ < 0) {
			req = STAILQ_FIRST(&ktr_free);
			if (req == NULL)
				return (ktr_requestpool);
			STAILQ_REMOVE_HEAD(&ktr_free, ktr_list);
			ktr_requestpool--;
			mtx_unlock(&ktrace_mtx);
			free(req, M_KTRACE);
			mtx_lock(&ktrace_mtx);
		}
	else
		/* Grow pool up to newsize. */
		while (bound-- > 0) {
			mtx_unlock(&ktrace_mtx);
			req = malloc(sizeof(struct ktr_request), M_KTRACE,
			    M_WAITOK);
			mtx_lock(&ktrace_mtx);
			STAILQ_INSERT_HEAD(&ktr_free, req, ktr_list);
			ktr_requestpool++;
		}
	return (ktr_requestpool);
}

/* ktr_getrequest() assumes that ktr_comm[] is the same size as p_comm[]. */
CTASSERT(sizeof(((struct ktr_header *)NULL)->ktr_comm) ==
    (sizeof((struct proc *)NULL)->p_comm));

static struct ktr_request *
ktr_getrequest(int type)
{
	struct ktr_request *req;
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	int pm;

	ktrace_enter(td);	/* XXX: In caller instead? */
	mtx_lock(&ktrace_mtx);
	if (!KTRCHECK(td, type)) {
		mtx_unlock(&ktrace_mtx);
		ktrace_exit(td);
		return (NULL);
	}
	req = STAILQ_FIRST(&ktr_free);
	if (req != NULL) {
		STAILQ_REMOVE_HEAD(&ktr_free, ktr_list);
		req->ktr_header.ktr_type = type;
		if (p->p_traceflag & KTRFAC_DROP) {
			req->ktr_header.ktr_type |= KTR_DROP;
			p->p_traceflag &= ~KTRFAC_DROP;
		}
		mtx_unlock(&ktrace_mtx);
		microtime(&req->ktr_header.ktr_time);
		req->ktr_header.ktr_pid = p->p_pid;
		req->ktr_header.ktr_tid = td->td_tid;
		bcopy(p->p_comm, req->ktr_header.ktr_comm,
		    sizeof(req->ktr_header.ktr_comm));
		req->ktr_buffer = NULL;
		req->ktr_header.ktr_len = 0;
	} else {
		p->p_traceflag |= KTRFAC_DROP;
		pm = print_message;
		print_message = 0;
		mtx_unlock(&ktrace_mtx);
		if (pm)
			printf("Out of ktrace request objects.\n");
		ktrace_exit(td);
	}
	return (req);
}

/*
 * Some trace generation environments don't permit direct access to VFS,
 * such as during a context switch where sleeping is not allowed.  Under these
 * circumstances, queue a request to the thread to be written asynchronously
 * later.
 */
static void
ktr_enqueuerequest(struct thread *td, struct ktr_request *req)
{

	mtx_lock(&ktrace_mtx);
	STAILQ_INSERT_TAIL(&td->td_proc->p_ktr, req, ktr_list);
	mtx_unlock(&ktrace_mtx);
	ktrace_exit(td);
}

/*
 * Drain any pending ktrace records from the per-thread queue to disk.  This
 * is used both internally before committing other records, and also on
 * system call return.  We drain all the ones we can find at the time when
 * drain is requested, but don't keep draining after that as those events
 * may be approximately "after" the current event.
 */
static void
ktr_drain(struct thread *td)
{
	struct ktr_request *queued_req;
	STAILQ_HEAD(, ktr_request) local_queue;

	ktrace_assert(td);
	sx_assert(&ktrace_sx, SX_XLOCKED);

	STAILQ_INIT(&local_queue);	/* XXXRW: needed? */

	if (!STAILQ_EMPTY(&td->td_proc->p_ktr)) {
		mtx_lock(&ktrace_mtx);
		STAILQ_CONCAT(&local_queue, &td->td_proc->p_ktr);
		mtx_unlock(&ktrace_mtx);

		while ((queued_req = STAILQ_FIRST(&local_queue))) {
			STAILQ_REMOVE_HEAD(&local_queue, ktr_list);
			ktr_writerequest(td, queued_req);
			ktr_freerequest(queued_req);
		}
	}
}

/*
 * Submit a trace record for immediate commit to disk -- to be used only
 * where entering VFS is OK.  First drain any pending records that may have
 * been cached in the thread.
 */
static void
ktr_submitrequest(struct thread *td, struct ktr_request *req)
{

	ktrace_assert(td);

	sx_xlock(&ktrace_sx);
	ktr_drain(td);
	ktr_writerequest(td, req);
	ktr_freerequest(req);
	sx_xunlock(&ktrace_sx);

	ktrace_exit(td);
}

static void
ktr_freerequest(struct ktr_request *req)
{

	if (req->ktr_buffer != NULL)
		free(req->ktr_buffer, M_KTRACE);
	mtx_lock(&ktrace_mtx);
	STAILQ_INSERT_HEAD(&ktr_free, req, ktr_list);
	mtx_unlock(&ktrace_mtx);
}

void
ktrsyscall(code, narg, args)
	int code, narg;
	register_t args[];
{
	struct ktr_request *req;
	struct ktr_syscall *ktp;
	size_t buflen;
	char *buf = NULL;

	buflen = sizeof(register_t) * narg;
	if (buflen > 0) {
		buf = malloc(buflen, M_KTRACE, M_WAITOK);
		bcopy(args, buf, buflen);
	}
	req = ktr_getrequest(KTR_SYSCALL);
	if (req == NULL) {
		if (buf != NULL)
			free(buf, M_KTRACE);
		return;
	}
	ktp = &req->ktr_data.ktr_syscall;
	ktp->ktr_code = code;
	ktp->ktr_narg = narg;
	if (buflen > 0) {
		req->ktr_header.ktr_len = buflen;
		req->ktr_buffer = buf;
	}
	ktr_submitrequest(curthread, req);
}

void
ktrsysret(code, error, retval)
	int code, error;
	register_t retval;
{
	struct ktr_request *req;
	struct ktr_sysret *ktp;

	req = ktr_getrequest(KTR_SYSRET);
	if (req == NULL)
		return;
	ktp = &req->ktr_data.ktr_sysret;
	ktp->ktr_code = code;
	ktp->ktr_error = error;
	ktp->ktr_retval = retval;		/* what about val2 ? */
	ktr_submitrequest(curthread, req);
}

/*
 * When a process exits, drain per-process asynchronous trace records.
 */
void
ktrprocexit(struct thread *td)
{

	ktrace_enter(td);
	sx_xlock(&ktrace_sx);
	ktr_drain(td);
	sx_xunlock(&ktrace_sx);
	ktrace_exit(td);
}

/*
 * When a thread returns, drain any asynchronous records generated by the
 * system call.
 */
void
ktruserret(struct thread *td)
{

	ktrace_enter(td);
	sx_xlock(&ktrace_sx);
	ktr_drain(td);
	sx_xunlock(&ktrace_sx);
	ktrace_exit(td);
}

void
ktrnamei(path)
	char *path;
{
	struct ktr_request *req;
	int namelen;
	char *buf = NULL;

	namelen = strlen(path);
	if (namelen > 0) {
		buf = malloc(namelen, M_KTRACE, M_WAITOK);
		bcopy(path, buf, namelen);
	}
	req = ktr_getrequest(KTR_NAMEI);
	if (req == NULL) {
		if (buf != NULL)
			free(buf, M_KTRACE);
		return;
	}
	if (namelen > 0) {
		req->ktr_header.ktr_len = namelen;
		req->ktr_buffer = buf;
	}
	ktr_submitrequest(curthread, req);
}

void
ktrsysctl(name, namelen)
	int *name;
	u_int namelen;
{
	struct ktr_request *req;
	u_int mib[CTL_MAXNAME + 2];
	char *mibname;
	size_t mibnamelen;
	int error;

	/* Lookup name of mib. */    
	KASSERT(namelen <= CTL_MAXNAME, ("sysctl MIB too long"));
	mib[0] = 0;
	mib[1] = 1;
	bcopy(name, mib + 2, namelen * sizeof(*name));
	mibnamelen = 128;
	mibname = malloc(mibnamelen, M_KTRACE, M_WAITOK);
	error = kernel_sysctl(curthread, mib, namelen + 2, mibname, &mibnamelen,
	    NULL, 0, &mibnamelen, 0);
	if (error) {
		free(mibname, M_KTRACE);
		return;
	}
	req = ktr_getrequest(KTR_SYSCTL);
	if (req == NULL) {
		free(mibname, M_KTRACE);
		return;
	}
	req->ktr_header.ktr_len = mibnamelen;
	req->ktr_buffer = mibname;
	ktr_submitrequest(curthread, req);
}

void
ktrgenio(fd, rw, uio, error)
	int fd;
	enum uio_rw rw;
	struct uio *uio;
	int error;
{
	struct ktr_request *req;
	struct ktr_genio *ktg;
	int datalen;
	char *buf;

	if (error) {
		free(uio, M_IOV);
		return;
	}
	uio->uio_offset = 0;
	uio->uio_rw = UIO_WRITE;
	datalen = imin(uio->uio_resid, ktr_geniosize);
	buf = malloc(datalen, M_KTRACE, M_WAITOK);
	error = uiomove(buf, datalen, uio);
	free(uio, M_IOV);
	if (error) {
		free(buf, M_KTRACE);
		return;
	}
	req = ktr_getrequest(KTR_GENIO);
	if (req == NULL) {
		free(buf, M_KTRACE);
		return;
	}
	ktg = &req->ktr_data.ktr_genio;
	ktg->ktr_fd = fd;
	ktg->ktr_rw = rw;
	req->ktr_header.ktr_len = datalen;
	req->ktr_buffer = buf;
	ktr_submitrequest(curthread, req);
}

void
ktrpsig(sig, action, mask, code)
	int sig;
	sig_t action;
	sigset_t *mask;
	int code;
{
	struct ktr_request *req;
	struct ktr_psig	*kp;

	req = ktr_getrequest(KTR_PSIG);
	if (req == NULL)
		return;
	kp = &req->ktr_data.ktr_psig;
	kp->signo = (char)sig;
	kp->action = action;
	kp->mask = *mask;
	kp->code = code;
	ktr_enqueuerequest(curthread, req);
}

void
ktrcsw(out, user)
	int out, user;
{
	struct ktr_request *req;
	struct ktr_csw *kc;

	req = ktr_getrequest(KTR_CSW);
	if (req == NULL)
		return;
	kc = &req->ktr_data.ktr_csw;
	kc->out = out;
	kc->user = user;
	ktr_enqueuerequest(curthread, req);
}

void
ktrstruct(name, namelen, data, datalen)
	const char *name;
	size_t namelen;
	void *data;
	size_t datalen;
{
	struct ktr_request *req;
	char *buf = NULL;
	size_t buflen;

	if (!data)
		datalen = 0;
	buflen = namelen + 1 + datalen;
	buf = malloc(buflen, M_KTRACE, M_WAITOK);
	bcopy(name, buf, namelen);
	buf[namelen] = '\0';
	bcopy(data, buf + namelen + 1, datalen);
	if ((req = ktr_getrequest(KTR_STRUCT)) == NULL) {
		free(buf, M_KTRACE);
		return;
	}
	req->ktr_buffer = buf;
	req->ktr_header.ktr_len = buflen;
	ktr_submitrequest(curthread, req);
}
#endif /* KTRACE */

/* Interface and common routines */

#ifndef _SYS_SYSPROTO_H_
struct ktrace_args {
	char	*fname;
	int	ops;
	int	facs;
	int	pid;
};
#endif
/* ARGSUSED */
int
ktrace(td, uap)
	struct thread *td;
	register struct ktrace_args *uap;
{
#ifdef KTRACE
	register struct vnode *vp = NULL;
	register struct proc *p;
	struct pgrp *pg;
	int facs = uap->facs & ~KTRFAC_ROOT;
	int ops = KTROP(uap->ops);
	int descend = uap->ops & KTRFLAG_DESCEND;
	int nfound, ret = 0;
	int flags, error = 0, vfslocked;
	struct nameidata nd;
	struct ucred *cred;

	/*
	 * Need something to (un)trace.
	 */
	if (ops != KTROP_CLEARFILE && facs == 0)
		return (EINVAL);

	ktrace_enter(td);
	if (ops != KTROP_CLEAR) {
		/*
		 * an operation which requires a file argument.
		 */
		NDINIT(&nd, LOOKUP, NOFOLLOW | MPSAFE, UIO_USERSPACE,
		    uap->fname, td);
		flags = FREAD | FWRITE | O_NOFOLLOW;
		error = vn_open(&nd, &flags, 0, NULL);
		if (error) {
			ktrace_exit(td);
			return (error);
		}
		vfslocked = NDHASGIANT(&nd);
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vp = nd.ni_vp;
		VOP_UNLOCK(vp, 0, td);
		if (vp->v_type != VREG) {
			(void) vn_close(vp, FREAD|FWRITE, td->td_ucred, td);
			VFS_UNLOCK_GIANT(vfslocked);
			ktrace_exit(td);
			return (EACCES);
		}
		VFS_UNLOCK_GIANT(vfslocked);
	}
	/*
	 * Clear all uses of the tracefile.
	 */
	if (ops == KTROP_CLEARFILE) {
		int vrele_count;

		vrele_count = 0;
		sx_slock(&allproc_lock);
		FOREACH_PROC_IN_SYSTEM(p) {
			PROC_LOCK(p);
			if (p->p_tracevp == vp) {
				if (ktrcanset(td, p)) {
					mtx_lock(&ktrace_mtx);
					cred = p->p_tracecred;
					p->p_tracecred = NULL;
					p->p_tracevp = NULL;
					p->p_traceflag = 0;
					mtx_unlock(&ktrace_mtx);
					vrele_count++;
					crfree(cred);
				} else
					error = EPERM;
			}
			PROC_UNLOCK(p);
		}
		sx_sunlock(&allproc_lock);
		if (vrele_count > 0) {
			vfslocked = VFS_LOCK_GIANT(vp->v_mount);
			while (vrele_count-- > 0)
				vrele(vp);
			VFS_UNLOCK_GIANT(vfslocked);
		}
		goto done;
	}
	/*
	 * do it
	 */
	sx_slock(&proctree_lock);
	if (uap->pid < 0) {
		/*
		 * by process group
		 */
		pg = pgfind(-uap->pid);
		if (pg == NULL) {
			sx_sunlock(&proctree_lock);
			error = ESRCH;
			goto done;
		}
		/*
		 * ktrops() may call vrele(). Lock pg_members
		 * by the proctree_lock rather than pg_mtx.
		 */
		PGRP_UNLOCK(pg);
		nfound = 0;
		LIST_FOREACH(p, &pg->pg_members, p_pglist) {
			PROC_LOCK(p);
			if (p_cansee(td, p) != 0) {
				PROC_UNLOCK(p); 
				continue;
			}
			PROC_UNLOCK(p); 
			nfound++;
			if (descend)
				ret |= ktrsetchildren(td, p, ops, facs, vp);
			else
				ret |= ktrops(td, p, ops, facs, vp);
		}
		if (nfound == 0) {
			sx_sunlock(&proctree_lock);
			error = ESRCH;
			goto done;
		}
	} else {
		/*
		 * by pid
		 */
		p = pfind(uap->pid);
		if (p == NULL) {
			sx_sunlock(&proctree_lock);
			error = ESRCH;
			goto done;
		}
		error = p_cansee(td, p);
		/*
		 * The slock of the proctree lock will keep this process
		 * from going away, so unlocking the proc here is ok.
		 */
		PROC_UNLOCK(p);
		if (error) {
			sx_sunlock(&proctree_lock);
			goto done;
		}
		if (descend)
			ret |= ktrsetchildren(td, p, ops, facs, vp);
		else
			ret |= ktrops(td, p, ops, facs, vp);
	}
	sx_sunlock(&proctree_lock);
	if (!ret)
		error = EPERM;
done:
	if (vp != NULL) {
		vfslocked = VFS_LOCK_GIANT(vp->v_mount);
		(void) vn_close(vp, FWRITE, td->td_ucred, td);
		VFS_UNLOCK_GIANT(vfslocked);
	}
	ktrace_exit(td);
	return (error);
#else /* !KTRACE */
	return (ENOSYS);
#endif /* KTRACE */
}

/* ARGSUSED */
int
utrace(td, uap)
	struct thread *td;
	register struct utrace_args *uap;
{

#ifdef KTRACE
	struct ktr_request *req;
	void *cp;
	int error;

	if (!KTRPOINT(td, KTR_USER))
		return (0);
	if (uap->len > KTR_USER_MAXLEN)
		return (EINVAL);
	cp = malloc(uap->len, M_KTRACE, M_WAITOK);
	error = copyin(uap->addr, cp, uap->len);
	if (error) {
		free(cp, M_KTRACE);
		return (error);
	}
	req = ktr_getrequest(KTR_USER);
	if (req == NULL) {
		free(cp, M_KTRACE);
		return (ENOMEM);
	}
	req->ktr_buffer = cp;
	req->ktr_header.ktr_len = uap->len;
	ktr_submitrequest(td, req);
	return (0);
#else /* !KTRACE */
	return (ENOSYS);
#endif /* KTRACE */
}

#ifdef KTRACE
static int
ktrops(td, p, ops, facs, vp)
	struct thread *td;
	struct proc *p;
	int ops, facs;
	struct vnode *vp;
{
	struct vnode *tracevp = NULL;
	struct ucred *tracecred = NULL;

	PROC_LOCK(p);
	if (!ktrcanset(td, p)) {
		PROC_UNLOCK(p);
		return (0);
	}
	mtx_lock(&ktrace_mtx);
	if (ops == KTROP_SET) {
		if (p->p_tracevp != vp) {
			/*
			 * if trace file already in use, relinquish below
			 */
			tracevp = p->p_tracevp;
			VREF(vp);
			p->p_tracevp = vp;
		}
		if (p->p_tracecred != td->td_ucred) {
			tracecred = p->p_tracecred;
			p->p_tracecred = crhold(td->td_ucred);
		}
		p->p_traceflag |= facs;
		if (priv_check(td, PRIV_KTRACE) == 0)
			p->p_traceflag |= KTRFAC_ROOT;
	} else {
		/* KTROP_CLEAR */
		if (((p->p_traceflag &= ~facs) & KTRFAC_MASK) == 0) {
			/* no more tracing */
			p->p_traceflag = 0;
			tracevp = p->p_tracevp;
			p->p_tracevp = NULL;
			tracecred = p->p_tracecred;
			p->p_tracecred = NULL;
		}
	}
	mtx_unlock(&ktrace_mtx);
	PROC_UNLOCK(p);
	if (tracevp != NULL) {
		int vfslocked;

		vfslocked = VFS_LOCK_GIANT(tracevp->v_mount);
		vrele(tracevp);
		VFS_UNLOCK_GIANT(vfslocked);
	}
	if (tracecred != NULL)
		crfree(tracecred);

	return (1);
}

static int
ktrsetchildren(td, top, ops, facs, vp)
	struct thread *td;
	struct proc *top;
	int ops, facs;
	struct vnode *vp;
{
	register struct proc *p;
	register int ret = 0;

	p = top;
	sx_assert(&proctree_lock, SX_LOCKED);
	for (;;) {
		ret |= ktrops(td, p, ops, facs, vp);
		/*
		 * If this process has children, descend to them next,
		 * otherwise do any siblings, and if done with this level,
		 * follow back up the tree (but not past top).
		 */
		if (!LIST_EMPTY(&p->p_children))
			p = LIST_FIRST(&p->p_children);
		else for (;;) {
			if (p == top)
				return (ret);
			if (LIST_NEXT(p, p_sibling)) {
				p = LIST_NEXT(p, p_sibling);
				break;
			}
			p = p->p_pptr;
		}
	}
	/*NOTREACHED*/
}

static void
ktr_writerequest(struct thread *td, struct ktr_request *req)
{
	struct ktr_header *kth;
	struct vnode *vp;
	struct proc *p;
	struct ucred *cred;
	struct uio auio;
	struct iovec aiov[3];
	struct mount *mp;
	int datalen, buflen, vrele_count;
	int error, vfslocked;

	/*
	 * We hold the vnode and credential for use in I/O in case ktrace is
	 * disabled on the process as we write out the request.
	 *
	 * XXXRW: This is not ideal: we could end up performing a write after
	 * the vnode has been closed.
	 */
	mtx_lock(&ktrace_mtx);
	vp = td->td_proc->p_tracevp;
	cred = td->td_proc->p_tracecred;

	/*
	 * If vp is NULL, the vp has been cleared out from under this
	 * request, so just drop it.  Make sure the credential and vnode are
	 * in sync: we should have both or neither.
	 */
	if (vp == NULL) {
		KASSERT(cred == NULL, ("ktr_writerequest: cred != NULL"));
		mtx_unlock(&ktrace_mtx);
		return;
	}
	VREF(vp);
	KASSERT(cred != NULL, ("ktr_writerequest: cred == NULL"));
	crhold(cred);
	mtx_unlock(&ktrace_mtx);

	kth = &req->ktr_header;
	KASSERT(((u_short)kth->ktr_type & ~KTR_DROP) <
	    sizeof(data_lengths) / sizeof(data_lengths[0]),
	    ("data_lengths array overflow"));
	datalen = data_lengths[(u_short)kth->ktr_type & ~KTR_DROP];
	buflen = kth->ktr_len;
	auio.uio_iov = &aiov[0];
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	aiov[0].iov_base = (caddr_t)kth;
	aiov[0].iov_len = sizeof(struct ktr_header);
	auio.uio_resid = sizeof(struct ktr_header);
	auio.uio_iovcnt = 1;
	auio.uio_td = td;
	if (datalen != 0) {
		aiov[1].iov_base = (caddr_t)&req->ktr_data;
		aiov[1].iov_len = datalen;
		auio.uio_resid += datalen;
		auio.uio_iovcnt++;
		kth->ktr_len += datalen;
	}
	if (buflen != 0) {
		KASSERT(req->ktr_buffer != NULL, ("ktrace: nothing to write"));
		aiov[auio.uio_iovcnt].iov_base = req->ktr_buffer;
		aiov[auio.uio_iovcnt].iov_len = buflen;
		auio.uio_resid += buflen;
		auio.uio_iovcnt++;
	}

	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	vn_start_write(vp, &mp, V_WAIT);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	(void)VOP_LEASE(vp, td, cred, LEASE_WRITE);
#ifdef MAC
	error = mac_check_vnode_write(cred, NOCRED, vp);
	if (error == 0)
#endif
		error = VOP_WRITE(vp, &auio, IO_UNIT | IO_APPEND, cred);
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
	crfree(cred);
	if (!error) {
		vrele(vp);
		VFS_UNLOCK_GIANT(vfslocked);
		return;
	}
	VFS_UNLOCK_GIANT(vfslocked);

	/*
	 * If error encountered, give up tracing on this vnode.  We defer
	 * all the vrele()'s on the vnode until after we are finished walking
	 * the various lists to avoid needlessly holding locks.
	 * NB: at this point we still hold the vnode reference that must
	 * not go away as we need the valid vnode to compare with. Thus let
	 * vrele_count start at 1 and the reference will be freed
	 * by the loop at the end after our last use of vp.
	 */
	log(LOG_NOTICE, "ktrace write failed, errno %d, tracing stopped\n",
	    error);
	vrele_count = 1;
	/*
	 * First, clear this vnode from being used by any processes in the
	 * system.
	 * XXX - If one process gets an EPERM writing to the vnode, should
	 * we really do this?  Other processes might have suitable
	 * credentials for the operation.
	 */
	cred = NULL;
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		PROC_LOCK(p);
		if (p->p_tracevp == vp) {
			mtx_lock(&ktrace_mtx);
			p->p_tracevp = NULL;
			p->p_traceflag = 0;
			cred = p->p_tracecred;
			p->p_tracecred = NULL;
			mtx_unlock(&ktrace_mtx);
			vrele_count++;
		}
		PROC_UNLOCK(p);
		if (cred != NULL) {
			crfree(cred);
			cred = NULL;
		}
	}
	sx_sunlock(&allproc_lock);

	/*
	 * We can't clear any pending requests in threads that have cached
	 * them but not yet committed them, as those are per-thread.  The
	 * thread will have to clear it itself on system call return.
	 */
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	while (vrele_count-- > 0)
		vrele(vp);
	VFS_UNLOCK_GIANT(vfslocked);
}

/*
 * Return true if caller has permission to set the ktracing state
 * of target.  Essentially, the target can't possess any
 * more permissions than the caller.  KTRFAC_ROOT signifies that
 * root previously set the tracing status on the target process, and
 * so, only root may further change it.
 */
static int
ktrcanset(td, targetp)
	struct thread *td;
	struct proc *targetp;
{

	PROC_LOCK_ASSERT(targetp, MA_OWNED);
	if (targetp->p_traceflag & KTRFAC_ROOT &&
	    priv_check(td, PRIV_KTRACE))
		return (0);

	if (p_candebug(td, targetp) != 0)
		return (0);

	return (1);
}

#endif /* KTRACE */
