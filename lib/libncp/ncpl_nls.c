/*
 * Copyright (c) 1999-2002, Boris Popov
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

/*
 * Languages support. Currently is very primitive.
 */
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

#include <netncp/ncp_lib.h>
#include <netncp/ncp_cfg.h>
#include <netncp/ncp_nls.h>

#ifndef	NCP_NLS_DEFAULT
#define	NCP_NLS_DEFAULT	NCP_NLS_AS_IS
#endif

/*
 * TODO: Make all tables dynamically loadable.
 */
#ifdef NCP_NLS_KOI2CP866
/* Russian tables from easy-cyrillic:
 * Copyright (C) 1993-1994 by Andrey A. Chernov, Moscow, Russia
 */
static u_int8_t alt2koi8[] = {
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,	/* 0x00 */
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,	/* 0x10 */
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,	/* 0x20 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	/* 0x30 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x5f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 0x40 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,	/* 0x50 */
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,	/* 0x60 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,	/* 0x70 */
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0xe1, 0xe2, 0xf7, 0xe7, 0xe4, 0xe5, 0xf6, 0xfa,
	0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0,
	0xf2, 0xf3, 0xf4, 0xf5, 0xe6, 0xe8, 0xe3, 0xfe,
	0xfb, 0xfd, 0xff, 0xf9, 0xf8, 0xfc, 0xe0, 0xf1,
	0xc1, 0xc2, 0xd7, 0xc7, 0xc4, 0xc5, 0xd6, 0xda,
	0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
	0x90, 0x91, 0x92, 0x81, 0x87, 0xb2, 0xb4, 0xa7,
	0xa6, 0xb5, 0xa1, 0xa8, 0xae, 0xad, 0xac, 0x83,
	0x84, 0x89, 0x88, 0x86, 0x80, 0x8a, 0xaf, 0xb0,
	0xab, 0xa5, 0xbb, 0xb8, 0xb1, 0xa0, 0xbe, 0xb9,
	0xba, 0xb6, 0xb7, 0xaa, 0xa9, 0xa2, 0xa4, 0xbd,
	0xbc, 0x85, 0x82, 0x8d, 0x8c, 0x8e, 0x8f, 0x8b,
	0xd2, 0xd3, 0xd4, 0xd5, 0xc6, 0xc8, 0xc3, 0xde,
	0xdb, 0xdd, 0xdf, 0xd9, 0xd8, 0xdc, 0xc0, 0xd1,
	0xb3, 0xa3, 0x99, 0x98, 0x93, 0x9b, 0x9f, 0x97,
	0x9c, 0x95, 0x9e, 0x96, 0xbf, 0x9d, 0x94, 0x9a
};

static u_int8_t koi82alt[] = {
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,	/* 0x00 */
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,	/* 0x10 */
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,	/* 0x20 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	/* 0x30 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x5f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 0x40 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,	/* 0x50 */
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,	/* 0x60 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,	/* 0x70 */
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0xc4, 0xb3, 0xda, 0xbf, 0xc0, 0xd9, 0xc3, 0xb4,	/* 0x80 */
	0xc2, 0xc1, 0xc5, 0xdf, 0xdc, 0xdb, 0xdd, 0xde,
	0xb0, 0xb1, 0xb2, 0xf4, 0xfe, 0xf9, 0xfb, 0xf7,
	0xf3, 0xf2, 0xff, 0xf5, 0xf8, 0xfd, 0xfa, 0xf6,
	0xcd, 0xba, 0xd5, 0xf1, 0xd6, 0xc9, 0xb8, 0xb7,
	0xbb, 0xd4, 0xd3, 0xc8, 0xbe, 0xbd, 0xbc, 0xc6,
	0xc7, 0xcc, 0xb5, 0xf0, 0xb6, 0xb9, 0xd1, 0xd2,
	0xcb, 0xcf, 0xd0, 0xca, 0xd8, 0xd7, 0xce, 0xfc,
	0xee, 0xa0, 0xa1, 0xe6, 0xa4, 0xa5, 0xe4, 0xa3,
	0xe5, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae,
	0xaf, 0xef, 0xe0, 0xe1, 0xe2, 0xe3, 0xa6, 0xa2,
	0xec, 0xeb, 0xa7, 0xe8, 0xed, 0xe9, 0xe7, 0xea,
	0x9e, 0x80, 0x81, 0x96, 0x84, 0x85, 0x94, 0x83,
	0x95, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
	0x8f, 0x9f, 0x90, 0x91, 0x92, 0x93, 0x86, 0x82,	/* 0xf0 */ 
	0x9c, 0x9b, 0x87, 0x98, 0x9d, 0x99, 0x97, 0x9a
};

