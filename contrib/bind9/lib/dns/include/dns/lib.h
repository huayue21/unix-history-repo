/*
 * Copyright (C) 2004, 2005  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: lib.h,v 1.8.18.4 2005-09-20 04:33:48 marka Exp $ */

#ifndef DNS_LIB_H
#define DNS_LIB_H 1

/*! \file */

#include <isc/types.h>
#include <isc/lang.h>

ISC_LANG_BEGINDECLS

/*%
 * Tuning: external query load in packets per seconds.
 */
LIBDNS_EXTERNAL_DATA extern unsigned int dns_pps;
LIBDNS_EXTERNAL_DATA extern isc_msgcat_t *dns_msgcat;

void
dns_lib_initmsgcat(void);
/*%<
 * Initialize the DNS library's message catalog, dns_msgcat, if it
 * has not already been initialized.
 */

ISC_LANG_ENDDECLS

#endif /* DNS_LIB_H */
