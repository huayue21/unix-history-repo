/*
 * Copyright (c) 1989 The Regents of the University of California.
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
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)vfs_syscalls.c	7.25 (Berkeley) %G%
 */

#include "param.h"
#include "systm.h"
#include "syscontext.h"
#include "kernel.h"
#include "file.h"
#include "stat.h"
#include "vnode.h"
#include "../ufs/inode.h"
#include "mount.h"
#include "proc.h"
#include "uio.h"
#include "malloc.h"

/*
 * Virtual File System System Calls
 */

/*
 * mount system call
 */
mount(scp)
	register struct syscontext *scp;
{
	register struct a {
		int	type;
		char	*dir;
		int	flags;
		caddr_t	data;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	register struct vnode *vp;
	register struct mount *mp;
	int error;

	/*
	 * Must be super user
	 */
	if (error = suser(scp->sc_cred, &scp->sc_acflag))
		RETURN (error);
	/*
	 * Get vnode to be covered
	 */
	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->dir;
	if (error = namei(ndp))
		RETURN (error);
	vp = ndp->ni_vp;
	if (uap->flags & M_UPDATE) {
		if ((vp->v_flag & VROOT) == 0) {
			vput(vp);
			RETURN (EINVAL);
		}
		mp = vp->v_mount;
		/*
		 * We allow going from read-only to read-write,
		 * but not from read-write to read-only.
		 */
		if ((mp->m_flag & M_RDONLY) == 0 &&
		    (uap->flags & M_RDONLY) != 0) {
			vput(vp);
			RETURN (EOPNOTSUPP);	/* Needs translation */
		}
		mp->m_flag |= M_UPDATE;
		VOP_UNLOCK(vp);
		goto update;
	}
	if (vp->v_count != 1) {
		vput(vp);
		RETURN (EBUSY);
	}
	if (vp->v_type != VDIR) {
		vput(vp);
		RETURN (ENOTDIR);
	}
	if (uap->type > MOUNT_MAXTYPE ||
	    vfssw[uap->type] == (struct vfsops *)0) {
		vput(vp);
		RETURN (ENODEV);
	}

	/*
	 * Allocate and initialize the file system.
	 */
	mp = (struct mount *)malloc((u_long)sizeof(struct mount),
		M_MOUNT, M_WAITOK);
	mp->m_op = vfssw[uap->type];
	mp->m_flag = 0;
	mp->m_exroot = 0;
	mp->m_mounth = (struct vnode *)0;
	if (error = vfs_lock(mp)) {
		free((caddr_t)mp, M_MOUNT);
		vput(vp);
		RETURN (error);
	}
	if (vp->v_mountedhere != (struct mount *)0) {
		vfs_unlock(mp);
		free((caddr_t)mp, M_MOUNT);
		vput(vp);
		RETURN (EBUSY);
	}
	/*
	 * Put the new filesystem on the mount list after root.
	 */
	mp->m_next = rootfs->m_next;
	mp->m_prev = rootfs;
	rootfs->m_next = mp;
	mp->m_next->m_prev = mp;
	vp->v_mountedhere = mp;
	mp->m_vnodecovered = vp;
update:
	/*
	 * Set the mount level flags.
	 */
	if (uap->flags & M_RDONLY)
		mp->m_flag |= M_RDONLY;
	else
		mp->m_flag &= ~M_RDONLY;
	if (uap->flags & M_NOSUID)
		mp->m_flag |= M_NOSUID;
	else
		mp->m_flag &= ~M_NOSUID;
	if (uap->flags & M_NOEXEC)
		mp->m_flag |= M_NOEXEC;
	else
		mp->m_flag &= ~M_NOEXEC;
	if (uap->flags & M_NODEV)
		mp->m_flag |= M_NODEV;
	else
		mp->m_flag &= ~M_NODEV;
	if (uap->flags & M_SYNCHRONOUS)
		mp->m_flag |= M_SYNCHRONOUS;
	else
		mp->m_flag &= ~M_SYNCHRONOUS;
	/*
	 * Mount the filesystem.
	 */
	error = VFS_MOUNT(mp, uap->dir, uap->data, ndp);
	if (mp->m_flag & M_UPDATE) {
		mp->m_flag &= ~M_UPDATE;
		vrele(vp);
		RETURN (error);
	}
	cache_purge(vp);
	if (!error) {
		VOP_UNLOCK(vp);
		vfs_unlock(mp);
		error = VFS_START(mp, 0);
	} else {
		vfs_remove(mp);
		free((caddr_t)mp, M_MOUNT);
		vput(vp);
	}
	RETURN (error);
}

/*
 * Unmount system call.
 *
 * Note: unmount takes a path to the vnode mounted on as argument,
 * not special file (as before).
 */
unmount(scp)
	register struct syscontext *scp;
{
	struct a {
		char	*pathp;
		int	flags;
	} *uap = (struct a *)scp->sc_ap;
	register struct vnode *vp;
	register struct nameidata *ndp = &scp->sc_nd;
	struct mount *mp;
	int error;

	/*
	 * Must be super user
	 */
	if (error = suser(scp->sc_cred, &scp->sc_acflag))
		RETURN (error);

	ndp->ni_nameiop = LOOKUP | LOCKLEAF | FOLLOW;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->pathp;
	if (error = namei(ndp))
		RETURN (error);
	vp = ndp->ni_vp;
	/*
	 * Must be the root of the filesystem
	 */
	if ((vp->v_flag & VROOT) == 0) {
		vput(vp);
		RETURN (EINVAL);
	}
	mp = vp->v_mount;
	vput(vp);
	RETURN (dounmount(mp, uap->flags));
}

/*
 * Do an unmount.
 */
dounmount(mp, flags)
	register struct mount *mp;
	int flags;
{
	struct vnode *coveredvp;
	int error;

	coveredvp = mp->m_vnodecovered;
	if (error = vfs_lock(mp))
		return (error);

	xumount(mp);		/* remove unused sticky files from text table */
	cache_purgevfs(mp);	/* remove cache entries for this file sys */
	VFS_SYNC(mp, MNT_WAIT);

	error = VFS_UNMOUNT(mp, flags);
	if (error) {
		vfs_unlock(mp);
	} else {
		vrele(coveredvp);
		vfs_remove(mp);
		free((caddr_t)mp, M_MOUNT);
	}
	return (error);
}

/*
 * Sync system call.
 * Sync each mounted filesystem.
 */
/* ARGSUSED */
sync(scp)
	struct syscontext *scp;
{
	register struct mount *mp;

	mp = rootfs;
	do {
		if ((mp->m_flag & M_RDONLY) == 0)
			VFS_SYNC(mp, MNT_NOWAIT);
		mp = mp->m_next;
	} while (mp != rootfs);
}

/*
 * get filesystem statistics
 */
statfs(scp)
	register struct syscontext *scp;
{
	struct a {
		char *path;
		struct statfs *buf;
	} *uap = (struct a *)scp->sc_ap;
	register struct vnode *vp;
	register struct mount *mp;
	register struct nameidata *ndp = &scp->sc_nd;
	struct statfs sb;
	int error;

	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->path;
	if (error = namei(ndp))
		RETURN (error);
	vp = ndp->ni_vp;
	mp = vp->v_mount;
	if (error = VFS_STATFS(mp, &sb))
		goto out;
	sb.f_flags = mp->m_flag & M_VISFLAGMASK;
	sb.f_fsid = mp->m_fsid;
	error = copyout((caddr_t)&sb, (caddr_t)uap->buf, sizeof(sb));
out:
	vput(vp);
	RETURN (error);
}

fstatfs(scp)
	register struct syscontext *scp;
{
	struct a {
		int fd;
		struct statfs *buf;
	} *uap = (struct a *)scp->sc_ap;
	struct file *fp;
	struct mount *mp;
	struct statfs sb;
	int error;

	if (error = getvnode(scp->sc_ofile, uap->fd, &fp))
		RETURN (error);
	mp = ((struct vnode *)fp->f_data)->v_mount;
	if (error = VFS_STATFS(mp, &sb))
		RETURN (error);
	sb.f_flags = mp->m_flag & M_VISFLAGMASK;
	sb.f_fsid = mp->m_fsid;
	RETURN (copyout((caddr_t)&sb, (caddr_t)uap->buf, sizeof(sb)));
}

/*
 * get statistics on all filesystems
 */
getfsstat(scp)
	register struct syscontext *scp;
{
	struct a {
		struct statfs *buf;
		long bufsize;
	} *uap = (struct a *)scp->sc_ap;
	register struct mount *mp;
	register struct statfs *sfsp;
	long count, maxcount, error;

	maxcount = uap->bufsize / sizeof(struct statfs);
	sfsp = uap->buf;
	mp = rootfs;
	count = 0;
	do {
		count++;
		if (sfsp && count <= maxcount &&
		    ((mp->m_flag & M_MLOCK) == 0)) {
			if (error = VFS_STATFS(mp, sfsp))
				RETURN (error);
			sfsp->f_flags = mp->m_flag & M_VISFLAGMASK;
			sfsp->f_fsid = mp->m_fsid;
			sfsp++;
		}
		mp = mp->m_prev;
	} while (mp != rootfs);
	if (sfsp && count > maxcount)
		scp->sc_retval1 = maxcount;
	else
		scp->sc_retval1 = count;
	RETURN (0);
}

/*
 * Change current working directory to a given file descriptor.
 */
fchdir(scp)
	register struct syscontext *scp;
{
	struct a {
		int	fd;
	} *uap = (struct a *)scp->sc_ap;
	register struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(scp->sc_ofile, uap->fd, &fp))
		RETURN (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LOCK(vp);
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, scp->sc_cred);
	VOP_UNLOCK(vp);
	vrele(scp->sc_cdir);
	scp->sc_cdir = vp;
	RETURN (error);
}

