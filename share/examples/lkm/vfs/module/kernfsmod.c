/* 05 Jun 93*/
/*
 * kernfsmod.c
 *
 * 05 Jun 93	Terry Lambert		Original
 *
 * Copyright (c) 1993 Terrence R. Lambert.
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
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TERRENCE R. LAMBERT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#define printf I_HATE_ANSI
#include <stdio.h>
#undef printf
#include <stdlib.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/lkm.h>
#include <a.out.h>
#include <sys/file.h>
#include <sys/errno.h>

/*
 * This is the vfsops table from /sys/miscfs/kernfs/kernfs_vfsops.c
 */
extern struct vfsops kernfs_vfsops;

/*
 * Currently, the mount system call is broken in the way it operates
 * and the vfssw[] table does not have a character string identifier
 * for the file system type; therefore, to remount a file system after
 * it has been mounted in the first place, the offset into the table
 * must be the same; this will be corrected in future patches, but
 * not right now.  At the same time the fstab format will need to
 * change to allow definition without mount of file systems.
 *
 * The flags field is a parameter to the init; this could be used to
 * change the file system operation: for instance, in ISOFS, this
 * could be used to enable/disable Rockridge extensions.
 */
MOD_VFS("kernfs",MOUNT_KERNFS,0,&kernfs_vfsops)

/*
 * This function is called each time the module is loaded.   Technically,
 * we could have made this "nosys" in the "DISPATCH" in "kernfsmod()",
 * but it's a convenient place to kick a copyright out to the console.
 */
static int
kernfsmod_load( lkmtp, cmd)
struct lkm_table	*lkmtp;
int			cmd;
{
	if( cmd == LKM_E_LOAD) {	/* print copyright on console*/
		printf( "\nSample Loaded file system\n");
		printf( "Copyright (c) 1990, 1992 Jan-Simon Pendry\n");
		printf( "All rights reserved.\n");
		printf( "\nLoader stub and module loader is\n");
		printf( "Copyright (c) 1993\n");
		printf( "Terrence R. Lambert\n");
		printf( "All rights reserved\n");
	}

	return( 0);
}


/*
 * External entry point; should generally match name of .o file.  The
 * arguments are always the same for all loaded modules.  The "load",
 * "unload", and "stat" functions in "DISPATCH" will be called under
 * their respective circumstances unless their value is "nosys".  If
 * called, they are called with the same arguments (cmd is included to
 * allow the use of a single function, ver is included for version
 * matching between modules and the kernel loader for the modules).
 *
 * Since we expect to link in the kernel and add external symbols to
 * the kernel symbol name space in a future version, generally all
 * functions used in the implementation of a particular module should
 * be static unless they are expected to be seen in other modules or
 * to resolve unresolved symbols alread existing in the kernel (the
 * second case is not likely to ever occur).
 *
 * The entry point should return 0 unless it is refusing load (in which
 * case it should return an errno from errno.h).
 */
kernfsmod( lkmtp, cmd, ver)
struct lkm_table	*lkmtp;	
int			cmd;
int			ver;
{
	DISPATCH(lkmtp,cmd,ver,kernfsmod_load,nosys,nosys)
}


/*
 * EOF -- This file has not been truncated.
 */
