/* Copyright (c) 1980 Regents of the University of California */
/* "@(#)as.h 4.7 %G%" */
#ifdef VMS
# define	vax	1
# define	VAX	1
#endif VMS

#include <sys/types.h>
#ifdef UNIX

#ifdef FLEXNAMES
#  include <a.out.h>
#  include <stab.h>
#else not FLEXNAMES
#  include <olda.out.h>
#  include <stab.h>
#endif FLEXNAMES

#endif UNIX 
#ifdef VMS

#ifdef UNIXDEVEL
#  include <a.out.h>
#else not UNIXDEVEL
#  include <aout.h>
#endif not UNIXDEVEL

#endif VMS

#define readonly
#define	NINST		300

#define	NEXP		20	/* max number of expr. terms per instruction */
#define	NARG		6	/* max number of args per instruction */
#define	NHASH		1103	/* hash table is dynamically extended */
#define	TNAMESIZE	32	/* maximum length of temporary file names */
#define	NLOC		4	/* number of location ctrs */

#ifdef UNIX
# ifndef	FLEXNAMES
#	ifndef	NCPS
#		define	NCPS	8	/* number of characters per symbol*/
#	endif
# else
#	ifdef NCPS
#		undef	NCPS
#	endif
#	define	NCPS	BUFSIZ	/* needed to allocate yytext */
# endif
# endif UNIX

# ifdef VMS
# ifdef NCPS
#	undef	NCPS
# endif NCPS
#	define	NCPS	15
# endif VMS

/*
 * Symbol types
 */
#define	XUNDEF	0x0
#define	XABS	0x2
#define	XTEXT	0x4
#define	XDATA	0x6
#define	XBSS	0x8

#define	XXTRN	0x1
#define	XTYPE	0x1E

#define	XFORW	0x20	/* Was forward-referenced when undefined */

#define	ERR	(-1)
#define	NBPW	32	/* Bits per word */

#define	AMASK	017

/*
 * Actual argument syntax types
 */
#define	AREG	1	/* %r */
#define	ABASE	2	/* (%r) */
#define	ADECR	3	/* -(%r) */
#define	AINCR	4	/* (%r)+ */
#define	ADISP	5	/* expr(%r) */
#define	AEXP	6	/* expr */
#define	AIMM	7	/* $ expr */
#define	ASTAR	8	/* * */
#define	AINDX	16	/* [%r] */

/*
 * Argument access types used to test validity of operands to operators
 */
#define	ACCR	(1<<3)				/* read */
#define	ACCW	(2<<3)				/* write */
#define	ACCB	(4<<3)				/* branch displacement */
#define	ACCA	(8<<3)				/* address only */
#define	ACCM	(ACCR | ACCW)			/* modify */
#define	ACCI	(ACCB | ACCR)			/* XFC code */

#define ACCESSMASK	(ACCA | ACCR | ACCW | ACCB)	/* the mask */

/*
 *	Argument data types
 *	Also used to tell outrel what it is relocating
 *	(possibly in combination with RELOC_PCREL and TYPNONE)
 */
#define	TYPB		0	/* byte */
#define	TYPW		1	/* word */
#define	TYPL		2	/* long */
#define	TYPQ		3	/* quad */
#define	TYPF		4	/* floating */
#define	TYPD		5	/* double floating */
#define	TYPNONE		6	/* when nothing */
#define	RELOC_PCREL	8	/* implicit argument to outrel; ==> PCREL */

#define	TYPMASK	7

/*
 *	reference types for loader
 */
#define	PCREL	1
#define	LEN1	2
#define	LEN2	4
#define	LEN4	6
#define	LEN8	8

extern	int	reflen[];	/* {LEN*+PCREL} ==> number of bytes */
extern	int	lgreflen[];	/* {LEN*+PCREL} ==> lg number of bytes */
extern	int	len124[];	/* {1,2,4,8} ==> {LEN1, LEN2, LEN4, LEN8} */
extern	char	mod124[];	/* {1,2,4,8} ==> {bits to construct operands */
extern	int	type_124[];	/* {1,2,4,8} ==> {TYPB, TYPW, TYPL, TYPQ} */
extern	int	ty_NORELOC[];	/* {TYPB..TYPD} ==> {1 if relocation not OK */
extern	int	ty_LEN[];	/* {TYPB..TYPD} ==> {LEN1..LEN8} */
extern	int	ty_nbyte[];	/* {TYPB..TYPD} ==> {1,2,4,8} */
extern	int	ty_nlg[];	/* {TYPB..TYPD} ==> lg{1,2,4,8} */