/*
 * Change current working directory (``.'').
 */
chdir(scp)
	register struct syscontext *scp;
{
	struct a {
		char	*fname;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	int error;

	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = chdirec(ndp))
		RETURN (error);
	vrele(scp->sc_cdir);
	scp->sc_cdir = ndp->ni_vp;
	RETURN (0);
}

/*
 * Change notion of root (``/'') directory.
 */
chroot(scp)
	register struct syscontext *scp;
{
	struct a {
		char	*fname;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	int error;

	if (error = suser(scp->sc_cred, &scp->sc_acflag))
		RETURN (error);
	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = chdirec(ndp))
		RETURN (error);
	if (scp->sc_rdir != NULL)
		vrele(scp->sc_rdir);
	scp->sc_rdir = ndp->ni_vp;
	RETURN (0);
}

/*
 * Common routine for chroot and chdir.
 */
chdirec(ndp)
	register struct nameidata *ndp;
{
	struct vnode *vp;
	int error;

	if (error = namei(ndp))
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, ndp->ni_cred);
	VOP_UNLOCK(vp);
	if (error)
		vrele(vp);
	return (error);
}

/*
 * Open system call.
 */
open(scp)
	register struct syscontext *scp;
{
	struct a {
		char	*fname;
		int	mode;
		int	crtmode;
	} *uap = (struct a *) scp->sc_ap;
	struct nameidata *ndp = &scp->sc_nd;

	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	RETURN (copen(scp, uap->mode-FOPEN, uap->crtmode &~ scp->sc_cmask, ndp,
		&scp->sc_retval1));
}