#endif

/*
 * Characters mapping for codepages used in Sweden.
 */
static u_int8_t se_nw2unix[] = {
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,	/* 0x00 */
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,	/* 0x10 */
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,	/* 0x20 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	/* 0x30 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x5f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 0x40 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,	/* 0x50 */
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,	/* 0x60 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,	/* 0x70 */
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0xe1, 0xe2, 0xf7, 0xe7, 0xE4, 0xc4, 0xE5, 0xfa, /* 0x80 */
	0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xC4, 0xC5,
	0xf2, 0xf3, 0xf4, 0xf5, 0xF6, 0xe8, 0xe3, 0xfe, /* 0x90 */
	0xfb, 0xD6, 0xff, 0xf9, 0xf8, 0xfc, 0xe0, 0xf1,
	0xc1, 0xc2, 0xd7, 0xc7, 0xc4, 0xc5, 0xd6, 0xda, /* 0xA0 */
	0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
	0x90, 0x91, 0x92, 0x81, 0x87, 0xb2, 0xb4, 0xa7, /* 0xB0 */
	0xa6, 0xb5, 0xa1, 0xa8, 0xae, 0xad, 0xac, 0x83,
	0x84, 0x89, 0x88, 0x86, 0x80, 0x8a, 0xaf, 0xb0, /* 0xC0 */
	0xab, 0xa5, 0xbb, 0xb8, 0xb1, 0xa0, 0xbe, 0xb9,
	0xba, 0xb6, 0xb7, 0xaa, 0xa9, 0xa2, 0xa4, 0xbd, /* 0xD0 */
	0xbc, 0x85, 0x82, 0x8d, 0x8c, 0x8e, 0x8f, 0x8b,
	0xd2, 0xd3, 0xd4, 0xd5, 0xc6, 0xc8, 0xc3, 0xde, /* 0xE0 */
	0xdb, 0xdd, 0xdf, 0xd9, 0xd8, 0xdc, 0xc0, 0xd1,
	0xb3, 0xa3, 0x99, 0x98, 0x93, 0x9b, 0x9f, 0x97, /* 0xF0 */
	0x9c, 0x95, 0x9e, 0x96, 0xbf, 0x9d, 0x94, 0x9a
};

static u_int8_t se_unix2nw[] = {
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,	/* 0x00 */
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,	/* 0x10 */
	0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,	/* 0x20 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	/* 0x30 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x5f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 0x40 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,	/* 0x50 */
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,	/* 0x60 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,	/* 0x70 */
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0xc4, 0xb3, 0xda, 0xbf, 0xc0, 0xd9, 0xc3, 0xb4,	/* 0x80 */
	0xc2, 0xc1, 0xc5, 0xdf, 0xdc, 0xdb, 0xdd, 0xde,
	0xb0, 0xb1, 0xb2, 0xf4, 0xfe, 0xf9, 0xfb, 0xf7, /* 0x90 */
	0xf3, 0xf2, 0xff, 0xf5, 0xf8, 0xfd, 0xfa, 0xf6,
	0xcd, 0xba, 0xd5, 0xf1, 0xd6, 0xc9, 0xb8, 0xb7, /* 0xA0 */
	0xbb, 0xd4, 0xd3, 0xc8, 0xbe, 0xbd, 0xbc, 0xc6,
	0xc7, 0xcc, 0xb5, 0xf0, 0xb6, 0xb9, 0xd1, 0xd2, /* 0xB0 */
	0xcb, 0xcf, 0xd0, 0xca, 0xd8, 0xd7, 0xce, 0xfc,
	0xee, 0xa0, 0xa1, 0xe6, 0x8E, 0x8F, 0xe4, 0xa3, /* 0xC0 */
	0xe5, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae,
	0xaf, 0xef, 0xe0, 0xe1, 0xe2, 0xe3, 0x99, 0xa2, /* 0xD0 */
	0xec, 0xeb, 0xa7, 0xe8, 0xed, 0xe9, 0xe7, 0xea,
	0x9e, 0x80, 0x81, 0x96, 0x84, 0x86, 0x94, 0x83, /* 0xE0 */
	0x95, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
	0x8f, 0x9f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x82,	/* 0xf0 */ 
	0x9c, 0x9b, 0x87, 0x98, 0x9d, 0x99, 0x97, 0x9a
};


