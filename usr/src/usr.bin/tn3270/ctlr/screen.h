/*
 * @(#)screen.h	3.1 (Berkeley) %G%
 */

#define	INCLUDED_SCREEN

/* defines and defines to describe how to deal with the screen */

#if	!defined(MSDOS)
#define	MAXNUMBERLINES		43		/* 3278-4 */
#define	MAXNUMBERCOLUMNS	132		/* 3278-5 */
#define	MAXSCREENSIZE		3564		/* (27*132) 3278-5 */
#else	/* !defined(MSDOS) */	/* MSDOS has memory constraints */
#define	MAXNUMBERLINES		25		/* XXX */
#define	MAXNUMBERCOLUMNS	80
#define	MAXSCREENSIZE		(MAXNUMBERLINES*MAXNUMBERCOLUMNS)
#endif	/* !defined(MSDOS) */	/* MSDOS has memory constraints */
#define LowestScreen()	0
#define HighestScreen()	(ScreenSize-1)

#define ScreenLineOffset(x)	((x)%NumberColumns)
#define ScreenLine(x)	((int)((x)/NumberColumns))
#define ScreenInc(x)	(((x)==HighestScreen())? LowestScreen():x+1)
#define ScreenDec(x)	(((x)==LowestScreen())? HighestScreen():x-1)
#define ScreenUp(x)	(((x)+(ScreenSize-NumberColumns))%ScreenSize)
#define ScreenDown(x)	(((x)+NumberColumns)%ScreenSize)
#define	IsOrder(x)	(Orders[x])
#define BAIC(x)		((x)&0x3f)
#define CIAB(x)		(CIABuffer[(x)&0x3f])
#define BufferTo3270_0(x)	(CIABuffer[(int)((x)/0x40)])
#define BufferTo3270_1(x)	(CIABuffer[(x)&0x3f])
#define Addr3270(x,y)	(BAIC(x)*64+BAIC(y))
#define SetBufferAddress(x,y)	((x)*NumberColumns+(y))

/* These know how fields are implemented... */

#define WhereAttrByte(p)	(IsStartField(p)? p: FieldDec(p))
#define	WhereHighByte(p)	ScreenDec(FieldInc(p))
#define WhereLowByte(p)		ScreenInc(WhereAttrByte(p))
#define FieldAttributes(x)	(IsStartField(x)? GetHost(x) : \
				    GetHost(WhereAttrByte(x)))
#define FieldAttributesPointer(p)	(IsStartFieldPointer(p)? \
				    GetHostPointer(p): \
				    GetHost(WhereAttrByte((p)-&Host[0])))
#define TurnOffMdt(x)	ModifyHost(WhereAttrByte(x), &= ~ATTR_MDT)
#define TurnOnMdt(x)	ModifyHost(WhereAttrByte(x), |= ATTR_MDT)
#define HasMdt(x)	(GetHost(x)&ATTR_MDT)	/* modified tag */

	/*
	 * Is the screen formatted?  Some algorithms change depending
	 * on whether there are any attribute bytes lying around.
	 */
#define	FormattedScreen() \
	    ((WhereAttrByte(0) != 0) || ((GetHost(0)&ATTR_MASK) == ATTR_MASK))

					    /* field starts here */
#define IsStartField(x)	((GetHost(x)&ATTR_MASK) == ATTR_MASK)
#define IsStartFieldPointer(p)	((GetHostPointer(p)&ATTR_MASK) == ATTR_MASK)

#define NewField(p,a)	SetHost(p, (a)|ATTR_MASK)
#define DeleteField(p)	SetHost(p, 0)
#define	DeleteAllFields()

/* The following are independent of the implementation of fields */
#define IsProtectedAttr(p,a)	(IsStartField(p) || ((a)&ATTR_PROT))
#define IsProtected(p)	IsProtectedAttr(p,FieldAttributes(p))

#define IsUnProtected(x)	(!IsProtected(x))

#define IsAutoSkip(x)	(FieldAttributes(x)&ATTR_AUTO_SKIP)

#define IsNonDisplayAttr(c)	(((c)&ATTR_DSPD_MASK) == ATTR_DSPD_NONDISPLAY)
#define	IsNonDisplay(p)	IsNonDisplayAttr(FieldAttributes(p))

#define IsHighlightedAttr(c) \
		(((c)&ATTR_DSPD_MASK) == ATTR_DSPD_HIGH)
#define	IsHighlighted(p) \
		(IsHighlightedAttr(FieldAttributes(p)) && !IsStartField(p))


#define MAX(x,y)	((x)<(y)? (y):(x))
#define MIN(x,y)	((x)<(y)? x:(y))

typedef unsigned char ScreenImage;

extern int
	FieldFind();

extern char
	CIABuffer[];

#define	GetGeneric(i,h)		(h)[i]
#define	GetGenericPointer(p)	(*(p))
#define	SetGeneric(i,c,h)	((h)[i] = (c))
#define	ModifyGeneric(i,what,h)	{(h)[i] what;}

#define GetHost(i)		GetGeneric(i,Host)
#define GetHostPointer(p)	GetGenericPointer(p)
#define	SetHost(i,c)		SetGeneric(i,c,Host)
#define	ModifyHost(i,what)	ModifyGeneric(i,what,Host)