/*
 * Creat system call.
 */
creat(scp)
	register struct syscontext *scp;
{
	struct a {
		char	*fname;
		int	fmode;
	} *uap = (struct a *)scp->sc_ap;
	struct nameidata *ndp = &scp->sc_nd;

	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	RETURN (copen(scp, FWRITE|FCREAT|FTRUNC, uap->fmode &~ scp->sc_cmask,
		ndp, &scp->sc_retval1));
}

/*
 * Common code for open and creat.
 * Check permissions, allocate an open file structure,
 * and call the device open routine if any.
 */
copen(scp, fmode, cmode, ndp, resultfd)
	register struct syscontext *scp;
	int fmode, cmode;
	struct nameidata *ndp;
	int *resultfd;
{
	register struct file *fp;
	struct file *nfp;
	int indx, error;
	extern struct fileops vnops;

	if (error = falloc(&nfp, &indx))
		return (error);
	fp = nfp;
	scp->sc_retval1 = indx;	/* XXX for fdopen() */
	if (error = vn_open(ndp, fmode, (cmode & 07777) &~ ISVTX)) {
		crfree(fp->f_cred);
		fp->f_count--;
		if (error == -1)	/* XXX from fdopen */
			return (0);	/* XXX from fdopen */
		scp->sc_ofile[indx] = NULL;
		return (error);
	}
	fp->f_flag = fmode & FMASK;
	fp->f_type = DTYPE_VNODE;
	fp->f_ops = &vnops;
	fp->f_data = (caddr_t)ndp->ni_vp;
	if (resultfd)
		*resultfd = indx;
	return (0);
}