#define	TMPC	7	
#define	HW	01
#define	FW	03
#define	DW	07

#ifdef UNIX
#  include <pagsiz.h>
#endif UNIX

#ifdef VMS
#  define PAGRND	0x1FFL
#endif VMS

#define	round(x,y)	(((x)+(y)) & ~(y))

#define	STABTYPS	0340
#define	STABFLAG	0200

/*
 *	Follows are the definitions for the symbol table tags, which are
 *	all unsigned characters..
 *	High value tags are generated by the asembler for internal
 *	use.
 *	Low valued tags are the parser coded tokens the scanner returns.
 *	There are several pertinant bounds in this ordering:
 *		a)	Symbols greater than JXQUESTIONABLE
 *			are used by the jxxx bumper, indicating that
 *			the symbol table entry is a jxxx entry
 *			that has yet to be bumped.
 *		b)	Symbols greater than IGNOREBOUND are not
 *			bequeathed to the loader; they are truly
 *			for assembler internal use only.
 *		c)	Symbols greater than OKTOBUMP represent
 *			indices into the program text that should
 *			be changed in preceeding jumps or aligns
 *			must get turned into their long form.
 */

#define	TAGMASK		0xFF

#	define	JXACTIVE	0xFF	/*jxxx size unknown*/
#	define	JXNOTYET	0xFE	/*jxxx size known, but not yet expanded*/
#	define	JXALIGN		0xFD	/*align jxxx entry*/
#	define	JXINACTIVE	0xFC	/*jxxx size known and expanded*/

#define	JXQUESTIONABLE		0xFB

#	define	JXTUNNEL	0xFA	/*jxxx that jumps to another*/
#	define	OBSOLETE	0xF9	/*erroneously entered symbol*/

#define	IGNOREBOUND	0xF8		/*symbols greater than this are ignored*/
#	define	STABFLOATING	0xF7
#	define	LABELID		0xF6

#define	OKTOBUMP	0xF5
#	define	STABFIXED	0xF4

/*
 *	astoks.h contains reserved word codings the parser should
 *	know about
 */
#include "astoks.h"

/*
 *	The structure for one symbol table entry.
 *	Symbol table entries are used for both user defined symbols,
 *	and symbol slots generated to create the jxxx jump from
 *	slots.
 *	Caution: the instructions are stored in a shorter version
 *	of the struct symtab, using all fields in sym_nm and
 *	tag.  The fields used in sym_nm are carefully redeclared
 *	in struct Instab and struct instab (see below).
 *	If struct nlist gets changed, then Instab and instab may
 *	have to be changed.
 */

struct symtab{
		struct	nlist	s_nm;
		u_char	s_tag;		/* assembler tag */
		u_char	s_ptype;	/* if tag == NAME */
		u_char	s_jxoveralign;	/* if a JXXX, jumped over align */
		short	s_index;	/* which segment */
		struct	symtab *s_dest;	/* if JXXX, where going to */
#ifdef DJXXX
		short	s_jxline;	/* source line of the jump from */
#endif
};
/*
 *	Redefinitions of the fields in symtab for
 *	use when the symbol table entry marks a jxxx instruction.
 */
#define	s_jxbump	s_ptype		/* tag == JX..., how far to expand */
#define	s_jxfear	s_desc		/* how far needs to be bumped */
/*
 *	Redefinitions of fields in the struct nlist for symbols so that
 *	one saves typing, and so that they conform 
 *	with the old naming conventions.
 */
#ifdef	FLEXNAMES
#define	s_name	s_nm.n_un.n_name	/* name pointer */
#define	s_nmx	s_nm.n_un.n_strx	/* string table index */
#else 	not FLEXNAMES
#define	s_name	s_nm.n_name
#endif
#define	s_type	s_nm.n_type		/* type of the symbol */
#define	s_other	s_nm.n_other		/* other information for sdb */
#define	s_desc	s_nm.n_desc		/* type descriptor */
#define	s_value	s_nm.n_value		/* value of the symbol, or sdb delta */

