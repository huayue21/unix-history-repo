/* Copyright (c) 1982 Regents of the University of California */

static char sccsid[] = "@(#)resume.c 1.5 %G%";

/*
 * resume execution, first setting appropriate registers
 */

#include "defs.h"
#include <signal.h>
#include "process.h"
#include "machine.h"
#include "main.h"
#include "process.rep"
#include "runtime/frame.rep"

#   if (isvaxpx)
#       include "machine/pxerrors.h"
#       include "pxinfo.h"
#   endif

LOCAL ADDRESS fetchpc();

/*
 * If we hit a breakpoint, px's pc points at a halt instruction,
 * this must be avoided when restarting.
 */

resume()
{
    register PROCESS *p;
    int oldsigno;

    p = process;
    do {
	if (option('e')) {
	    printf("execution resumes at pc 0x%x, lc %d\n", process->pc, pc);
	    fflush(stdout);
	}
	pcont(p);
#       if (isvaxpx)
	    if (p->status == STOPPED) {
		if (isbperr()) {
		    pc = p->reg[11];
		} else {
		    dread(&pcframe, PCADDRP, sizeof(pcframe));
		    pcframe++;
		    pc = fetchpc(pcframe);
		}
		pc -= (sizeof(char) + ENDOFF);
	    }
#       else
	    pc = process->pc;
#       endif
	if (option('e')) {
	    printf("execution stops at pc 0x%x, lc %d on sig %d\n",
		process->pc, pc, p->signo);
	    fflush(stdout);
	}
	if (p->status == STOPPED) {
	    errnum = 0;
	}
    } while (p->signo == SIGCONT);
#   if (isvaxpx)
	if (option('r') && p->signo != 0) {
	    choose();
	}
	if (isbperr()) {
	    p->pc++;
	}
#   endif
}

# if (isvaxpx)

/*
 * Find the location in the Pascal object where execution was suspended.
 *
 * We basically walk back through the frames looking for saved
 * register 11's.  Each time we find one, we remember it.  When we reach
 * the frame associated with the interpreter procedure, the most recently
 * saved register 11 is the one we want.
 */

typedef struct {
    int fr_handler;
    unsigned int fr_psw : 16;   /* saved psw */
    unsigned int fr_mask : 12;  /* register save mask */
    unsigned int fr_unused : 1;
    unsigned int fr_s : 1;      /* call was a calls, not callg */
    unsigned int fr_spa : 2;    /* stack pointer alignment */
    unsigned int fr_savap;      /* saved arg pointer */
    unsigned int fr_savfp;      /* saved frame pointer */
    int fr_savpc;           /* saved program counter */
} Vaxframe;

#define regsaved(frame, n) ((frame.fr_mask&(1 << n)) != 0)

LOCAL ADDRESS fetchpc(framep)
ADDRESS *framep;
{
    register PROCESS *p;
    Vaxframe vframe;
    ADDRESS *savfp;
    ADDRESS r;

    p = process;
    r = p->reg[11];
    if (p->fp == (ADDRESS) framep) {
	return r;
    }
    savfp = (ADDRESS *) p->fp;
    dread(&vframe, savfp, sizeof(vframe));
    while (vframe.fr_savfp != (int) framep && vframe.fr_savfp != 0) {
	if (regsaved(vframe, 11)) {
	    dread(&r, savfp + 5, sizeof(r));
	    r -= sizeof(char);
	}
	savfp = (ADDRESS *) vframe.fr_savfp;
	dread(&vframe, savfp, sizeof(vframe));
    }
    if (vframe.fr_savfp == 0) {
	panic("resume: can't find interpreter frame 0x%x", framep);
    }
    if (regsaved(vframe, 11)) {
	dread(&r, savfp + 5, sizeof(r));
	r -= sizeof(char);
    }
    return(r);
}

/*
 * Under the -r option, we offer the opportunity to just get
 * the px traceback and not actually enter the debugger.
 */

LOCAL choose()
{
    register int c;

    fprintf(stderr, "\nProgram error");
    if (errnum != 0) {
	fprintf(stderr, " -- %s", pxerrmsg[errnum]);
    }
    fprintf(stderr, "\nDo you wish to enter the debugger? ");
    c = getchar();
    if (c == 'n') {
	unsetsigtraces(process);
	pcont(process);
	quit(process->exitval);
    }
    while (c != '\n') {
	c = getchar();
    }
    fprintf(stderr, "\nEntering debugger ...");
    init();
    option('r') = FALSE;
    fprintf(stderr, " type 'help' for help.\n");
}

# endif