/*
 * Mknod system call
 */
mknod(scp)
	register struct syscontext *scp;
{
	register struct a {
		char	*fname;
		int	fmode;
		int	dev;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	register struct vnode *vp;
	struct vattr vattr;
	int error;

	if (error = suser(scp->sc_cred, &scp->sc_acflag))
		RETURN (error);
	ndp->ni_nameiop = CREATE | LOCKPARENT;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp))
		RETURN (error);
	vp = ndp->ni_vp;
	if (vp != NULL) {
		error = EEXIST;
		goto out;
	}
	vattr_null(&vattr);
	switch (uap->fmode & IFMT) {

	case IFMT:	/* used by badsect to flag bad sectors */
		vattr.va_type = VBAD;
		break;
	case IFCHR:
		vattr.va_type = VCHR;
		break;
	case IFBLK:
		vattr.va_type = VBLK;
		break;
	default:
		error = EINVAL;
		goto out;
	}
	vattr.va_mode = (uap->fmode & 07777) &~ scp->sc_cmask;
	vattr.va_rdev = uap->dev;
out:
	if (error)
		VOP_ABORTOP(ndp);
	else
		error = VOP_MKNOD(ndp, &vattr, ndp->ni_cred);
	RETURN (error);
}

/*
 * link system call
 */
