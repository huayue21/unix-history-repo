/*
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)endian.h	7.7 (Berkeley) %G%
 */

/*
 * Definitions for byte order,
 * according to byte significance from low address to high.
 */
#define	LITTLE_ENDIAN	1234	/* least-significant byte first (vax) */
#define	BIG_ENDIAN	4321	/* most-significant byte first (IBM, net) */
#define	PDP_ENDIAN	3412	/* LSB first in word, MSW first in long (pdp) */

#define	BYTE_ORDER	LITTLE_ENDIAN	/* byte order on vax */

/*
 * Macros for network/external number representation conversion.
 */
#if BYTE_ORDER == BIG_ENDIAN && !defined(lint)
#define	ntohl(x)	(x)
#define	ntohs(x)	(x)
#define	htonl(x)	(x)
#define	htons(x)	(x)

#define	NTOHL(x)	(x)
#define	NTOHS(x)	(x)
#define	HTONL(x)	(x)
#define	HTONS(x)	(x)

#else

#ifdef KERNEL
#include "cdefs.h"
#else
#include <sys/cdefs.h>
#endif

__BEGIN_DECLS
unsigned long	htonl __P((unsigned long));
unsigned short	htons __P((unsigned short));
unsigned long	ntohl __P((unsigned long));
unsigned short	ntohs __P((unsigned short));
__END_DECLS

#define	NTOHL(x)	(x) = ntohl((u_long)x)
#define	NTOHS(x)	(x) = ntohs((u_short)x)
#define	HTONL(x)	(x) = htonl((u_long)x)
#define	HTONS(x)	(x) = htons((u_short)x)
#endif
