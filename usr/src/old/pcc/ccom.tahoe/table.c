#ifndef lint
static char sccsid[] = "@(#)table.c	1.16 (Berkeley) %G%";
#endif

# include "pass2.h"
/* special shapes (SSOREG,SZERO etc.) shouldn't be or-ed */
# define AWD SNAME|SOREG|SCON|STARNM|STARREG
/* tbl */
# define ANYSIGNED TPOINT|TINT|TSHORT|TCHAR
# define ANYUSIGNED TUNSIGNED|TUSHORT|TUCHAR
# define ANYFIXED ANYSIGNED|ANYUSIGNED
# define TWORD TINT|TUNSIGNED|TPOINT
/* tbl */

struct optab  table[] = {

#ifdef REG_CHAR
PCONV,	INAREG|INTAREG,
	SAREG|AWD,	TCHAR|TSHORT,
	SANY,	TPOINT,
		NAREG|NASL,	RESC1,
		"	cvtZLl	AL,A1\n",

PCONV,	INAREG|INTAREG,
	SAREG|AWD,	TUCHAR|TUSHORT,
	SANY,	TPOINT,
		NAREG|NASL,	RESC1,
		"	movzZLl	AL,A1\n",
#endif

	/* the following entry is to fix a problem with
	   the manner that the first pass handles the
	   type of a shift expression                 */
PCONV,	INAREG|INTAREG,
	SAREG|AWD,	TINT|TUNSIGNED,
	SANY,	TPOINT,
		NAREG|NASL,	RLEFT,
		"",

/* take care of redundant conversions introduced by reclaim() */
SCONV,	INAREG|INTAREG,
	STAREG,	TWORD,
	SANY,	TWORD,
		0,	RLEFT,
		"",

SCONV,	INAREG|INTAREG,
	SAREG|AWD,	ANYFIXED,
	SANY,	ANYFIXED,
		NAREG|NASL,	RESC1,
		"	ZU\n",

SCONV,	FORCC,
	SAREG|AWD,	ANYFIXED,
	SANY,	ANYFIXED,
		NAREG|NASL,	RESCC,
		"	ZV\n",

SCONV,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TDOUBLE,
	SANY,	TFLOAT,
		NAREG|NASL,	RESC1|RESCC,
		"	ldd	AL\n	cvdf\n	stf	TA1\n",

SCONV,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TFLOAT,
	SANY,	TDOUBLE,
		NAREG|NASL,	RESC1|RESCC,
		"	ldfd	AL\n	std	A1\n",

SCONV,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TINT,
	SANY,	TFLOAT|TDOUBLE,
		NAREG|NASL,	RESC1|RESCC,
		"	cvlZR	AL\n	stZR	TA1\n",

/* the hard stuff */
/* XXX how about TUCHAR|TUSHORT to TFLOAT|TDOUBLE? */
SCONV,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TUNSIGNED,
	SANY,	TFLOAT|TDOUBLE,
		NAREG|NASL,	RESC1|RESCC,
		"	ZY\n",

SCONV,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TFLOAT|TDOUBLE,
	SANY,	ANYUSIGNED,
		NAREG|NASL,	RESC1|RESCC,
		"	ZW\n",

/* XXX need to trim significance here? */
SCONV,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TFLOAT|TDOUBLE,
	SANY,	ANYSIGNED,
		NAREG|NASL,	RESC1|RESCC,
		"	ldZL	AL\n	cvZLl	A1\n",

INIT,	FOREFF,
	SCON,	TANY,
	SANY,	TWORD,
		0,	RNOP,
		"	.long	CL\n",

INIT,	FOREFF,
	SCON,	TANY,
	SANY,	TSHORT|TUSHORT,
		0,	RNOP,
		"	.word	CL\n",

INIT,	FOREFF,
	SCON,	TANY,
	SANY,	TCHAR|TUCHAR,
		0,	RNOP,
		"	.byte	CL\n",

#ifdef FORT
	/* for the use of fortran only */

GOTO,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	jbr	CL\n",
#endif

GOTO,	FOREFF,
	AWD,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	jmp	*AL\n",

GOTO,	FOREFF,
	SAREG,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	jmp	(AL)\n",

STARG,	FORARG,
	SCON|SOREG,	TANY,
	SANY,	TANY,
		NTEMP+2*NAREG,	RESC3,
		"ZS",

STASG,	FORARG,
	SNAME|SOREG,	TANY,
	SCON|SAREG,	TANY,
		0,	RNULL,
		"	ZT\nZS",

STASG,	FOREFF,
	SNAME|SOREG,	TANY,
	SCON|SAREG,	TANY,
		0,	RNOP,
		"ZS",

STASG,	INAREG,
	SNAME|SOREG,	TANY,
	SCON,	TANY,
		NAREG,	RESC1,
		"ZS	movl	AR,A1\n",

STASG,	INAREG,
	SNAME|SOREG,	TANY,
	SAREG,	TANY,
		0,	RRIGHT,
		"	pushl	AR\nZS	movl	(sp)+,AR\n",

OPLOG,	FORCC,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TWORD,
		0,	RESCC,
		"	cmpl	AL,AR\nZP",

/* tahoe won't handle uns char/short equality/inequality with mem and const */
EQ,	FORCC,
	AWD,		TUCHAR|TUSHORT,
	SCON,	TANY,
		NAREG,	RESCC,
		"	movzZLl	AL,A1\n	cmpZL	A1,AR\nZP",

NE,	FORCC,
	AWD,		TUCHAR|TUSHORT,
	SCON,	TANY,
		NAREG,	RESCC,
		"	movzZLl	AL,A1\n	cmpZL	A1,AR\nZP",

/* optim2() handles degenerate comparisons with constants */
OPLOG,	FORCC,
	SAREG|AWD,	TCHAR|TUCHAR|TSHORT|TUSHORT,
	SCON,	TANY,
		0,	RESCC,
		"	cmpZL	AL,AR\nZP",

OPLOG,	FORCC,
	SAREG|AWD,	TSHORT,
	SAREG|AWD,	TSHORT,
		0,	RESCC,
		"	cmpw	AL,AR\nZP",

OPLOG,	FORCC,
	SAREG|AWD,	TUSHORT,
	SAREG|AWD,	TUSHORT,
		0,	RESCC,
		"	cmpw	AL,AR\nZP",

OPLOG,	FORCC,
	SAREG|AWD,	TCHAR,
	SAREG|AWD,	TCHAR,
		0,	RESCC,
		"	cmpb	AL,AR\nZP",

OPLOG,	FORCC,
	SAREG|AWD,	TUCHAR,
	SAREG|AWD,	TUCHAR,
		0,	RESCC,
		"	cmpb	AL,AR\nZP",

OPLOG,	FORCC,
	SAREG|AWD,	TFLOAT,
	SAREG|AWD,	TFLOAT,
		0,	RESCC,
		"	cmpZL2	AL,AR\nZP",

OPLOG,	FORCC,
	SZERO,		TDOUBLE,
	SAREG|AWD,	TDOUBLE,
		0,	RESCC,
		"	cmpf2	AL,AR\nZP",

OPLOG,	FORCC,
	SAREG|AWD,	TDOUBLE,
	SAREG|AWD,	TDOUBLE,
		0,	RESCC,
		"	cmpZL2	AL,AR\nZP",

#ifdef FORT
/* some implicit conversions made explicit to help f77 out (sigh) */
OPLOG,	FORCC,
	SAREG|AWD,	TFLOAT,
	SAREG|AWD,	TDOUBLE,
		0,	RESCC,
		"	ldfd	AL\n	cmpd	AR\nZP",

/* ought to flip this comparison, save an instruction */
OPLOG,	FORCC,
	SAREG|AWD,	TDOUBLE,
	SAREG|AWD,	TFLOAT,
		NAREG|NEVEN|NASR,	RESCC,
		"	ldfd	AR\n	std	A1\n	cmpd2	AL,A1\nZP",
#endif

CCODES,	INAREG|INTAREG,
	SANY,	TANY,
	SANY,	TANY,
		NAREG,	RESC1,
		"	movl	$1,A1\nZN",

UNARY CALL,	INAREG|INTAREG,
	SCON,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1, /* should be register 0 */
		"	ZC,CL\n",

UNARY CALL,	INAREG|INTAREG,
	SAREG,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	ZC,(AL)\n",

UNARY CALL,	INAREG|INTAREG,
	SNAME,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,	/* really reg 0 */
		"	ZC,*AL\n",

UNARY CALL,	INAREG|INTAREG,
	SSOREG,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,	/* really reg 0 */
		"	ZC,*AL\n",

FORTCALL,	INAREG|INTAREG,
	SCON,	TANY,
	SAREG|AWD,	TFLOAT,
		NAREG|NASL|NASR,	RESC1,
		"	ldf	AR\n	CLf\n	stf	TA1\n",

ASG OPSHFT,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TWORD,
	SAREG|SCON,	ANYFIXED,
		0,	RLEFT|RESCC,
		"	ZH	AR,AL,AL\n",

ASG OPSHFT,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TWORD,
	AWD,	TCHAR|TUCHAR,
		0,	RLEFT|RESCC,
		"	ZH	AR,AL,AL\n",

ASG OPSHFT,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TWORD,
	AWD,	ANYFIXED,
		NAREG,	RLEFT|RESCC,
		"	ZB	AR,A1\n	ZH	A1,AL,AL\n",

OPSHFT,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TWORD,
	SAREG|SCON,	ANYFIXED,
		NAREG|NASL|NASR,	RESC1|RESCC,
		"	ZH	AR,AL,A1\n",

OPSHFT,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TWORD,
	AWD,	TCHAR|TUCHAR,
		NAREG|NASL|NASR,	RESC1|RESCC,
		"	ZH	AR,AL,A1\n",

OPSHFT,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TWORD,
	AWD,	ANYFIXED,
		NAREG|NASR,	RESC1|RESCC,
		"	ZB	AR,A1\n	ZH	A1,AL,A1\n",

INCR,	FOREFF,
	SAREG|AWD,	TWORD,
	SCON|SNAME,	TANY,
		0,	RLEFT,
		"	ZE\n",

DECR,	FOREFF,
	SAREG|AWD,	TWORD,
	SCON|SNAME,	TANY,
		0,	RLEFT,
		"	ZE\n",

INCR,	INAREG|INTAREG,
	SAREG|AWD,	TANY,
	SCON|SNAME,	TANY,
		NAREG,	RESC1,
		"	ZD\n",

DECR,	INAREG|INTAREG,
	SAREG|AWD,	TANY,
	SCON|SNAME,	TANY,
		NAREG,	RESC1,
		"	ZD\n",

ASSIGN,	INAREG|FOREFF,
	SAREG|AWD,	ANYFIXED,
	SAREG|AWD,	ANYFIXED,
		0,	RLEFT,
		"	ZU\n",

ASSIGN,	FORCC,
	SAREG|AWD,	ANYFIXED,
	SAREG|AWD,	ANYFIXED,
		0,	RESCC,
		"	ZV\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SIREG,	TDOUBLE,
	SZERO,	TANY,
		0,	RLEFT|RESCC,
		"	cvld	$0\n	std	AL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|SNAME|SOREG,	TDOUBLE,
	SZERO,	TANY,
		0,	RLEFT|RESCC,
		"	clrl	UL\n	clrl	AL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TDOUBLE,
	SIREG,	TDOUBLE,
		0,	RLEFT|RRIGHT|RESCC,
		"	ldd	AR\n	std	AL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SIREG,	TDOUBLE,
	SAREG|AWD,	TDOUBLE,
		0,	RLEFT|RRIGHT|RESCC,
		"	ldd	AR\n	std	AL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|SNAME|SOREG,	TDOUBLE,
	SAREG|SNAME|SOREG,	TDOUBLE,
		0,	RLEFT|RRIGHT|RESCC,
		"	movl	UR,UL\n	movl	AR,AL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TFLOAT,
	SZERO,	TANY,
		0,	RLEFT|RESCC,
		"	clrl	TAL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TFLOAT,
	SAREG|AWD,	TFLOAT,
		0,	RLEFT|RRIGHT|RESCC,
		"	movl	AR,TAL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TFLOAT,
	SAREG|AWD,	TDOUBLE,
		0,	RLEFT|RESCC,
		"	ldd	AR\n	cvdf\n	stf	AL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TDOUBLE,
	SAREG|AWD,	TFLOAT,
		0,	RLEFT|RESCC,
		"	ldfd	AR\n	std	AL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TFLOAT|TDOUBLE,
	SAREG|AWD,	TINT,
		0,	RLEFT|RESCC,
		"	cvlZL	AR\n	stZL	AL\n",

/* XXX need to trim significance here? */
ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TINT,
	SAREG|AWD,	TFLOAT|TDOUBLE,
		0,	RLEFT|RESCC,
		"	ldZR	AR\n	cvZRl	AL\n",

/* unfortunately assignments are exempt from type balancing */
ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TCHAR|TUCHAR|TSHORT|TUSHORT,
	SAREG|AWD,	TFLOAT|TDOUBLE,
		NAREG|NASL,	RLEFT|RESCC,
		"	ldZR	AR\n	cvZRl	A1\n	cvtlZL	A1,AL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TFLOAT|TDOUBLE,
	SAREG|AWD,	TCHAR|TSHORT,
		NAREG|NASL,	RLEFT|RESCC,
		"	cvtZRl	AR,A1\n	cvlZL	A1\n	stZL	AL\n",

ASSIGN,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TFLOAT|TDOUBLE,
	SAREG|AWD,	TUCHAR|TUSHORT,
		NAREG|NASL,	RLEFT|RESCC,
		"	movzZRl	AR,A1\n	cvlZL	A1\n	stZL	AL\n",

ASSIGN, INAREG|FOREFF|FORCC,
	SAREG|AWD,	TUNSIGNED,
	SAREG|AWD,	TFLOAT|TDOUBLE,
		0,	RLEFT,
		"	ZW\n",

ASSIGN, INAREG|FOREFF|FORCC,
	SAREG|AWD,	TFLOAT|TDOUBLE,
	SAREG|AWD,	TUNSIGNED,
		0,	RLEFT,
		"	ZY\n",

ASSIGN,	INAREG|FOREFF,
	SFLD,	TANY,
	SZERO,	TANY,
		0,	RRIGHT,
		"	andl2	N,AL\n",

ASSIGN,	FOREFF,
	SFLD,	TANY,
	SCON,	TANY,
		0,	RNOP,
		"	andl2	N,AL\n	orl2	ZF,AL\n",

ASSIGN,	INAREG,
	SFLD,	TANY,
	SCON,	TANY,
		NAREG,	RESC1,
		"	andl2	N,AL\n	orl2	ZF,AL\n	ZI\n",

ASSIGN,	FOREFF,
	SFLD,	TANY,
	SAREG|AWD,	TWORD,
		NAREG|NASR,	RNOP,
		"	shll	$H,AR,A1\n	andl2	M,A1\n	andl2	N,AL\n\
	orl2	A1,AL\n",

ASSIGN,	INAREG,
	SFLD,	TANY,
	SAREG|AWD,	TWORD,
		NAREG,	RESC1,
		"	shll	$H,AR,A1\n	andl2	M,A1\n	andl2	N,AL\n\
	orl2	A1,AL\n	ZI\n",

/* dummy UNARY MUL entry to get U* to possibly match OPLTYPE */
UNARY MUL,	FOREFF,
	SCC,	TANY,
	SCC,	TANY,
		0,	RNULL,
		"	help help help\n",

REG,	INTEMP,
	SANY,	TANY,
	SAREG,	TDOUBLE,
		2*NTEMP,	RESC1,
		"	movl	UR,U1\n	movl	AR,A1\n",

REG,	INTEMP,
	SANY,	TANY,
	SAREG,	TANY,
		NTEMP,	RESC1,
		"	movl	AR,A1\n",

REG,	FORARG,
	SANY,	TANY,
	SAREG,	TFLOAT,
		0,	RNULL,
		"	pushl	$0\n	pushl	AR\n",

REG,	FORARG,
	SANY,	TANY,
	SAREG,	TDOUBLE,
		0,	RNULL,
		"	pushl	UR\n	pushl	AR\n",

OPLEAF,	FOREFF,
	SANY,	TANY,
	SAREG|AWD,	TANY,
		0,	RLEFT,
		"",

OPLTYPE,	INAREG|INTAREG,
	SANY,	TANY,
	SZERO,	TDOUBLE,
		NAREG|NASR,	RESC1,
		"	clrl	U1\n	clrl	A1\n",

OPLTYPE,	INAREG|INTAREG,
	SANY,	TANY,
	SIREG,	TDOUBLE,
		NAREG|NASR,	RESC1,
		"	ldd	AR\n	std	A1\n",

OPLTYPE,	INAREG|INTAREG,
	SANY,	TANY,
	SAREG|SNAME|SOREG|SCON,	TDOUBLE,
		NAREG,		RESC1,
		"	movl	AR,A1\n	movl	UR,U1\n",

OPLTYPE,	INAREG|INTAREG,
	SANY,	TANY,
	SZERO,	TANY,
		NAREG|NASR,	RESC1,
		"	clrl	TA1\n",

OPLTYPE,	INAREG|INTAREG,
	SANY,	TANY,
	SAREG|AWD,	TFLOAT,
		NAREG|NASR,	RESC1,
		"	movl	AR,TA1\n",

OPLTYPE,	INAREG|INTAREG,
	SANY,	TANY,
	SANY,	ANYFIXED,
		NAREG|NASR,	RESC1,
		"	ZU\n",

OPLTYPE,	FORCC,
	SANY,	TANY,
	SAREG|AWD,	TFLOAT,
		0,	RESCC,
		"	ldf	AR\n	tstf\n",

OPLTYPE,	FORCC,
	SANY,	TANY,
	SAREG|AWD,	TDOUBLE,
		0,	RESCC,
		"	ldd	AR\n	tstd\n",

OPLTYPE,	FORCC,
	SANY,	TANY,
	SANY,	ANYFIXED,
		0,	RESCC,
		"	tstZR	AR\n",

OPLTYPE,	FORARG,
	SANY,	TANY,
	SANY,	TWORD,
		0,	RNULL,
		"	pushl	AR\n",

OPLTYPE,	FORARG,
	SANY,	TANY,
	SANY,	TCHAR|TSHORT,
		0,	RNULL,
		"	pushZR	AR\n",

OPLTYPE,	FORARG,
	SANY,	TANY,
	SANY,	TUCHAR|TUSHORT,
		0,	RNULL,
		"	movzZRl	AR,-(sp)\n",

OPLTYPE,	FORARG,
	SANY,	TANY,
	SZERO,	TFLOAT|TDOUBLE,
		0,	RNULL,
		"	pushl	$0\n	pushl	$0\n",

OPLTYPE,	FORARG,
	SANY,	TANY,
	SIREG,	TDOUBLE,
		0,	RNULL,
		"	ldd	AR\n	pushd\n",

OPLTYPE,	FORARG,
	SANY,	TANY,
	SAREG|SNAME|SOREG,	TDOUBLE,
		0,	RNULL,
		"	pushl	UR\n	pushl	AR\n",

OPLTYPE,	FORARG,
	SANY,	TANY,
	SAREG|AWD,	TFLOAT,
		0,	RNULL,
		"	pushl	$0\n	pushl	AR\n",

UNARY MINUS,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	ANYSIGNED|TUNSIGNED,
	SANY,	TANY,
		NAREG|NASL,	RESC1|RESCC,
		"	mnegZL	AL,A1\n",

UNARY MINUS,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TFLOAT|TDOUBLE,
	SANY,	TANY,
		NAREG|NASL,	RESC1|RESCC,
		"	lnZL	AL\n	stZL	TA1\n",

COMPL,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	ANYSIGNED|TUNSIGNED,
	SANY,	TANY,
		NAREG|NASL,	RESC1|RESCC,
		"	mcomZL	AL,A1\n",

COMPL,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	ANYFIXED,
	SANY,	TANY,
		NAREG|NASL,	RESC1|RESCC,
		"	movzZLl	AL,A1\n	mcoml	A1,A1\n",

AND,	FORCC,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TWORD,
		0,	RESCC,
		"	bitl	AR,AL\n",

AND,	FORCC,
	SAREG|AWD,	TSHORT|TUSHORT,
	SSCON,	TWORD,
		0,	RESCC,
		"	bitw	AR,AL\n",

AND,	FORCC,
	SAREG|AWD,	TSHORT|TUSHORT,
	SAREG|AWD,	TSHORT|TUSHORT,
		0,	RESCC,
		"	bitw	AR,AL\n",

AND,	FORCC,
	SAREG|AWD,	TCHAR|TUCHAR,
	SCCON,	TWORD,
		0,	RESCC,
		"	bitb	AR,AL\n",

AND,	FORCC,
	SAREG|AWD,	TCHAR|TUCHAR,
	SAREG|AWD,	TCHAR|TUCHAR,
		0,	RESCC,
		"	bitb	AR,AL\n",

/* General cases for DIV and ASG DIV are handled below with OPMUL */
/* Some special cases are handled in optim2() */

DIV,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TUNSIGNED|TULONG,
	SCON,	ANYUSIGNED,
		NAREG|NEVEN,	RESC1|RESCC,
		"	movl	AL,U1\n	clrl	A1\n	ediv	AR,A1,A1,U1\n",

ASG DIV,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TINT|TLONG|TUNSIGNED|TULONG,
	SMCON,	ANYUSIGNED,
		0,	RLEFT|RESCC,
		"	ZJ\n",

ASG DIV,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TINT|TLONG|TUNSIGNED|TULONG,
	SCON,	ANYUSIGNED,
		NAREG|NEVEN,	RLEFT|RESCC,
		"	movl	AL,U1\n	clrl	A1\n	ediv	AR,A1,AL,U1\n",

MOD,	INAREG|INTAREG,
	SAREG|AWD,	TINT|TLONG,
	SAREG|AWD,	TINT|TLONG,
		NAREG|NEVEN,	RESC1,
		"	ZM	ediv	AR,A1,U1,A1\n",

MOD,	INAREG|FOREFF,
	SAREG|AWD,	TUNSIGNED|TULONG,
	SMCON,	ANYUSIGNED,
		NAREG|NASL,	RLEFT|RESC1,
		"	ZJ\n",

MOD,	INAREG|FOREFF,
	SAREG|AWD,	TUNSIGNED|TULONG,
	SCON,	ANYUSIGNED,
		NAREG|NEVEN,	RESC1,
		"	movl	AL,U1\n	clrl	A1\n	ediv	AR,A1,U1,A1\n",

/* should only see UNSIGNED lhs here if converted from UCHAR/USHORT lhs */
ASG MOD,	INAREG|FOREFF,
	SAREG|AWD,	TINT|TLONG|TUNSIGNED|TULONG,
	SAREG|AWD,	TINT|TLONG,
		NAREG|NEVEN,	RLEFT|RESCC,
		"	ZM	ediv	AR,A1,A1,AL\n",

ASG MOD,	INAREG|FOREFF,
	SAREG|AWD,	TINT|TLONG|TUNSIGNED|TULONG,
	SMCON,	ANYUSIGNED,
		0,	RLEFT,
		"	ZJ\n",

ASG MOD,	INAREG|FOREFF,
	SAREG|AWD,	TINT|TLONG|TUNSIGNED|TULONG,
	SCON,	ANYUSIGNED,
		NAREG|NEVEN,	RLEFT,
		"	movl	AL,U1\n	clrl	A1\n	ediv	AR,A1,A1,AL\n",

/* XXX is this supposed to help on overflow? */
ASG MUL,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TUNSIGNED,
	SAREG|AWD,	TUNSIGNED|TINT,
		NAREG|NEVEN,	RLEFT|RESCC,
		"	emul	AR,AL,$0,A1\n	movl	U1,AL\n",

ASG MUL,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TUNSIGNED|TINT,
	SAREG|AWD,	TUNSIGNED,
		NAREG|NEVEN,	RLEFT|RESCC,
		"	emul	AR,AL,$0,A1\n	movl	U1,AL\n",

ASG OPMUL,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TWORD,
		0,	RLEFT|RESCC,
		"	OL2	AR,AL\n",

MUL,	INAREG|INTAREG|FORCC,
	STAREG,		TUNSIGNED,
	SAREG|AWD,	TUNSIGNED,
		NAREG|NEVEN,	RLEFT|RESCC,
		"	emul	AR,AL,$0,A1\n	movl	U1,AL\n",

OPMUL,	INAREG|INTAREG|FORCC,
	STAREG,		TWORD,
	SAREG|AWD,	TWORD,
		0,	RLEFT|RESCC,
		"	OL2	AR,AL\n",

MUL,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TUNSIGNED,
	SAREG|AWD,	TUNSIGNED,
		NAREG|NEVEN,	RESC1|RESCC,
		"	emul	AR,AL,$0,A1\n	movl	U1,A1\n",

OPMUL,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TWORD,
		NAREG|NASL|NASR,	RESC1|RESCC,
		"	OL3	AR,AL,A1\n",

#ifdef REG_CHAR
ASG PLUS,	INAREG|FOREFF|FORCC,
	SAREG,	TWORD,
	SONE,	TINT,
		0,	RLEFT|RESCC,
		"	incZL	AL\n",

ASG PLUS,	INAREG|FOREFF|FORCC,
	AWD,	ANYFIXED,
	SONE,	TINT,
		0,	RLEFT|RESCC,
		"	incZL	AL\n",

ASG MINUS,	INAREG|FOREFF|FORCC,
	SAREG,	TWORD,
	SONE,	TINT,
		0,	RLEFT|RESCC,
		"	decZL	AL\n",

ASG MINUS,	INAREG|FOREFF|FORCC,
	AWD,	ANYFIXED,
	SONE,	TINT,
		0,	RLEFT|RESCC,
		"	decZL	AL\n",
#else
ASG PLUS,       INAREG|FOREFF|FORCC,
	SAREG|AWD,	ANYFIXED,
	SONE,	TANY,
		0,	RLEFT|RESCC,
		"	incZL	AL\n",

ASG MINUS,       INAREG|FOREFF|FORCC,
	SAREG|AWD,	ANYFIXED,
	SONE,	TANY,
		0,	RLEFT|RESCC,
		"	decZL	AL\n",
#endif

PLUS,	INAREG|INTAREG|FORCC,
	STAREG,	TWORD,
	SONE,	TWORD,
		0,	RLEFT|RESCC,
		"	incZL	AL\n",

MINUS,	INAREG|INTAREG|FORCC,
	STAREG,	TWORD,
	SONE,	TWORD,
		0,	RLEFT|RESCC,
		"	decZL	AL\n",

ASG OPSIMP,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TWORD,
		0,	RLEFT|RESCC,
		"	OL2	AR,AL\n",

ASG OPSIMP,	INAREG|FOREFF|FORCC,
	SAREG,	TWORD,
	SAREG,	TSHORT|TUSHORT|TCHAR|TUCHAR,
		0,	RLEFT|RESCC,
		"	OL2	AR,AL\n",

ASG OPSIMP,	INAREG|FOREFF|FORCC,
	AWD,	TSHORT|TUSHORT,
	SAREG|AWD,	TSHORT|TUSHORT,
		0,	RLEFT|RESCC,
		"	OW2	AR,AL\n",

ASG OPSIMP,	INAREG|FOREFF|FORCC,
	AWD,	TSHORT|TUSHORT,
	SSCON,	TWORD,
		0,	RLEFT|RESCC,
		"	OW2	AR,AL\n",

ASG OPSIMP,	INAREG|FOREFF|FORCC,
	AWD,	TCHAR|TUCHAR,
	SAREG|AWD,	TCHAR|TUCHAR,
		0,	RLEFT|RESCC,
		"	OB2	AR,AL\n",

ASG OPSIMP,	INAREG|FOREFF|FORCC,
	AWD,	TCHAR|TUCHAR,
	SCCON,	TWORD,
		0,	RLEFT|RESCC,
		"	OB2	AR,AL\n",

OPSIMP,	INAREG|INTAREG|FORCC,
	STAREG,	ANYFIXED,
	SAREG|AWD,	TWORD,
		0,	RLEFT|RESCC,
		"	OL2	AR,AL\n",

OPSIMP,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TWORD,
	SAREG|AWD,	TWORD,
		NAREG|NASL|NASR,	RESC1|RESCC,
		"	OL3	AR,AL,A1\n",

ASG OPSIMP,	INAREG|FOREFF,
	SAREG|AWD,	TFLOAT|TDOUBLE,
	SZERO,	TANY,
		0,	RLEFT,
		"",

ASG MUL,	INAREG|FOREFF|FORCC,
	SIREG,	TDOUBLE,
	SZERO,	TANY,
		0,	RLEFT|RESCC,
		"	cvld	$0\n	std	AL\n",

ASG MUL,	INAREG|FOREFF|FORCC,
	SAREG|SNAME|SOREG,	TDOUBLE,
	SZERO,	TANY,
		0,	RLEFT|RESCC,
		"	clrl	UL\n	clrl	AL\n",

ASG MUL,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TFLOAT,
	SZERO,	TANY,
		0,	RLEFT|RESCC,
		"	clrl	TAL\n",

ASG OPFLOAT,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TFLOAT,
	SAREG|AWD,	TFLOAT,
		0,	RLEFT|RESCC,
		"	ldf	AL\n	OF	AR\n	stf	TAL\n",

ASG OPFLOAT,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TDOUBLE,
	SAREG|AWD,	TDOUBLE,
		0,	RLEFT|RESCC,
		"	ldd	AL\n	OD	AR\n	std	AL\n",

ASG PLUS,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TDOUBLE,
	SAREG|AWD,	TFLOAT,
		NAREG|NASL,	RLEFT|RESCC,
		"	ldfd	AR\n	OD	AL\n	std	AL\n",

ASG MUL,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TDOUBLE,
	SAREG|AWD,	TFLOAT,
		NAREG|NASL,	RLEFT|RESCC,
		"	ldfd	AR\n	OD	AL\n	std	AL\n",

ASG OPFLOAT,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TDOUBLE,
	SAREG|AWD,	TFLOAT,
		NAREG|NASL,	RLEFT|RESCC,
		"	ldfd	AR\n	std	A1\n	ldd	AL\n	OD	A1\n	std	AL\n",

ASG OPFLOAT,	INAREG|FOREFF|FORCC,
	SAREG|AWD,	TFLOAT,
	SAREG|AWD,	TDOUBLE,
		0,	RLEFT|RESCC,
		"	ldfd	AL\n	OD	AR\n	cvdf\n	stf	TAL\n",

ASG OPFLOAT,	INAREG|FOREFF,
	SAREG|AWD,	ANYFIXED,
	SAREG|AWD,	TFLOAT|TDOUBLE,
		NAREG,	RLEFT|RESCC,	/* usable() knows we may need a reg pair */
		"	ZG\n",

OPSIMP,	INAREG|INTAREG,
	SAREG,	TFLOAT|TDOUBLE,
	SZERO,	TANY,
		0,	RLEFT,
#if defined(FORT) || defined(SPRECC)
		"T",
#else
		"",
#endif

OPSIMP,	INAREG|INTAREG|FORCC,
	AWD,	TFLOAT,
	SZERO,	TANY,
		NAREG,	RESC1|RESCC,
		"	movl	AL,A1\n",

OPSIMP,	INAREG|INTAREG|FORCC,
	SIREG,	TDOUBLE,
	SZERO,	TANY,
		NAREG,	RESC1|RESCC,
		"	ldd	AL\n	std	A1\n",

OPSIMP,	INAREG|INTAREG|FORCC,
	SAREG|SNAME|SOREG,	TDOUBLE,
	SZERO,	TANY,
		NAREG,	RESC1|RESCC,
		"	movl	UL,U1\n	movl	AL,A1\n",

MUL,	INAREG|INTAREG|FORCC,
	SIREG,	TDOUBLE,
	SZERO,	TANY,
		NAREG|NASR,	RESC1|RESCC,
		"	cvld	$0\n	std	A1\n",

MUL,	INAREG|INTAREG|FORCC,
	SAREG|SNAME|SOREG,	TDOUBLE,
	SZERO,	TANY,
		NAREG|NASR,	RESC1|RESCC,
		"	clrl	U1\n	clrl	A1\n",

MUL,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TFLOAT,
	SZERO,	TANY,
		NAREG|NASR,	RESC1|RESCC,
		"	clrl	TA1\n",

OPFLOAT,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TFLOAT,
	SAREG|AWD,	TFLOAT,
		NAREG|NASL|NASR,	RESC1|RESCC,
		"	ldf	AL\n	OF	AR\n	stf	TA1\n",

OPFLOAT,	INAREG|INTAREG|FORCC,
	SZERO,		TANY,
	SAREG|AWD,	TDOUBLE,
		NAREG,	RESC1|RESCC,
		"\tclrl\tA1\n\tclrl\tU1\n\tldd\tA1\n\tOD\tAR\n\tstd\tA1\n",

OPFLOAT,	INAREG|INTAREG|FORCC,
	SAREG|AWD,	TDOUBLE,
	SAREG|AWD,	TDOUBLE,
		NAREG|NASL|NASR,	RESC1|RESCC,
		"	ldd	AL\n	OD	AR\n	std	A1\n",

	/* Default actions for hard trees ... */

# define DF(x) FORREW,SANY,TANY,SANY,TANY,REWRITE,x,""

UNARY MUL, DF( UNARY MUL ),

INCR, DF(INCR),

DECR, DF(INCR),

ASSIGN, DF(ASSIGN),

STASG, DF(STASG),

FLD, DF(FLD),

OPLEAF, DF(NAME),

OPLOG,	FORCC,
	SANY,	TANY,
	SANY,	TANY,
		REWRITE,	BITYPE,
		"",

OPLOG,	DF(NOT),

COMOP, DF(COMOP),

INIT, DF(INIT),

OPUNARY, DF(UNARY MINUS),


ASG OPANY, DF(ASG PLUS),

OPANY, DF(BITYPE),

FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	"help; I'm in trouble\n" };