struct	instab{
	struct	nlist	s_nm;		/* instruction name, type (opcode) */
	u_char	s_tag;			
};
/*
 *	The fields nm.n_desc and nm.n_value total 6 bytes; this is
 *	just enough for the 6 bytes describing the argument types.
 *	We use a macro to define access to these 6 bytes, assuming that
 *	they are allocated adjacently.
 *	IF THE FORMAT OF STRUCT nlist CHANGES, THESE MAY HAVE TO BE CHANGED.
 *
 *	Instab is cleverly declared to look very much the combination of
 *	a struct symtab and a struct nlist.
 */
struct	Instab{
#ifdef FLEXNAMES
	char	*I_name;
#else not FLEXNAMES
	char	I_name[NCPS];
#endif
	u_char	I_opcode;
	char	I_nargs;
	char	I_args[6];
	u_char	I_s_tag;
};
/*
 *	Redefinitions of fields in the struct nlist for instructions so that
 *	one saves typing, and conforms to the old naming conventions
 */
#define	i_opcode	s_nm.n_type	/* use the same field as symtab.type */
#define	i_nargs		s_nm.n_other	/* number of arguments */
#define	fetcharg(ptr, n) ((struct Instab *)ptr)->I_args[n]

struct	arg {				/*one argument to an instruction*/
	char	a_atype;
	char	a_areg1;
	char	a_areg2;
	char	a_dispsize;		/*usually d124, unless have B^, etc*/
	struct	exp *a_xp;
};

struct	exp {
	long	e_xvalue;		/* MUST be the first field (look at union Double) */
	long	e_yvalue;		/* MUST be second field; least sig word of a double */
	char	e_xtype;
	char	e_xloc;
	struct	symtab *e_xname;
};

#define doub_MSW e_xvalue
#define doub_LSW e_yvalue

union	Double {
	struct{
		long	doub_MSW;
		long	doub_LSW;
	} dis_dvalue;
	double	dvalue;
};

struct	Quad {
	long	quad_low_long;
	long	quad_high_long;
};

/*
 *	Magic layout macros
 */
#define 	MINBYTE	-128
#define		MAXBYTE	127
#define		MINWORD	-32768
#define		MAXWORD	32767

#define		LITFLTMASK 0x000043F0	/*really magic*/
/*
 *		Is the floating point double word in xp a
 *		short literal floating point number?
 */
#define 	slitflt(xp) \
			(    (xp->doub_LSW == 0) \
			 &&  ( (xp->doub_MSW & LITFLTMASK) \
			      == xp->doub_MSW) )
/*
 *	If it is a slitflt, then extract the 6 interesting bits
 */
#define		extlitflt(xp) \
			xp->doub_MSW >> 4

	extern	struct	arg	arglist[NARG];	/*building operands in instructions*/
	extern	struct	exp	explist[NEXP];	/*building up a list of expressions*/
	extern	struct	exp	*xp;		/*current free expression*/
	/*
	 *	Communication between the scanner and the jxxx handlers.
	 *	lastnam:	the last name seen on the input
	 *	lastjxxx:	pointer to the last symbol table entry for
	 *			a jump from
	 */
	extern	struct	symtab	*lastnam;
	extern	struct	symtab	*lastjxxx;	

#ifdef VMS
	extern	char	*vms_obj_ptr;		/* object buffer pointer */
	extern	char	sobuf[];		/* object buffer         */
	extern	int	objfil;			/* VMS object file descriptor */