static u_int8_t def2lower[256];
static u_int8_t def2upper[256];

/*
 * List of available charsets
 */
struct ncp_nlsdesc {
	int	scheme;
	char	*name;
	struct ncp_nlstables nls;
};

static struct ncp_nlsdesc ncp_nlslist[] = {
	{NCP_NLS_AS_IS, NCP_NLS_AS_IS_NAME, 
	    {def2lower, def2upper, NULL, NULL, 0}
	},
#ifdef NCP_NLS_KOI2CP866
	{NCP_NLS_KOI_866, NCP_NLS_KOI_866_NAME, 
	    {def2lower, def2upper, alt2koi8, koi82alt, 0}
	},
#endif
	{NCP_NLS_SE, NCP_NLS_SE_NAME, 
	    {def2lower, def2upper, se_nw2unix, se_unix2nw, 0}
	},
	{0}
};

struct ncp_nlstables ncp_nls;

int
ncp_nls_setlocale(char *name) {
	int i;

	ncp_nls.to_lower = def2lower;
	ncp_nls.to_upper = def2upper;
	if (setlocale(LC_CTYPE, name) == NULL) {
		fprintf(stderr, "Can't set locale '%s'\n", name);
		return EINVAL;
	}
	for (i = 0; i < 256; i++) {
		ncp_nls.to_lower[i] = tolower(i);
		ncp_nls.to_upper[i] = toupper(i);
	}
	return 0;
}

int
ncp_nls_setrecode(int scheme) {
	struct ncp_nlsdesc *nd;

	if (scheme == 0) {
#if NCP_NLS_DEFAULT
		scheme = NCP_NLS_DEFAULT;
#else
		scheme = NCP_NLS_AS_IS;
#endif
	}
	for (nd = ncp_nlslist; nd->name; nd++) {
		if (nd->scheme != scheme) continue;
		ncp_nls.u2n = nd->nls.u2n;
		ncp_nls.n2u = nd->nls.n2u;
		return ncp_nls_setlocale("");
	}
	fprintf(stderr, "Character conversion scheme %d was not compiled in\n", scheme);
	return EINVAL;
}

int
ncp_nls_setrecodebyname(char *name) {
	struct ncp_nlsdesc *nd;

	for (nd = ncp_nlslist; nd->name; nd++) {
		if (strcmp(nd->name, name) != 0) continue;
		ncp_nls.u2n = nd->nls.u2n;
		ncp_nls.n2u = nd->nls.n2u;
		return 0;
	}
	fprintf(stderr, "Character conversion scheme %s was not compiled in\n", name);
	return EINVAL;
}

char *
ncp_nls_str_n2u(char *dst, const char *src) {
	char *p;

	if (ncp_nls.n2u == NULL) {
		return strcpy(dst, src);
	}
	p = dst;
	while (*src)
		*p++ = ncp_nls.n2u[(u_char)*(src++)];
	*p = 0;
	return dst;
}

char *
ncp_nls_str_u2n(char *dst, const char *src) {
	char *p;

	if (ncp_nls.u2n == NULL) {
		return strcpy(dst, src);
	}
	p = dst;
	while (*src)
		*p++ = ncp_nls.u2n[(u_char)*(src++)];
	*p = 0;
	return dst;
}

char *
ncp_nls_mem_n2u(char *dst, const char *src, int size) {
	char *p;

	if (size == 0) return NULL;
	if (ncp_nls.n2u == NULL) {
		return memcpy(dst, src, size);
	}
	for(p = dst; size; size--, p++)
		*p = ncp_nls.n2u[(u_char)*(src++)];
	return dst;
}

char *
ncp_nls_mem_u2n(char *dst, const char *src, int size) {
	char *p;

	if (size == 0) return NULL;
	if (ncp_nls.u2n == NULL) {
		return strcpy(dst, src);
	}
	for(p = dst; size; size--, p++)
		*p = ncp_nls.u2n[(u_char)*(src++)];
	return dst;
}

char *
ncp_str_upper(char *s) {
	char *p = s;
	while (*s) {
		*s = toupper(*s);
		s++;
	}
	return p;
}