link(scp)
	register struct syscontext *scp;
{
	register struct a {
		char	*target;
		char	*linkname;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	register struct vnode *vp, *xp;
	int error;

	ndp->ni_nameiop = LOOKUP | FOLLOW;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->target;
	if (error = namei(ndp))
		RETURN (error);
	vp = ndp->ni_vp;
	if (vp->v_type == VDIR &&
	    (error = suser(scp->sc_cred, &scp->sc_acflag)))
		goto out1;
	ndp->ni_nameiop = CREATE | LOCKPARENT;
	ndp->ni_dirp = (caddr_t)uap->linkname;
	if (error = namei(ndp))
		goto out1;
	xp = ndp->ni_vp;
	if (xp != NULL) {
		error = EEXIST;
		goto out;
	}
	xp = ndp->ni_dvp;
	if (vp->v_mount != xp->v_mount)
		error = EXDEV;
out:
	if (error)
		VOP_ABORTOP(ndp);
	else
		error = VOP_LINK(vp, ndp);
out1:
	vrele(vp);
	RETURN (error);
}

/*
 * symlink -- make a symbolic link
 */
symlink(scp)
	register struct syscontext *scp;
{
	struct a {
		char	*target;
		char	*linkname;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	register struct vnode *vp;
	struct vattr vattr;
	char *target;
	int error;

	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->linkname;
	MALLOC(target, char *, MAXPATHLEN, M_NAMEI, M_WAITOK);
	if (error = copyinstr(uap->target, target, MAXPATHLEN, (u_int *)0))
		goto out1;
	ndp->ni_nameiop = CREATE | LOCKPARENT;
	if (error = namei(ndp))
		goto out1;
	vp = ndp->ni_vp;
	if (vp) {
		error = EEXIST;
		goto out;
	}
	vp = ndp->ni_dvp;
	vattr_null(&vattr);
	vattr.va_mode = 0777 &~ scp->sc_cmask;
out:
	if (error)
		VOP_ABORTOP(ndp);
	else
		error = VOP_SYMLINK(ndp, &vattr, target);
out1:
	FREE(target, M_NAMEI);
	RETURN (error);
}

/*
 * Unlink system call.
 * Hard to avoid races here, especially
 * in unlinking directories.
 */
unlink(scp)
	register struct syscontext *scp;
{
	struct a {
		char	*fname;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	register struct vnode *vp;
	int error;

	ndp->ni_nameiop = DELETE | LOCKPARENT | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp))
		RETURN (error);
	vp = ndp->ni_vp;
	if (vp->v_type == VDIR &&
	    (error = suser(scp->sc_cred, &scp->sc_acflag)))
		goto out;
	/*
	 * Don't unlink a mounted file.
	 */
	if (vp->v_flag & VROOT) {
		error = EBUSY;
		goto out;
	}
	if (vp->v_flag & VTEXT)
		xrele(vp);	/* try once to free text */
out:
	if (error)
		VOP_ABORTOP(ndp);
	else
		error = VOP_REMOVE(ndp);
	RETURN (error);
}

/*
 * Seek system call
 */
lseek(scp)
	register struct syscontext *scp;
{
	register struct file *fp;
	register struct a {
		int	fdes;
		off_t	off;
		int	sbase;
	} *uap = (struct a *)scp->sc_ap;
	struct vattr vattr;
	int error;

	if ((unsigned)uap->fdes >= NOFILE ||
	    (fp = scp->sc_ofile[uap->fdes]) == NULL)
		RETURN (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		RETURN (ESPIPE);
	switch (uap->sbase) {

	case L_INCR:
		fp->f_offset += uap->off;
		break;

	case L_XTND:
		if (error = VOP_GETATTR((struct vnode *)fp->f_data,
		    &vattr, scp->sc_cred))
			RETURN (error);
		fp->f_offset = uap->off + vattr.va_size;
		break;

	case L_SET:
		fp->f_offset = uap->off;
		break;

	default:
		RETURN (EINVAL);
	}
	scp->sc_offset = fp->f_offset;
	RETURN (0);
}

/*
 * Access system call
 */
saccess(scp)
	register struct syscontext *scp;
{
	register struct a {
		char	*fname;
		int	fmode;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	register struct vnode *vp;
	int error, mode, svuid, svgid;

	svuid = scp->sc_uid;
	svgid = scp->sc_gid;
	scp->sc_uid = scp->sc_ruid;
	scp->sc_gid = scp->sc_rgid;
	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp))
		goto out1;
	vp = ndp->ni_vp;
	/*
	 * fmode == 0 means only check for exist
	 */
	if (uap->fmode) {
		mode = 0;
		if (uap->fmode & R_OK)
			mode |= VREAD;
		if (uap->fmode & W_OK)
			mode |= VWRITE;
		if (uap->fmode & X_OK)
			mode |= VEXEC;
		if ((mode & VWRITE) == 0 || (error = vn_writechk(vp)) == 0)
			error = VOP_ACCESS(vp, mode, ndp->ni_cred);
	}
	vput(vp);
out1:
	scp->sc_uid = svuid;
	scp->sc_gid = svgid;
	RETURN (error);
}

/*
 * Stat system call.  This version follows links.
 */
stat(scp)
	struct syscontext *scp;
{

	stat1(scp, FOLLOW);
}

/*
 * Lstat system call.  This version does not follow links.
 */
lstat(scp)
	struct syscontext *scp;
{

	stat1(scp, NOFOLLOW);
}

stat1(scp, follow)
	register struct syscontext *scp;
	int follow;
{
	register struct a {
		char	*fname;
		struct stat *ub;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	struct stat sb;
	int error;

	ndp->ni_nameiop = LOOKUP | LOCKLEAF | follow;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp))
		RETURN (error);
	error = vn_stat(ndp->ni_vp, &sb);
	vput(ndp->ni_vp);
	if (error)
		RETURN (error);
	error = copyout((caddr_t)&sb, (caddr_t)uap->ub, sizeof (sb));
	RETURN (error);
}

/*
 * Return target name of a symbolic link
 */