#endif VMS

	/*
	 *	Lgensym is used to make up funny names for local labels.
	 *	lgensym[i] is the current funny number to put after
	 *	references to if, lgensym[i]-1 is for ib.
	 *	genref[i] is set when the label is referenced before
	 *	it is defined (i.e. 2f) so that we can be sure these
	 *	labels are always defined to avoid weird diagnostics
	 *	from the loader later.
	 */
	extern	int	lgensym[10];
	extern	char	genref[10];

	extern	char	tmpn1[TNAMESIZE];	/* Interpass temporary */
	extern	struct	exp	*dotp;		/* the current dot location */
	extern	int	loctr;

	extern	struct	exec	hdr;		/* a.out header */
	extern	u_long	tsize;			/* total text size */
	extern	u_long	dsize;			/* total data size */
	extern	u_long	trsize;			/* total text relocation size */
	extern	u_long	drsize;			/* total data relocation size */
	extern	u_long	datbase;		/* base of the data segment */
	/*
	 *	Bitoff and bitfield keep track of the packing into 
	 *	bytes mandated by the expression syntax <expr> ':' <expr>
	 */
	extern	int	bitoff;	
	extern	long	bitfield;
	
	/*
	 *	The lexical analyzer builds up symbols in yytext.  Lookup
	 *	expects its argument in this buffer
	 */
	extern	char	yytext[NCPS+2];		/* text buffer for lexical */
	/*
	 *	Variables to manage the input assembler source file
	 */
	extern	int	lineno;			/*the line number*/
	extern	char	*dotsname;		/*the name of the as source*/

	extern	FILE	*tmpfil;		/* interpass communication*/

	extern	int	passno;			/* 1 or 2 */

	extern	int	anyerrs;		/*errors assembling arguments*/
	extern	int	silent;			/*don't mention the errors*/
	extern	int	savelabels;		/*save labels in a.out*/
	extern	int	orgwarn;		/* questionable origin ? */
	extern	int	useVM;			/*use virtual memory temp file*/
	extern	int	jxxxJUMP;		/*use jmp instead of brw for jxxx */
	extern	int	readonlydata;		/*initialized data into text space*/
#ifdef DEBUG
	extern	int	debug;
	extern	int	toktrace;
#endif
	/*
	 *	Information about the instructions
	 */
	extern	struct	instab	*itab[NINST];	/*maps opcodes to instructions*/
	extern  readonly struct Instab instab[];

	extern	int	curlen;			/*current literal storage size*/
	extern	int	d124;			/*current pointer storage size*/
	
	struct	symtab	**lookup();		/*argument in yytext*/
	struct 	symtab	*symalloc();

#define outb(val) {dotp->e_xvalue++; if (passno==2) bputc((val), (txtfil));}

#define outs(cp, lg) dotp->e_xvalue += (lg); if (passno == 2) bwrite((cp), (lg), (txtfil))

/*
 *	Most of the time, the argument to flushfield is a power of two constant,
 *	the calculations involving it can be optimized to shifts.
 */
#define flushfield(n) if (bitoff != 0)  Flushfield( ( (bitoff+n-1) /n ) * n)

/*
 * The biobuf structure and associated routines are used to write
 * into one file at several places concurrently.  Calling bopen
 * with a biobuf structure sets it up to write ``biofd'' starting
 * at the specified offset.  You can then use ``bwrite'' and/or ``bputc''
 * to stuff characters in the stream, much like ``fwrite'' and ``fputc''.
 * Calling bflush drains all the buffers and MUST be done before exit.
 */
struct	biobuf {
	short	b_nleft;		/* Number free spaces left in b_buf */
/* Initialize to be less than BUFSIZ initially, to boundary align in file */
	char	*b_ptr;			/* Next place to stuff characters */
	char	b_buf[BUFSIZ];		/* The buffer itself */
	off_t	b_off;			/* Current file offset */
	struct	biobuf *b_link;		/* Link in chain for bflush() */
};
#define	bputc(c,b) ((b)->b_nleft ? (--(b)->b_nleft, *(b)->b_ptr++ = (c)) \
		       : bflushc(b, c))
#define BFILE	struct biobuf

	extern	BFILE	*biobufs;	/* head of the block I/O buffer chain */
	extern	int	biofd;		/* file descriptor for block I/O file */
	extern	off_t	boffset;	/* physical position in logical file */

	/*
	 *	For each of the named .text .data segments
	 *	(introduced by .text <expr>), we maintain
	 *	the current value of the dot, and the BFILE where
	 *	the information for each of the segments is placed
	 *	during the second pass.
	 */
	extern	struct	exp	usedot[NLOC + NLOC];
	extern		BFILE	*usefile[NLOC + NLOC];
	extern		BFILE	*txtfil;/* file for text and data: into usefile */
	/*
	 *	Relocation information for each segment is accumulated
	 *	seperately from the others.  Writing the relocation
	 *	information is logically viewed as writing to one
	 *	relocation saving file for  each segment; physically
	 *	we have a bunch of buffers allocated internally that
	 *	contain the relocation information.
	 */
	struct	relbufdesc	*rusefile[NLOC + NLOC];
	struct	relbufdesc	*relfil;
