/*-
 * Copyright (c) 1994, Garrett Wollman
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <nsswitch.h>
#include <arpa/nameser.h>
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif
#include "netdb_private.h"

#ifdef YP
static int
_getnetbynis(const char *name, char *map, int af, struct netent *ne,
    struct netent_data *ned)
{
	char *p, *bp, *ep;
	char *cp, **q;
	char *result;
	int resultlen, len;
	char ypbuf[YPMAXRECORD + 2];

	switch(af) {
	case AF_INET:
		break;
	default:
	case AF_INET6:
		errno = EAFNOSUPPORT;
		return -1;
	}

	if (ned->yp_domain == (char *)NULL)
		if (yp_get_default_domain (&ned->yp_domain))
			return -1;

	if (yp_match(ned->yp_domain, map, name, strlen(name), &result,
	    &resultlen))
		return -1;

	bcopy((char *)result, (char *)&ypbuf, resultlen);
	ypbuf[resultlen] = '\0';
	free(result);
	result = (char *)&ypbuf;

	if ((cp = index(result, '\n')))
		*cp = '\0';

	cp = strpbrk(result, " \t");
	*cp++ = '\0';
	bp = ned->netbuf;
	ep = ned->netbuf + sizeof ned->netbuf;
	len = strlen(result) + 1;
	if (ep - bp < len) {
		h_errno = NETDB_INTERNAL;
		return -1;
	}
	strlcpy(bp, result, ep - bp);
	ne->n_name = bp;
	bp += len;

	while (*cp == ' ' || *cp == '\t')
		cp++;

	ne->n_net = inet_network(cp);
	ne->n_addrtype = AF_INET;

	q = ne->n_aliases = ned->net_aliases;
	cp = strpbrk(cp, " \t");
	if (cp != NULL)
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q > &ned->net_aliases[_MAXALIASES - 1])
			break;
		p = strpbrk(cp, " \t");
		if (p != NULL)
			*p++ = '\0';
		len = strlen(cp) + 1;
		if (ep - bp < len)
			break;
		strlcpy(bp, cp, ep - bp);
		*q++ = bp;
		bp += len;
		cp = p;
	}
	*q = NULL;
	return 0;
}
#endif /* YP */

int
_nis_getnetbyname(void *rval, void *cb_data, va_list ap)
{
#ifdef YP
	const char *name;
	struct netent *ne;
	struct netent_data *ned;
	int error;

	name = va_arg(ap, const char *);
	ne = va_arg(ap, struct netent *);
	ned = va_arg(ap, struct netent_data *);

	error = _getnetbynis(name, "networks.byname", AF_INET, ne, ned);
	return (error == 0) ? NS_SUCCESS : NS_NOTFOUND;
#else
	return NS_UNAVAIL;
#endif

}

int 
_nis_getnetbyaddr(void *rval, void *cb_data, va_list ap)
{
#ifdef YP
	unsigned long addr;
	int af;
	struct netent *ne;
	struct netent_data *ned;
	char *str, *cp;
	unsigned long net2;
	int nn;
	unsigned int netbr[4];
	char buf[MAXDNAME];
	int error;

	addr = va_arg(ap, unsigned long);
	af = va_arg(ap, int);
	ne = va_arg(ap, struct netent *);
	ned = va_arg(ap, struct netent_data *);

	if (af != AF_INET) {
		errno = EAFNOSUPPORT;
		return NS_UNAVAIL;
	}

        for (nn = 4, net2 = addr; net2; net2 >>= 8) {
                netbr[--nn] = net2 & 0xff;
	}

	switch (nn) {
	case 3:		/* Class A */
		sprintf(buf, "%u", netbr[3]);
		break;
        case 2:		/* Class B */
		sprintf(buf, "%u.%u", netbr[2], netbr[3]);
		break;
        case 1:		/* Class C */
		sprintf(buf, "%u.%u.%u", netbr[1], netbr[2], netbr[3]);
                break;
        case 0:		/* Class D - E */
		sprintf(buf, "%u.%u.%u.%u", netbr[0], netbr[1],
			netbr[2], netbr[3]);
		break;
	}

	str = (char *)&buf;
	cp = str + (strlen(str) - 2);

	while(!strcmp(cp, ".0")) {
		*cp = '\0';
		cp = str + (strlen(str) - 2);
	}

	error = _getnetbynis(str, "networks.byaddr", af, ne, ned);
	return (error == 0) ? NS_SUCCESS : NS_NOTFOUND;
#else
	return NS_UNAVAIL;
#endif /* YP */
}