readlink(scp)
	register struct syscontext *scp;
{
	register struct a {
		char	*name;
		char	*buf;
		int	count;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	register struct vnode *vp;
	struct iovec aiov;
	struct uio auio;
	int error;

	ndp->ni_nameiop = LOOKUP | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->name;
	if (error = namei(ndp))
		RETURN (error);
	vp = ndp->ni_vp;
	if (vp->v_type != VLNK) {
		error = EINVAL;
		goto out;
	}
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_resid = uap->count;
	error = VOP_READLINK(vp, &auio, ndp->ni_cred);
out:
	vput(vp);
	scp->sc_retval1 = uap->count - auio.uio_resid;
	RETURN (error);
}

/*
 * Change flags of a file given path name.
 */
chflags(scp)
	register struct syscontext *scp;
{
	struct a {
		char	*fname;
		int	flags;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	register struct vnode *vp;
	struct vattr vattr;
	int error;

	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	vattr_null(&vattr);
	vattr.va_flags = uap->flags;
	if (error = namei(ndp))
		RETURN (error);
	vp = ndp->ni_vp;
	if (vp->v_mount->m_flag & M_RDONLY) {
		error = EROFS;
		goto out;
	}
	error = VOP_SETATTR(vp, &vattr, ndp->ni_cred);
out:
	vput(vp);
	RETURN (error);
}

/*
 * Change flags of a file given a file descriptor.
 */
fchflags(scp)
	register struct syscontext *scp;
{
	struct a {
		int	fd;
		int	flags;
	} *uap = (struct a *)scp->sc_ap;
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(scp->sc_ofile, uap->fd, &fp))
		RETURN (error);
	vattr_null(&vattr);
	vattr.va_flags = uap->flags;
	vp = (struct vnode *)fp->f_data;
	VOP_LOCK(vp);
	if (vp->v_mount->m_flag & M_RDONLY) {
		error = EROFS;
		goto out;
	}
	error = VOP_SETATTR(vp, &vattr, fp->f_cred);
out:
	VOP_UNLOCK(vp);
	RETURN (error);
}

/*
 * Change mode of a file given path name.
 */
chmod(scp)
	register struct syscontext *scp;
{
	struct a {
		char	*fname;
		int	fmode;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	register struct vnode *vp;
	struct vattr vattr;
	int error;

	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	vattr_null(&vattr);
	vattr.va_mode = uap->fmode & 07777;
	if (error = namei(ndp))
		RETURN (error);
	vp = ndp->ni_vp;
	if (vp->v_mount->m_flag & M_RDONLY) {
		error = EROFS;
		goto out;
	}
	error = VOP_SETATTR(vp, &vattr, ndp->ni_cred);
out:
	vput(vp);
	RETURN (error);
}

/*
 * Change mode of a file given a file descriptor.
 */
fchmod(scp)
	register struct syscontext *scp;
{
	struct a {
		int	fd;
		int	fmode;
	} *uap = (struct a *)scp->sc_ap;
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(scp->sc_ofile, uap->fd, &fp))
		RETURN (error);
	vattr_null(&vattr);
	vattr.va_mode = uap->fmode & 07777;
	vp = (struct vnode *)fp->f_data;
	VOP_LOCK(vp);
	if (vp->v_mount->m_flag & M_RDONLY) {
		error = EROFS;
		goto out;
	}
	error = VOP_SETATTR(vp, &vattr, fp->f_cred);
out:
	VOP_UNLOCK(vp);
	RETURN (error);
}

/*
 * Set ownership given a path name.
 */
chown(scp)
	register struct syscontext *scp;
{
	struct a {
		char	*fname;
		int	uid;
		int	gid;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	register struct vnode *vp;
	struct vattr vattr;
	int error;

	ndp->ni_nameiop = LOOKUP | NOFOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	vattr_null(&vattr);
	vattr.va_uid = uap->uid;
	vattr.va_gid = uap->gid;
	if (error = namei(ndp))
		RETURN (error);
	vp = ndp->ni_vp;
	if (vp->v_mount->m_flag & M_RDONLY) {
		error = EROFS;
		goto out;
	}
	error = VOP_SETATTR(vp, &vattr, ndp->ni_cred);
out:
	vput(vp);
	RETURN (error);
}

/*
 * Set ownership given a file descriptor.
 */
fchown(scp)
	register struct syscontext *scp;
{
	struct a {
		int	fd;
		int	uid;
		int	gid;
	} *uap = (struct a *)scp->sc_ap;
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(scp->sc_ofile, uap->fd, &fp))
		RETURN (error);
	vattr_null(&vattr);
	vattr.va_uid = uap->uid;
	vattr.va_gid = uap->gid;
	vp = (struct vnode *)fp->f_data;
	VOP_LOCK(vp);
	if (vp->v_mount->m_flag & M_RDONLY) {
		error = EROFS;
		goto out;
	}
	error = VOP_SETATTR(vp, &vattr, fp->f_cred);
out:
	VOP_UNLOCK(vp);
	RETURN (error);
}

utimes(scp)
	register struct syscontext *scp;
{
	register struct a {
		char	*fname;
		struct	timeval *tptr;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	register struct vnode *vp;
	struct timeval tv[2];
	struct vattr vattr;
	int error;

	if (error = copyin((caddr_t)uap->tptr, (caddr_t)tv, sizeof (tv)))
		RETURN (error);
	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	vattr_null(&vattr);
	vattr.va_atime = tv[0];
	vattr.va_mtime = tv[1];
	if (error = namei(ndp))
		RETURN (error);
	vp = ndp->ni_vp;
	if (vp->v_mount->m_flag & M_RDONLY) {
		error = EROFS;
		goto out;
	}
	error = VOP_SETATTR(vp, &vattr, ndp->ni_cred);
out:
	vput(vp);
	RETURN (error);
}

/*
 * Truncate a file given its path name.
 */
truncate(scp)
	register struct syscontext *scp;
{
	struct a {
		char	*fname;
		off_t	length;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	register struct vnode *vp;
	struct vattr vattr;
	int error;

	ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	vattr_null(&vattr);
	vattr.va_size = uap->length;
	if (error = namei(ndp))
		RETURN (error);
	vp = ndp->ni_vp;
	if (vp->v_type == VDIR) {
		error = EISDIR;
		goto out;
	}
	if ((error = vn_writechk(vp)) ||
	    (error = VOP_ACCESS(vp, VWRITE, ndp->ni_cred)))
		goto out;
	error = VOP_SETATTR(vp, &vattr, ndp->ni_cred);
out:
	vput(vp);
	RETURN (error);
}

/*
 * Truncate a file given a file descriptor.
 */
ftruncate(scp)
	register struct syscontext *scp;
{
	struct a {
		int	fd;
		off_t	length;
	} *uap = (struct a *)scp->sc_ap;
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(scp->sc_ofile, uap->fd, &fp))
		RETURN (error);
	if ((fp->f_flag & FWRITE) == 0)
		RETURN (EINVAL);
	vattr_null(&vattr);
	vattr.va_size = uap->length;
	vp = (struct vnode *)fp->f_data;
	VOP_LOCK(vp);
	if (vp->v_type == VDIR) {
		error = EISDIR;
		goto out;
	}
	if (error = vn_writechk(vp))
		goto out;
	error = VOP_SETATTR(vp, &vattr, fp->f_cred);
out:
	VOP_UNLOCK(vp);
	RETURN (error);
}

/*
 * Synch an open file.
 */
fsync(scp)
	register struct syscontext *scp;
{
	struct a {
		int	fd;
	} *uap = (struct a *)scp->sc_ap;
	struct file *fp;
	int error;

	if (error = getvnode(scp->sc_ofile, uap->fd, &fp))
		RETURN (error);
	error = VOP_FSYNC((struct vnode *)fp->f_data, fp->f_flag, fp->f_cred);
	RETURN (error);
}

/*
 * Rename system call.
 *
 * Source and destination must either both be directories, or both
 * not be directories.  If target is a directory, it must be empty.
 */
rename(scp)
	register struct syscontext *scp;
{
	struct a {
		char	*from;
		char	*to;
	} *uap = (struct a *)scp->sc_ap;
	register struct vnode *tvp, *fvp, *tdvp;
	register struct nameidata *ndp = &scp->sc_nd;
	struct nameidata tond;
	int error;

	ndp->ni_nameiop = DELETE | WANTPARENT;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->from;
	if (error = namei(ndp))
		RETURN (error);
	fvp = ndp->ni_vp;
	nddup(ndp, &tond);
	tond.ni_nameiop = RENAME | LOCKPARENT | LOCKLEAF | NOCACHE;
	tond.ni_segflg = UIO_USERSPACE;
	tond.ni_dirp = uap->to;
	error = namei(&tond);
	tdvp = tond.ni_dvp;
	tvp = tond.ni_vp;
	if (tvp != NULL) {
		if (fvp->v_type == VDIR && tvp->v_type != VDIR) {
			error = ENOTDIR;
			goto out;
		} else if (fvp->v_type != VDIR && tvp->v_type == VDIR) {
			error = EISDIR;
			goto out;
		}
	}
	if (error) {
		VOP_ABORTOP(ndp);
		goto out1;
	}
	if (fvp->v_mount != tdvp->v_mount) {
		error = EXDEV;
		goto out;
	}
	if (fvp == tdvp)
		error = EINVAL;
	/*
	 * If source is the same as the destination,
	 * then there is nothing to do.
	 */
	if (fvp == tvp)
		error = -1;
out:
	if (error) {
		VOP_ABORTOP(&tond);
		VOP_ABORTOP(ndp);
	} else {
		error = VOP_RENAME(ndp, &tond);
	}
out1:
	ndrele(&tond);
	if (error == -1)
		RETURN (0);
	RETURN (error);
}

/*
 * Mkdir system call
 */
mkdir(scp)
	register struct syscontext *scp;
{
	struct a {
		char	*name;
		int	dmode;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	register struct vnode *vp;
	struct vattr vattr;
	int error;

	ndp->ni_nameiop = CREATE | LOCKPARENT;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->name;
	if (error = namei(ndp))
		RETURN (error);
	vp = ndp->ni_vp;
	if (vp != NULL) {
		VOP_ABORTOP(ndp);
		RETURN (EEXIST);
	}
	vattr_null(&vattr);
	vattr.va_type = VDIR;
	vattr.va_mode = (uap->dmode & 0777) &~ scp->sc_cmask;
	error = VOP_MKDIR(ndp, &vattr);
	if (!error)
		vput(ndp->ni_vp);
	RETURN (error);
}

/*
 * Rmdir system call.
 */
rmdir(scp)
	register struct syscontext *scp;
{
	struct a {
		char	*name;
	} *uap = (struct a *)scp->sc_ap;
	register struct nameidata *ndp = &scp->sc_nd;
	register struct vnode *vp;
	int error;

	ndp->ni_nameiop = DELETE | LOCKPARENT | LOCKLEAF;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->name;
	if (error = namei(ndp))
		RETURN (error);
	vp = ndp->ni_vp;
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}
	/*
	 * No rmdir "." please.
	 */
	if (ndp->ni_dvp == vp) {
		error = EINVAL;
		goto out;
	}
	/*
	 * Don't unlink a mounted file.
	 */
	if (vp->v_flag & VROOT)
		error = EBUSY;
out:
	if (error)
		VOP_ABORTOP(ndp);
	else
		error = VOP_RMDIR(ndp);
	RETURN (error);
}

/*
 * Read a block of directory entries in a file system independent format
 */
getdirentries(scp)
	register struct syscontext *scp;
{
	register struct a {
		int	fd;
		char	*buf;
		unsigned count;
		long	*basep;
	} *uap = (struct a *)scp->sc_ap;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	off_t off;
	int error;

	if (error = getvnode(scp->sc_ofile, uap->fd, &fp))
		RETURN (error);
	if ((fp->f_flag & FREAD) == 0)
		RETURN (EBADF);
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_resid = uap->count;
	off = fp->f_offset;
	if (error = VOP_READDIR((struct vnode *)fp->f_data, &auio,
	    &(fp->f_offset), fp->f_cred))
		RETURN (error);
	error = copyout((caddr_t)&off, (caddr_t)uap->basep,
		sizeof(long));
	scp->sc_retval1 = uap->count - auio.uio_resid;
	RETURN (error);
}

/*
 * mode mask for creation of files
 */
umask(scp)
	register struct syscontext *scp;
{
	register struct a {
		int	mask;
	} *uap = (struct a *)scp->sc_ap;

	scp->sc_retval1 = scp->sc_cmask;
	scp->sc_cmask = uap->mask & 07777;
	RETURN (0);
}

getvnode(ofile, fdes, fpp)
	struct file *ofile[];
	struct file **fpp;
	int fdes;
{
	struct file *fp;

	if ((unsigned)fdes >= NOFILE || (fp = ofile[fdes]) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return (EINVAL);
	*fpp = fp;
	return (0);
}
