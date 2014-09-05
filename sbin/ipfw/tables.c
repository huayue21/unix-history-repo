/*
 * Copyright (c) 2002-2003 Luigi Rizzo
 * Copyright (c) 1996 Alex Nash, Paul Traina, Poul-Henning Kamp
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 *
 * Idea and grammar partially left from:
 * Copyright (c) 1993 Daniel Boulet
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * in-kernel tables support
 *
 * $FreeBSD: projects/ipfw/sbin/ipfw/ipfw2.c 267467 2014-06-14 10:58:39Z melifaro $
 */


#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <arpa/inet.h>

#include "ipfw2.h"

static void table_list(ipfw_xtable_info *i, int need_header);
static void table_modify_record(ipfw_obj_header *oh, int ac, char *av[],
    int add, int quiet, int update, int atomic);
static int table_flush(ipfw_obj_header *oh);
static int table_destroy(ipfw_obj_header *oh);
static int table_do_create(ipfw_obj_header *oh, ipfw_xtable_info *i);
static int table_do_modify(ipfw_obj_header *oh, ipfw_xtable_info *i);
static int table_do_swap(ipfw_obj_header *oh, char *second);
static void table_create(ipfw_obj_header *oh, int ac, char *av[]);
static void table_modify(ipfw_obj_header *oh, int ac, char *av[]);
static void table_lookup(ipfw_obj_header *oh, int ac, char *av[]);
static void table_lock(ipfw_obj_header *oh, int lock);
static int table_swap(ipfw_obj_header *oh, char *second);
static int table_get_info(ipfw_obj_header *oh, ipfw_xtable_info *i);
static int table_show_info(ipfw_xtable_info *i, void *arg);
static void table_fill_ntlv(ipfw_obj_ntlv *ntlv, char *name, uint32_t set,
    uint16_t uidx);

static int table_flush_one(ipfw_xtable_info *i, void *arg);
static int table_show_one(ipfw_xtable_info *i, void *arg);
static int table_do_get_list(ipfw_xtable_info *i, ipfw_obj_header **poh);
static void table_show_list(ipfw_obj_header *oh, int need_header);
static void table_show_entry(ipfw_xtable_info *i, ipfw_obj_tentry *tent);

static void tentry_fill_key(ipfw_obj_header *oh, ipfw_obj_tentry *tent,
    char *key, int add, uint8_t *ptype, uint32_t *pvmask, ipfw_xtable_info *xi);
static void tentry_fill_value(ipfw_obj_header *oh, ipfw_obj_tentry *tent,
    char *arg, uint8_t type, uint32_t vmask);
static void table_show_value(char *buf, size_t bufsize, ipfw_table_value *v,
    uint32_t vmask, int print_ip);

typedef int (table_cb_t)(ipfw_xtable_info *i, void *arg);
static int tables_foreach(table_cb_t *f, void *arg, int sort);

#ifndef s6_addr32
#define s6_addr32 __u6_addr.__u6_addr32
#endif

static struct _s_x tabletypes[] = {
      { "addr",		IPFW_TABLE_ADDR },
      { "iface",	IPFW_TABLE_INTERFACE },
      { "number",	IPFW_TABLE_NUMBER },
      { "flow",		IPFW_TABLE_FLOW },
      { NULL, 0 }
};

static struct _s_x tablevaltypes[] = {
      { "skipto",	IPFW_VTYPE_SKIPTO },
      { "pipe",		IPFW_VTYPE_PIPE },
      { "fib",		IPFW_VTYPE_FIB },
      { "nat",		IPFW_VTYPE_NAT },
      { "dscp",		IPFW_VTYPE_DSCP },
      { "tag",		IPFW_VTYPE_TAG },
      { "divert",	IPFW_VTYPE_DIVERT },
      { "netgraph",	IPFW_VTYPE_NETGRAPH },
      { "limit",	IPFW_VTYPE_LIMIT },
      { "ipv4",		IPFW_VTYPE_NH4 },
      { "ipv6",		IPFW_VTYPE_NH6 },
      { NULL, 0 }
};

static struct _s_x tablecmds[] = {
      { "add",		TOK_ADD },
      { "delete",	TOK_DEL },
      { "create",	TOK_CREATE },
      { "destroy",	TOK_DESTROY },
      { "flush",	TOK_FLUSH },
      { "modify",	TOK_MODIFY },
      { "swap",		TOK_SWAP },
      { "info",		TOK_INFO },
      { "detail",	TOK_DETAIL },
      { "list",		TOK_LIST },
      { "lookup",	TOK_LOOKUP },
      { "atomic",	TOK_ATOMIC },
      { "lock",		TOK_LOCK },
      { "unlock",	TOK_UNLOCK },
      { NULL, 0 }
};

static int
lookup_host (char *host, struct in_addr *ipaddr)
{
	struct hostent *he;

	if (!inet_aton(host, ipaddr)) {
		if ((he = gethostbyname(host)) == NULL)
			return(-1);
		*ipaddr = *(struct in_addr *)he->h_addr_list[0];
	}
	return(0);
}

static int
get_token(struct _s_x *table, char *string, char *errbase)
{
	int tcmd;

	if ((tcmd = match_token_relaxed(table, string)) < 0)
		errx(EX_USAGE, "%s %s %s",
		    (tcmd == 0) ? "invalid" : "ambiguous", errbase, string);

	return (tcmd);
}

/*
 * This one handles all table-related commands
 * 	ipfw table NAME create ...
 * 	ipfw table NAME modify ...
 * 	ipfw table NAME destroy
 * 	ipfw table NAME swap NAME
 * 	ipfw table NAME lock
 * 	ipfw table NAME unlock
 * 	ipfw table NAME add addr[/masklen] [value] 
 * 	ipfw table NAME add [addr[/masklen] value] [addr[/masklen] value] ..
 * 	ipfw table NAME delete addr[/masklen] [addr[/masklen]] ..
 * 	ipfw table NAME lookup addr
 * 	ipfw table {NAME | all} flush
 * 	ipfw table {NAME | all} list
 * 	ipfw table {NAME | all} info
 * 	ipfw table {NAME | all} detail
 */
void
ipfw_table_handler(int ac, char *av[])
{
	int do_add, is_all;
	int atomic, error, tcmd;
	ipfw_xtable_info i;
	ipfw_obj_header oh;
	char *tablename;
	uint32_t set;
	void *arg;

	memset(&oh, 0, sizeof(oh));
	is_all = 0;
	if (co.use_set != 0)
		set = co.use_set - 1;
	else
		set = 0;

	ac--; av++;
	NEED1("table needs name");
	tablename = *av;

	if (table_check_name(tablename) == 0) {
		table_fill_ntlv(&oh.ntlv, *av, set, 1);
		oh.idx = 1;
	} else {
		if (strcmp(tablename, "all") == 0)
			is_all = 1;
		else
			errx(EX_USAGE, "table name %s is invalid", tablename);
	}
	ac--; av++;
	NEED1("table needs command");

	tcmd = get_token(tablecmds, *av, "table command");
	/* Check if atomic operation was requested */
	atomic = 0;
	if (tcmd == TOK_ATOMIC) {
		ac--; av++;
		NEED1("atomic needs command");
		tcmd = get_token(tablecmds, *av, "table command");
		switch (tcmd) {
		case TOK_ADD:
			break;
		default:
			errx(EX_USAGE, "atomic is not compatible with %s", *av);
		}
		atomic = 1;
	}

	switch (tcmd) {
	case TOK_LIST:
	case TOK_INFO:
	case TOK_DETAIL:
	case TOK_FLUSH:
		break;
	default:
		if (is_all != 0)
			errx(EX_USAGE, "table name required");
	}

	switch (tcmd) {
	case TOK_ADD:
	case TOK_DEL:
		do_add = **av == 'a';
		ac--; av++;
		table_modify_record(&oh, ac, av, do_add, co.do_quiet,
		    co.do_quiet, atomic);
		break;
	case TOK_CREATE:
		ac--; av++;
		table_create(&oh, ac, av);
		break;
	case TOK_MODIFY:
		ac--; av++;
		table_modify(&oh, ac, av);
		break;
	case TOK_DESTROY:
		if (table_destroy(&oh) != 0)
			err(EX_OSERR, "failed to destroy table %s", tablename);
		break;
	case TOK_FLUSH:
		if (is_all == 0) {
			if ((error = table_flush(&oh)) != 0)
				err(EX_OSERR, "failed to flush table %s info",
				    tablename);
		} else {
			error = tables_foreach(table_flush_one, &oh, 1);
			if (error != 0)
				err(EX_OSERR, "failed to flush tables list");
		}
		break;
	case TOK_SWAP:
		ac--; av++;
		NEED1("second table name required");
		table_swap(&oh, *av);
		break;
	case TOK_LOCK:
	case TOK_UNLOCK:
		table_lock(&oh, (tcmd == TOK_LOCK));
		break;
	case TOK_DETAIL:
	case TOK_INFO:
		arg = (tcmd == TOK_DETAIL) ? (void *)1 : NULL;
		if (is_all == 0) {
			if ((error = table_get_info(&oh, &i)) != 0)
				err(EX_OSERR, "failed to request table info");
			table_show_info(&i, arg);
		} else {
			error = tables_foreach(table_show_info, arg, 1);
			if (error != 0)
				err(EX_OSERR, "failed to request tables list");
		}
		break;
	case TOK_LIST:
		if (is_all == 0) {
			ipfw_xtable_info i;
			if ((error = table_get_info(&oh, &i)) != 0)
				err(EX_OSERR, "failed to request table info");
			table_show_one(&i, NULL);
		} else {
			error = tables_foreach(table_show_one, NULL, 1);
			if (error != 0)
				err(EX_OSERR, "failed to request tables list");
		}
		break;
	case TOK_LOOKUP:
		ac--; av++;
		table_lookup(&oh, ac, av);
		break;
	}
}

static void
table_fill_ntlv(ipfw_obj_ntlv *ntlv, char *name, uint32_t set, uint16_t uidx)
{

	ntlv->head.type = IPFW_TLV_TBL_NAME;
	ntlv->head.length = sizeof(ipfw_obj_ntlv);
	ntlv->idx = uidx;
	ntlv->set = set;
	strlcpy(ntlv->name, name, sizeof(ntlv->name));
}

static void
table_fill_objheader(ipfw_obj_header *oh, ipfw_xtable_info *i)
{

	oh->idx = 1;
	table_fill_ntlv(&oh->ntlv, i->tablename, i->set, 1);
}

static struct _s_x tablenewcmds[] = {
      { "type",		TOK_TYPE },
      { "valtype",	TOK_VALTYPE },
      { "algo",		TOK_ALGO },
      { "limit",	TOK_LIMIT },
      { "locked",	TOK_LOCK },
      { NULL, 0 }
};

static struct _s_x flowtypecmds[] = {
      { "src-ip",	IPFW_TFFLAG_SRCIP },
      { "proto",	IPFW_TFFLAG_PROTO },
      { "src-port",	IPFW_TFFLAG_SRCPORT },
      { "dst-ip",	IPFW_TFFLAG_DSTIP },
      { "dst-port",	IPFW_TFFLAG_DSTPORT },
      { NULL, 0 }
};

int
table_parse_type(uint8_t ttype, char *p, uint8_t *tflags)
{
	uint32_t fset, fclear;
	char *e;

	/* Parse type options */
	switch(ttype) {
	case IPFW_TABLE_FLOW:
		fset = fclear = 0;
		if (fill_flags(flowtypecmds, p, &e, &fset, &fclear) != 0)
			errx(EX_USAGE,
			    "unable to parse flow option %s", e);
		*tflags = fset;
		break;
	default:
		return (EX_USAGE);
	}

	return (0);
}

void
table_print_type(char *tbuf, size_t size, uint8_t type, uint8_t tflags)
{
	const char *tname;
	int l;

	if ((tname = match_value(tabletypes, type)) == NULL)
		tname = "unknown";

	l = snprintf(tbuf, size, "%s", tname);
	tbuf += l;
	size -= l;

	switch(type) {
	case IPFW_TABLE_FLOW:
		if (tflags != 0) {
			*tbuf++ = ':';
			l--;
			print_flags_buffer(tbuf, size, flowtypecmds, tflags);
		}
		break;
	}
}

/*
 * Creates new table
 *
 * ipfw table NAME create [ type { addr | iface | number | flow } ]
 *     [ algo algoname ]
 */
static void
table_create(ipfw_obj_header *oh, int ac, char *av[])
{
	ipfw_xtable_info xi;
	int error, tcmd, val;
	uint32_t fset, fclear;
	size_t sz;
	char *e, *p;
	char tbuf[128];

	sz = sizeof(tbuf);
	memset(&xi, 0, sizeof(xi));

	while (ac > 0) {
		tcmd = get_token(tablenewcmds, *av, "option");
		ac--; av++;

		switch (tcmd) {
		case TOK_LIMIT:
			NEED1("limit value required");
			xi.limit = strtol(*av, NULL, 10);
			ac--; av++;
			break;
		case TOK_TYPE:
			NEED1("table type required");
			/* Type may have suboptions after ':' */
			if ((p = strchr(*av, ':')) != NULL)
				*p++ = '\0';
			val = match_token(tabletypes, *av);
			if (val == -1) {
				concat_tokens(tbuf, sizeof(tbuf), tabletypes,
				    ", ");
				errx(EX_USAGE,
				    "Unknown tabletype: %s. Supported: %s",
				    *av, tbuf);
			}
			xi.type = val;
			if (p != NULL) {
				error = table_parse_type(val, p, &xi.tflags);
				if (error != 0)
					errx(EX_USAGE,
					    "Unsupported suboptions: %s", p);
			}
			ac--; av++;
			break;
		case TOK_VALTYPE:
			NEED1("table value type required");
			fset = fclear = 0;
			val = fill_flags(tablevaltypes, *av, &e, &fset, &fclear);
			if (val != -1) {
				xi.vmask = fset;
				ac--; av++;
				break;
			}
			concat_tokens(tbuf, sizeof(tbuf), tablevaltypes, ", ");
			errx(EX_USAGE, "Unknown value type: %s. Supported: %s",
			    e, tbuf);
			break;
		case TOK_ALGO:
			NEED1("table algorithm name required");
			if (strlen(*av) > sizeof(xi.algoname))
				errx(EX_USAGE, "algorithm name too long");
			strlcpy(xi.algoname, *av, sizeof(xi.algoname));
			ac--; av++;
			break;
		case TOK_LOCK:
			xi.flags |= IPFW_TGFLAGS_LOCKED;
			break;
		}
	}

	/* Set some defaults to preserve compability */
	if (xi.algoname[0] == '\0' && xi.type == 0)
		xi.type = IPFW_TABLE_ADDR;
	if (xi.vmask == 0)
		xi.vmask = IPFW_VTYPE_LEGACY;

	if ((error = table_do_create(oh, &xi)) != 0)
		err(EX_OSERR, "Table creation failed");
}

/*
 * Creates new table
 *
 * Request: [ ipfw_obj_header ipfw_xtable_info ]
 *
 * Returns 0 on success.
 */
static int
table_do_create(ipfw_obj_header *oh, ipfw_xtable_info *i)
{
	char tbuf[sizeof(ipfw_obj_header) + sizeof(ipfw_xtable_info)];
	int error;

	memcpy(tbuf, oh, sizeof(*oh));
	memcpy(tbuf + sizeof(*oh), i, sizeof(*i));
	oh = (ipfw_obj_header *)tbuf;

	error = do_set3(IP_FW_TABLE_XCREATE, &oh->opheader, sizeof(tbuf));

	return (error);
}

/*
 * Modifies existing table
 *
 * ipfw table NAME modify [ limit number ]
 */
static void
table_modify(ipfw_obj_header *oh, int ac, char *av[])
{
	ipfw_xtable_info xi;
	int tcmd;
	size_t sz;
	char tbuf[128];

	sz = sizeof(tbuf);
	memset(&xi, 0, sizeof(xi));

	while (ac > 0) {
		tcmd = get_token(tablenewcmds, *av, "option");
		ac--; av++;

		switch (tcmd) {
		case TOK_LIMIT:
			NEED1("limit value required");
			xi.limit = strtol(*av, NULL, 10);
			xi.mflags |= IPFW_TMFLAGS_LIMIT;
			ac--; av++;
			break;
		default:
			errx(EX_USAGE, "cmd is not supported for modificatiob");
		}
	}

	if (table_do_modify(oh, &xi) != 0)
		err(EX_OSERR, "Table modification failed");
}

/*
 * Modifies existing table.
 *
 * Request: [ ipfw_obj_header ipfw_xtable_info ]
 *
 * Returns 0 on success.
 */
static int
table_do_modify(ipfw_obj_header *oh, ipfw_xtable_info *i)
{
	char tbuf[sizeof(ipfw_obj_header) + sizeof(ipfw_xtable_info)];
	int error;

	memcpy(tbuf, oh, sizeof(*oh));
	memcpy(tbuf + sizeof(*oh), i, sizeof(*i));
	oh = (ipfw_obj_header *)tbuf;

	error = do_set3(IP_FW_TABLE_XMODIFY, &oh->opheader, sizeof(tbuf));

	return (error);
}

/*
 * Locks or unlocks given table
 */
static void
table_lock(ipfw_obj_header *oh, int lock)
{
	ipfw_xtable_info xi;

	memset(&xi, 0, sizeof(xi));

	xi.mflags |= IPFW_TMFLAGS_LOCK;
	xi.flags |= (lock != 0) ? IPFW_TGFLAGS_LOCKED : 0;

	if (table_do_modify(oh, &xi) != 0)
		err(EX_OSERR, "Table %s failed", lock != 0 ? "lock" : "unlock");
}

/*
 * Destroys given table specified by @oh->ntlv.
 * Returns 0 on success.
 */
static int
table_destroy(ipfw_obj_header *oh)
{

	if (do_set3(IP_FW_TABLE_XDESTROY, &oh->opheader, sizeof(*oh)) != 0)
		return (-1);

	return (0);
}

/*
 * Flushes given table specified by @oh->ntlv.
 * Returns 0 on success.
 */
static int
table_flush(ipfw_obj_header *oh)
{

	if (do_set3(IP_FW_TABLE_XFLUSH, &oh->opheader, sizeof(*oh)) != 0)
		return (-1);

	return (0);
}

static int
table_do_swap(ipfw_obj_header *oh, char *second)
{
	char tbuf[sizeof(ipfw_obj_header) + sizeof(ipfw_obj_ntlv)];
	int error;

	memset(tbuf, 0, sizeof(tbuf));
	memcpy(tbuf, oh, sizeof(*oh));
	oh = (ipfw_obj_header *)tbuf;
	table_fill_ntlv((ipfw_obj_ntlv *)(oh + 1), second, oh->ntlv.set, 1);

	error = do_set3(IP_FW_TABLE_XSWAP, &oh->opheader, sizeof(tbuf));

	return (error);
}

/*
 * Swaps given table with @second one.
 */
static int
table_swap(ipfw_obj_header *oh, char *second)
{
	int error;

	if (table_check_name(second) != 0)
		errx(EX_USAGE, "table name %s is invalid", second);

	error = table_do_swap(oh, second);

	switch (error) {
	case EINVAL:
		errx(EX_USAGE, "Unable to swap table: check types");
	case EFBIG:
		errx(EX_USAGE, "Unable to swap table: check limits");
	}

	return (0);
}


/*
 * Retrieves table in given table specified by @oh->ntlv.
 * it inside @i.
 * Returns 0 on success.
 */
static int
table_get_info(ipfw_obj_header *oh, ipfw_xtable_info *i)
{
	char tbuf[sizeof(ipfw_obj_header) + sizeof(ipfw_xtable_info)];
	size_t sz;

	sz = sizeof(tbuf);
	memset(tbuf, 0, sizeof(tbuf));
	memcpy(tbuf, oh, sizeof(*oh));
	oh = (ipfw_obj_header *)tbuf;

	if (do_get3(IP_FW_TABLE_XINFO, &oh->opheader, &sz) != 0)
		return (errno);

	if (sz < sizeof(tbuf))
		return (EINVAL);

	*i = *(ipfw_xtable_info *)(oh + 1);

	return (0);
}

static struct _s_x tablealgoclass[] = {
      { "hash",		IPFW_TACLASS_HASH },
      { "array",	IPFW_TACLASS_ARRAY },
      { "radix",	IPFW_TACLASS_RADIX },
      { NULL, 0 }
};

struct ta_cldata {
	uint8_t		taclass;
	uint8_t		spare4;
	uint16_t	itemsize;
	uint16_t	itemsize6;
	uint32_t	size;	
	uint32_t	count;
};

/*
 * Print global/per-AF table @i algorithm info.
 */
static void
table_show_tainfo(ipfw_xtable_info *i, struct ta_cldata *d,
    const char *af, const char *taclass)
{

	switch (d->taclass) {
	case IPFW_TACLASS_HASH:
	case IPFW_TACLASS_ARRAY:
		printf(" %salgorithm %s info\n", af, taclass);
		if (d->itemsize == d->itemsize6)
			printf("  size: %u items: %u itemsize: %u\n",
			    d->size, d->count, d->itemsize);
		else
			printf("  size: %u items: %u "
			    "itemsize4: %u itemsize6: %u\n",
			    d->size, d->count,
			    d->itemsize, d->itemsize6);
		break;
	case IPFW_TACLASS_RADIX:
		printf(" %salgorithm %s info\n", af, taclass);
		if (d->itemsize == d->itemsize6)
			printf("  items: %u itemsize: %u\n",
			    d->count, d->itemsize);
		else
			printf("  items: %u "
			    "itemsize4: %u itemsize6: %u\n",
			    d->count, d->itemsize, d->itemsize6);
		break;
	default:
		printf(" algo class: %s\n", taclass);
	}
}

static void
table_print_valheader(char *buf, size_t bufsize, uint32_t vmask)
{

	if (vmask == IPFW_VTYPE_LEGACY) {
		snprintf(buf, bufsize, "legacy");
		return;
	}

	print_flags_buffer(buf, bufsize, tablevaltypes, vmask);
}

/*
 * Prints table info struct @i in human-readable form.
 */
static int
table_show_info(ipfw_xtable_info *i, void *arg)
{
	const char *vtype;
	ipfw_ta_tinfo *tainfo;
	int afdata, afitem;
	struct ta_cldata d;
	char ttype[64], tvtype[64];

	table_print_type(ttype, sizeof(ttype), i->type, i->tflags);
	table_print_valheader(tvtype, sizeof(tvtype), i->vmask);

	printf("--- table(%s), set(%u) ---\n", i->tablename, i->set);
	if ((i->flags & IPFW_TGFLAGS_LOCKED) != 0)
		printf(" kindex: %d, type: %s, locked\n", i->kidx, ttype);
	else
		printf(" kindex: %d, type: %s\n", i->kidx, ttype);
	printf(" references: %u, valtype: %s\n", i->refcnt, tvtype);
	printf(" algorithm: %s\n", i->algoname);
	printf(" items: %u, size: %u\n", i->count, i->size);
	if (i->limit > 0)
		printf(" limit: %u\n", i->limit);

	/* Print algo-specific info if requested & set  */
	if (arg == NULL)
		return (0);

	if ((i->ta_info.flags & IPFW_TATFLAGS_DATA) == 0)
		return (0);
	tainfo = &i->ta_info;

	afdata = 0;
	afitem = 0;
	if (tainfo->flags & IPFW_TATFLAGS_AFDATA)
		afdata = 1;
	if (tainfo->flags & IPFW_TATFLAGS_AFITEM)
		afitem = 1;

	memset(&d, 0, sizeof(d));
	d.taclass = tainfo->taclass4;
	d.size = tainfo->size4;
	d.count = tainfo->count4;
	d.itemsize = tainfo->itemsize4;
	if (afdata == 0 && afitem != 0)
		d.itemsize6 = tainfo->itemsize6;
	else
		d.itemsize6 = d.itemsize;
	if ((vtype = match_value(tablealgoclass, d.taclass)) == NULL)
		vtype = "unknown";

	if (afdata == 0) {
		table_show_tainfo(i, &d, "", vtype);
	} else {
		table_show_tainfo(i, &d, "IPv4 ", vtype);
		memset(&d, 0, sizeof(d));
		d.taclass = tainfo->taclass6;
		if ((vtype = match_value(tablealgoclass, d.taclass)) == NULL)
			vtype = "unknown";
		d.size = tainfo->size6;
		d.count = tainfo->count6;
		d.itemsize = tainfo->itemsize6;
		d.itemsize6 = d.itemsize;
		table_show_tainfo(i, &d, "IPv6 ", vtype);
	}

	return (0);
}


/*
 * Function wrappers which can be used either
 * as is or as foreach function parameter.
 */

static int
table_show_one(ipfw_xtable_info *i, void *arg)
{
	ipfw_obj_header *oh;
	int error;

	if ((error = table_do_get_list(i, &oh)) != 0) {
		err(EX_OSERR, "Error requesting table %s list", i->tablename);
		return (error);
	}

	table_show_list(oh, 1);

	free(oh);
	return (0);	
}

static int
table_flush_one(ipfw_xtable_info *i, void *arg)
{
	ipfw_obj_header *oh;

	oh = (ipfw_obj_header *)arg;

	table_fill_ntlv(&oh->ntlv, i->tablename, i->set, 1);

	return (table_flush(oh));
}

static int
table_do_modify_record(int cmd, ipfw_obj_header *oh,
    ipfw_obj_tentry *tent, int count, int atomic)
{
	ipfw_obj_ctlv *ctlv;
	ipfw_obj_tentry *tent_base;
	caddr_t pbuf;
	char xbuf[sizeof(*oh) + sizeof(ipfw_obj_ctlv) + sizeof(*tent)];
	int error, i;
	size_t sz;

	sz = sizeof(*ctlv) + sizeof(*tent) * count;
	if (count == 1) {
		memset(xbuf, 0, sizeof(xbuf));
		pbuf = xbuf;
	} else {
		if ((pbuf = calloc(1, sizeof(*oh) + sz)) == NULL)
			return (ENOMEM);
	}

	memcpy(pbuf, oh, sizeof(*oh));
	oh = (ipfw_obj_header *)pbuf;
	oh->opheader.version = 1;

	ctlv = (ipfw_obj_ctlv *)(oh + 1);
	ctlv->count = count;
	ctlv->head.length = sz;
	if (atomic != 0)
		ctlv->flags |= IPFW_CTF_ATOMIC;

	tent_base = tent;
	memcpy(ctlv + 1, tent, sizeof(*tent) * count);
	tent = (ipfw_obj_tentry *)(ctlv + 1);
	for (i = 0; i < count; i++, tent++) {
		tent->head.length = sizeof(ipfw_obj_tentry);
		tent->idx = oh->idx;
	}

	sz += sizeof(*oh);
	error = do_get3(cmd, &oh->opheader, &sz);
	tent = (ipfw_obj_tentry *)(ctlv + 1);
	/* Copy result back to provided buffer */
	memcpy(tent_base, ctlv + 1, sizeof(*tent) * count);

	if (pbuf != xbuf)
		free(pbuf);

	return (error);
}

static void
table_modify_record(ipfw_obj_header *oh, int ac, char *av[], int add,
    int quiet, int update, int atomic)
{
	ipfw_obj_tentry *ptent, tent, *tent_buf;
	ipfw_xtable_info xi;
	uint8_t type;
	uint32_t vmask;
	int cmd, count, error, i, ignored;
	char *texterr, *etxt, *px;

	if (ac == 0)
		errx(EX_USAGE, "address required");
	
	if (add != 0) {
		cmd = IP_FW_TABLE_XADD;
		texterr = "Adding record failed";
	} else {
		cmd = IP_FW_TABLE_XDEL;
		texterr = "Deleting record failed";
	}

	/*
	 * Calculate number of entries:
	 * Assume [key val] x N for add
	 * and
	 * key x N for delete
	 */
	count = (add != 0) ? ac / 2 + 1 : ac;

	if (count <= 1) {
		/* Adding single entry with/without value */
		memset(&tent, 0, sizeof(tent));
		tent_buf = &tent;
	} else {
		
		if ((tent_buf = calloc(count, sizeof(tent))) == NULL)
			errx(EX_OSERR,
			    "Unable to allocate memory for all entries");
	}
	ptent = tent_buf;

	memset(&xi, 0, sizeof(xi));
	count = 0;
	while (ac > 0) {
		tentry_fill_key(oh, ptent, *av, add, &type, &vmask, &xi);

		/*
		 * compability layer: auto-create table if not exists
		 */
		if (xi.tablename[0] == '\0') {
			xi.type = type;
			xi.vmask = vmask;
			strlcpy(xi.tablename, oh->ntlv.name,
			    sizeof(xi.tablename));
			fprintf(stderr, "DEPRECATED: inserting data info "
			    "non-existent table %s. (auto-created)\n",
			    xi.tablename);
			table_do_create(oh, &xi);
		}
	
		oh->ntlv.type = type;
		ac--; av++;
	
		if (add != 0 && ac > 0) {
			tentry_fill_value(oh, ptent, *av, type, vmask);
			ac--; av++;
		}

		if (update != 0)
			ptent->head.flags |= IPFW_TF_UPDATE;

		count++;
		ptent++;
	}

	error = table_do_modify_record(cmd, oh, tent_buf, count, atomic);

	quiet = 0;

	/*
	 * Compatibility stuff: do not yell on duplicate keys or
	 * failed deletions.
	 */
	if (error == 0 || (error == EEXIST && add != 0) ||
	    (error == ENOENT && add == 0)) {
		if (quiet != 0) {
			if (tent_buf != &tent)
				free(tent_buf);
			return;
		}
	}

	/* Report results back */
	ptent = tent_buf;
	for (i = 0; i < count; ptent++, i++) {
		ignored = 0;
		switch (ptent->result) {
		case IPFW_TR_ADDED:
			px = "added";
			break;
		case IPFW_TR_DELETED:
			px = "deleted";
			break;
		case IPFW_TR_UPDATED:
			px = "updated";
			break;
		case IPFW_TR_LIMIT:
			px = "limit";
			ignored = 1;
			break;
		case IPFW_TR_ERROR:
			px = "error";
			ignored = 1;
			break;
		case IPFW_TR_NOTFOUND:
			px = "notfound";
			ignored = 1;
			break;
		case IPFW_TR_EXISTS:
			px = "exists";
			ignored = 1;
			break;
		case IPFW_TR_IGNORED:
			px = "ignored";
			ignored = 1;
			break;
		default:
			px = "unknown";
			ignored = 1;
		}

		if (error != 0 && atomic != 0 && ignored == 0)
			printf("%s(reverted): ", px);
		else
			printf("%s: ", px);

		table_show_entry(&xi, ptent);
	}

	if (tent_buf != &tent)
		free(tent_buf);

	if (error == 0)
		return;

	/* Try to provide more human-readable error */
	switch (error) {
	case EEXIST:
		etxt = "record already exists";
		break;
	case EFBIG:
		etxt = "limit hit";
		break;
	case ESRCH:
		etxt = "table not found";
		break;
	case ENOENT:
		etxt = "record not found";
		break;
	case EACCES:
		etxt = "table is locked";
		break;
	default:
		etxt = strerror(error);
	}

	errx(EX_OSERR, "%s: %s", texterr, etxt);
}

static int
table_do_lookup(ipfw_obj_header *oh, char *key, ipfw_xtable_info *xi,
    ipfw_obj_tentry *xtent)
{
	char xbuf[sizeof(ipfw_obj_header) + sizeof(ipfw_obj_tentry)];
	ipfw_obj_tentry *tent;
	uint8_t type;
	uint32_t vmask;
	size_t sz;

	memcpy(xbuf, oh, sizeof(*oh));
	oh = (ipfw_obj_header *)xbuf;
	tent = (ipfw_obj_tentry *)(oh + 1);

	memset(tent, 0, sizeof(*tent));
	tent->head.length = sizeof(*tent);
	tent->idx = 1;

	tentry_fill_key(oh, tent, key, 0, &type, &vmask, xi);
	oh->ntlv.type = type;

	sz = sizeof(xbuf);
	if (do_get3(IP_FW_TABLE_XFIND, &oh->opheader, &sz) != 0)
		return (errno);

	if (sz < sizeof(xbuf))
		return (EINVAL);

	*xtent = *tent;

	return (0);
}

static void
table_lookup(ipfw_obj_header *oh, int ac, char *av[])
{
	ipfw_obj_tentry xtent;
	ipfw_xtable_info xi;
	char key[64];
	int error;

	if (ac == 0)
		errx(EX_USAGE, "address required");

	strlcpy(key, *av, sizeof(key));

	memset(&xi, 0, sizeof(xi));
	error = table_do_lookup(oh, key, &xi, &xtent);

	switch (error) {
	case 0:
		break;
	case ESRCH:
		errx(EX_UNAVAILABLE, "Table %s not found", oh->ntlv.name);
	case ENOENT:
		errx(EX_UNAVAILABLE, "Entry %s not found", *av);
	case ENOTSUP:
		errx(EX_UNAVAILABLE, "Table %s algo does not support "
		    "\"lookup\" method", oh->ntlv.name);
	default:
		err(EX_OSERR, "getsockopt(IP_FW_TABLE_XFIND)");
	}

	table_show_entry(&xi, &xtent);
}

static void
tentry_fill_key_type(char *arg, ipfw_obj_tentry *tentry, uint8_t type,
    uint8_t tflags)
{
	char *p, *pp;
	int mask, af;
	struct in6_addr *paddr, tmp;
	struct tflow_entry *tfe;
	uint32_t key, *pkey;
	uint16_t port;
	struct protoent *pent;
	struct servent *sent;
	int masklen;

	masklen = 0;
	af = 0;
	paddr = (struct in6_addr *)&tentry->k;

	switch (type) {
	case IPFW_TABLE_ADDR:
		/* Remove / if exists */
		if ((p = strchr(arg, '/')) != NULL) {
			*p = '\0';
			mask = atoi(p + 1);
		}

		if (inet_pton(AF_INET, arg, paddr) == 1) {
			if (p != NULL && mask > 32)
				errx(EX_DATAERR, "bad IPv4 mask width: %s",
				    p + 1);

			masklen = p ? mask : 32;
			af = AF_INET;
		} else if (inet_pton(AF_INET6, arg, paddr) == 1) {
			if (IN6_IS_ADDR_V4COMPAT(paddr))
				errx(EX_DATAERR,
				    "Use IPv4 instead of v4-compatible");
			if (p != NULL && mask > 128)
				errx(EX_DATAERR, "bad IPv6 mask width: %s",
				    p + 1);

			masklen = p ? mask : 128;
			af = AF_INET6;
		} else {
			/* Assume FQDN */
			if (lookup_host(arg, (struct in_addr *)paddr) != 0)
				errx(EX_NOHOST, "hostname ``%s'' unknown", arg);

			masklen = 32;
			type = IPFW_TABLE_ADDR;
			af = AF_INET;
		}
		break;
	case IPFW_TABLE_INTERFACE:
		/* Assume interface name. Copy significant data only */
		mask = MIN(strlen(arg), IF_NAMESIZE - 1);
		memcpy(paddr, arg, mask);
		/* Set mask to exact match */
		masklen = 8 * IF_NAMESIZE;
		break;
	case IPFW_TABLE_NUMBER:
		/* Port or any other key */
		key = strtol(arg, &p, 10);
		if (*p != '\0')
			errx(EX_DATAERR, "Invalid number: %s", arg);

		pkey = (uint32_t *)paddr;
		*pkey = key;
		masklen = 32;
		break;
	case IPFW_TABLE_FLOW:
		/* Assume [src-ip][,proto][,src-port][,dst-ip][,dst-port] */
		tfe = &tentry->k.flow;
		af = 0;

		/* Handle <ipv4|ipv6> */
		if ((tflags & IPFW_TFFLAG_SRCIP) != 0) {
			if ((p = strchr(arg, ',')) != NULL)
				*p++ = '\0';
			/* Determine family using temporary storage */
			if (inet_pton(AF_INET, arg, &tmp) == 1) {
				if (af != 0 && af != AF_INET)
					errx(EX_DATAERR,
					    "Inconsistent address family\n");
				af = AF_INET;
				memcpy(&tfe->a.a4.sip, &tmp, 4);
			} else if (inet_pton(AF_INET6, arg, &tmp) == 1) {
				if (af != 0 && af != AF_INET6)
					errx(EX_DATAERR,
					    "Inconsistent address family\n");
				af = AF_INET6;
				memcpy(&tfe->a.a6.sip6, &tmp, 16);
			}

			arg = p;
		}

		/* Handle <proto-num|proto-name> */
		if ((tflags & IPFW_TFFLAG_PROTO) != 0) {
			if (arg == NULL)
				errx(EX_DATAERR, "invalid key: proto missing");
			if ((p = strchr(arg, ',')) != NULL)
				*p++ = '\0';

			key = strtol(arg, &pp, 10);
			if (*pp != '\0') {
				if ((pent = getprotobyname(arg)) == NULL)
					errx(EX_DATAERR, "Unknown proto: %s",
					    arg);
				else
					key = pent->p_proto;
			}
			
			if (key > 255)
				errx(EX_DATAERR, "Bad protocol number: %u",key);

			tfe->proto = key;

			arg = p;
		}

		/* Handle <port-num|service-name> */
		if ((tflags & IPFW_TFFLAG_SRCPORT) != 0) {
			if (arg == NULL)
				errx(EX_DATAERR, "invalid key: src port missing");
			if ((p = strchr(arg, ',')) != NULL)
				*p++ = '\0';

			if ((port = htons(strtol(arg, NULL, 10))) == 0) {
				if ((sent = getservbyname(arg, NULL)) == NULL)
					errx(EX_DATAERR, "Unknown service: %s",
					    arg);
				else
					key = sent->s_port;
			}
			
			tfe->sport = port;

			arg = p;
		}

		/* Handle <ipv4|ipv6>*/
		if ((tflags & IPFW_TFFLAG_DSTIP) != 0) {
			if (arg == NULL)
				errx(EX_DATAERR, "invalid key: dst ip missing");
			if ((p = strchr(arg, ',')) != NULL)
				*p++ = '\0';
			/* Determine family using temporary storage */
			if (inet_pton(AF_INET, arg, &tmp) == 1) {
				if (af != 0 && af != AF_INET)
					errx(EX_DATAERR,
					    "Inconsistent address family");
				af = AF_INET;
				memcpy(&tfe->a.a4.dip, &tmp, 4);
			} else if (inet_pton(AF_INET6, arg, &tmp) == 1) {
				if (af != 0 && af != AF_INET6)
					errx(EX_DATAERR,
					    "Inconsistent address family");
				af = AF_INET6;
				memcpy(&tfe->a.a6.dip6, &tmp, 16);
			}

			arg = p;
		}

		/* Handle <port-num|service-name> */
		if ((tflags & IPFW_TFFLAG_DSTPORT) != 0) {
			if (arg == NULL)
				errx(EX_DATAERR, "invalid key: dst port missing");
			if ((p = strchr(arg, ',')) != NULL)
				*p++ = '\0';

			if ((port = htons(strtol(arg, NULL, 10))) == 0) {
				if ((sent = getservbyname(arg, NULL)) == NULL)
					errx(EX_DATAERR, "Unknown service: %s",
					    arg);
				else
					key = sent->s_port;
			}
			
			tfe->dport = port;

			arg = p;
		}

		tfe->af = af;

		break;
	
	default:
		errx(EX_DATAERR, "Unsupported table type: %d", type);
	}

	tentry->subtype = af;
	tentry->masklen = masklen;
}

static void
tentry_fill_key(ipfw_obj_header *oh, ipfw_obj_tentry *tent, char *key,
    int add, uint8_t *ptype, uint32_t *pvmask, ipfw_xtable_info *xi)
{
	uint8_t type, tflags;
	uint32_t vmask;
	int error;
	char *del;

	type = 0;
	tflags = 0;
	vmask = 0;

	if (xi->tablename[0] == '\0')
		error = table_get_info(oh, xi);
	else
		error = 0;

	if (error == 0) {
		/* Table found. */
		type = xi->type;
		tflags = xi->tflags;
		vmask = xi->vmask;
	} else {
		if (error != ESRCH)
			errx(EX_OSERR, "Error requesting table %s info",
			    oh->ntlv.name);
		if (add == 0)
			errx(EX_DATAERR, "Table %s does not exist",
			    oh->ntlv.name);
		/*
		 * Table does not exist.
		 * Compability layer: try to interpret data as ADDR
		 * before failing.
		 */
		if ((del = strchr(key, '/')) != NULL)
			*del = '\0';
		if (inet_pton(AF_INET, key, &tent->k.addr6) == 1 ||
		    inet_pton(AF_INET6, key, &tent->k.addr6) == 1) {
			/* OK Prepare and send */
			type = IPFW_TABLE_ADDR;
			vmask = IPFW_VTYPE_LEGACY;
		} else {
			/* Inknown key */
			errx(EX_USAGE, "Table %s does not exist, cannot guess "
			    "key '%s' type", oh->ntlv.name, key);
		}
		if (del != NULL)
			*del = '/';
	}

	tentry_fill_key_type(key, tent, type, tflags);

	*ptype = type;
	*pvmask = vmask;
}

static void
set_legacy_value(uint32_t val, ipfw_table_value *v)
{
	v->tag = val;
	v->pipe = val;
	v->divert = val;
	v->skipto = val;
	v->netgraph = val;
	v->fib = val;
	v->nat = val;
	v->nh4 = val;
	v->dscp = (uint8_t)val;
	v->limit = val;
}

static void
tentry_fill_value(ipfw_obj_header *oh, ipfw_obj_tentry *tent, char *arg,
    uint8_t type, uint32_t vmask)
{
	uint32_t a4, flag, val, vm;
	ipfw_table_value *v;
	uint32_t i;
	char *comma, *e, *etype, *n, *p;

	v = &tent->v.value;
	vm = vmask;

	/* Compat layer: keep old behavior for legacy value types */
	if (vmask == IPFW_VTYPE_LEGACY) {
		/* Try to interpret as number first */
		val = strtoul(arg, &p, 0);
		if (*p == '\0') {
			set_legacy_value(val, v);
			return;
		}
		if (inet_pton(AF_INET, arg, &val) == 1) {
			set_legacy_value(ntohl(val), v);
			return;
		}
		/* Try hostname */
		if (lookup_host(arg, (struct in_addr *)&val) == 0) {
			set_legacy_value(val, v);
			return;
		}
		errx(EX_OSERR, "Unable to parse value %s", arg);
	}

	/*
	 * Shorthands: handle single value if vmask consists
	 * of numbers only. e.g.:
	 * vmask = "fib,skipto" -> treat input "1" as "1,1"
	 */

	n = arg;
	etype = NULL;
	for (i = 1; i < (1 << 31); i *= 2) {
		if ((flag = (vmask & i)) == 0)
			continue;
		vmask &= ~flag;

		if ((comma = strchr(n, ',')) != NULL)
			*comma = '\0';

		switch (flag) {
		case IPFW_VTYPE_TAG:
			v->tag = strtol(n, &e, 10);
			if (*e != '\0')
				etype = "tag";
			break;
		case IPFW_VTYPE_PIPE:
			v->pipe = strtol(n, &e, 10);
			if (*e != '\0')
				etype = "pipe";
			break;
		case IPFW_VTYPE_DIVERT:
			v->divert = strtol(n, &e, 10);
			if (*e != '\0')
				etype = "divert";
			break;
		case IPFW_VTYPE_SKIPTO:
			v->skipto = strtol(n, &e, 10);
			if (*e != '\0')
				etype = "skipto";
			break;
		case IPFW_VTYPE_NETGRAPH:
			v->netgraph = strtol(n, &e, 10);
			if (*e != '\0')
				etype = "netgraph";
			break;
		case IPFW_VTYPE_FIB:
			v->fib = strtol(n, &e, 10);
			if (*e != '\0')
				etype = "fib";
			break;
		case IPFW_VTYPE_NAT:
			v->nat = strtol(n, &e, 10);
			if (*e != '\0')
				etype = "nat";
			break;
		case IPFW_VTYPE_LIMIT:
			v->limit = strtol(n, &e, 10);
			if (*e != '\0')
				etype = "limit";
			break;
		case IPFW_VTYPE_NH4:
			if (strchr(n, '.') != NULL &&
			    inet_pton(AF_INET, n, &a4) == 1) {
				v->nh4 = ntohl(a4);
				break;
			}
			if (lookup_host(n, (struct in_addr *)&v->nh4) == 0)
				break;
			etype = "ipv4";
			break;
		case IPFW_VTYPE_DSCP:
			if (isalpha(*n)) {
				if ((v->dscp = match_token(f_ipdscp, n)) != -1)
					break;
				else
					etype = "DSCP code";
			} else {
				v->dscp = strtol(n, &e, 10);
				if (v->dscp > 63 || *e != '\0')
					etype = "DSCP value";
			}
			break;
		case IPFW_VTYPE_NH6:
			if (strchr(n, ':') != NULL &&
			    inet_pton(AF_INET6, n, &v->nh6) == 1)
				break;
			etype = "ipv6";
			break;
		}

		if (etype != NULL)
			errx(EX_USAGE, "Unable to parse %s as %s", n, etype);

		if (comma != NULL)
			*comma++ = ',';

		if ((n = comma) != NULL)
			continue;

		/* End of input. */
		if (vmask != 0)
			errx(EX_USAGE, "Not enough fields inside value");
	}
}

/*
 * Compare table names.
 * Honor number comparison.
 */
static int
tablename_cmp(const void *a, const void *b)
{
	ipfw_xtable_info *ia, *ib;

	ia = (ipfw_xtable_info *)a;
	ib = (ipfw_xtable_info *)b;

	return (stringnum_cmp(ia->tablename, ib->tablename));
}

/*
 * Retrieves table list from kernel,
 * optionally sorts it and calls requested function for each table.
 * Returns 0 on success.
 */
static int
tables_foreach(table_cb_t *f, void *arg, int sort)
{
	ipfw_obj_lheader *olh;
	ipfw_xtable_info *info;
	size_t sz;
	int i, error;

	/* Start with reasonable default */
	sz = sizeof(*olh) + 16 * sizeof(ipfw_xtable_info);

	for (;;) {
		if ((olh = calloc(1, sz)) == NULL)
			return (ENOMEM);

		olh->size = sz;
		if (do_get3(IP_FW_TABLES_XLIST, &olh->opheader, &sz) != 0) {
			free(olh);
			if (errno == ENOMEM) {
				sz = olh->size;
				continue;
			}
			return (errno);
		}

		if (sort != 0)
			qsort(olh + 1, olh->count, olh->objsize, tablename_cmp);

		info = (ipfw_xtable_info *)(olh + 1);
		for (i = 0; i < olh->count; i++) {
			error = f(info, arg); /* Ignore errors for now */
			info = (ipfw_xtable_info *)((caddr_t)info + olh->objsize);
		}

		free(olh);
		break;
	}

	return (0);
}


/*
 * Retrieves all entries for given table @i in
 * eXtended format. Allocate buffer large enough
 * to store result. Called needs to free it later.
 *
 * Returns 0 on success.
 */
static int
table_do_get_list(ipfw_xtable_info *i, ipfw_obj_header **poh)
{
	ipfw_obj_header *oh;
	size_t sz;
	int c;

	sz = 0;
	oh = NULL;
	for (c = 0; c < 8; c++) {
		if (sz < i->size)
			sz = i->size + 44;
		if (oh != NULL)
			free(oh);
		if ((oh = calloc(1, sz)) == NULL)
			continue;
		table_fill_objheader(oh, i);
		oh->opheader.version = 1; /* Current version */
		if (do_get3(IP_FW_TABLE_XLIST, &oh->opheader, &sz) == 0) {
			*poh = oh;
			return (0);
		}

		if (errno != ENOMEM)
			break;
	}
	free(oh);

	return (errno);
}

/*
 * Shows all entries from @oh in human-readable format
 */
static void
table_show_list(ipfw_obj_header *oh, int need_header)
{
	ipfw_obj_tentry *tent;
	uint32_t count;
	ipfw_xtable_info *i;

	i = (ipfw_xtable_info *)(oh + 1);
	tent = (ipfw_obj_tentry *)(i + 1);

	if (need_header)
		printf("--- table(%s), set(%u) ---\n", i->tablename, i->set);

	count = i->count;
	while (count > 0) {
		table_show_entry(i, tent);
		tent = (ipfw_obj_tentry *)((caddr_t)tent + tent->head.length);
		count--;
	}
}

static void
table_show_value(char *buf, size_t bufsize, ipfw_table_value *v,
    uint32_t vmask, int print_ip)
{
	uint32_t flag, i, l;
	size_t sz;
	struct in_addr a4;
	char abuf[INET6_ADDRSTRLEN];

	sz = bufsize;

	/*
	 * Some shorthands for printing values:
	 * legacy assumes all values are equal, so keep the first one.
	 */
	if (vmask == IPFW_VTYPE_LEGACY) {
		if (print_ip != 0) {
			flag = htonl(v->tag);
			inet_ntop(AF_INET, &flag, buf, sz);
		} else
			snprintf(buf, sz, "%u", v->tag);
		return;
	}

	for (i = 1; i < (1 << 31); i *= 2) {
		if ((flag = (vmask & i)) == 0)
			continue;
		l = 0;

		switch (flag) {
		case IPFW_VTYPE_TAG:
			l = snprintf(buf, sz, "%u,", v->tag);
			break;
		case IPFW_VTYPE_PIPE:
			l = snprintf(buf, sz, "%u,", v->pipe);
			break;
		case IPFW_VTYPE_DIVERT:
			l = snprintf(buf, sz, "%d,", v->divert);
			break;
		case IPFW_VTYPE_SKIPTO:
			l = snprintf(buf, sz, "%d,", v->skipto);
			break;
		case IPFW_VTYPE_NETGRAPH:
			l = snprintf(buf, sz, "%u,", v->netgraph);
			break;
		case IPFW_VTYPE_FIB:
			l = snprintf(buf, sz, "%u,", v->fib);
			break;
		case IPFW_VTYPE_NAT:
			l = snprintf(buf, sz, "%u,", v->nat);
			break;
		case IPFW_VTYPE_LIMIT:
			l = snprintf(buf, sz, "%u,", v->limit);
			break;
		case IPFW_VTYPE_NH4:
			a4.s_addr = htonl(v->nh4);
			inet_ntop(AF_INET, &a4, abuf, sizeof(abuf));
			l = snprintf(buf, sz, "%s,", abuf);
			break;
		case IPFW_VTYPE_DSCP:
			l = snprintf(buf, sz, "%d,", v->dscp);
			break;
		case IPFW_VTYPE_NH6:
			inet_ntop(AF_INET6, &v->nh6, abuf, sizeof(abuf));
			l = snprintf(buf, sz, "%s,", abuf);
			break;
		}

		buf += l;
		sz -= l;
	}

	if (sz != bufsize)
		*(buf - 1) = '\0';
}

static void
table_show_entry(ipfw_xtable_info *i, ipfw_obj_tentry *tent)
{
	char *comma, tbuf[128], pval[128];
	void *paddr;
	struct tflow_entry *tfe;

	table_show_value(pval, sizeof(pval), &tent->v.value, i->vmask,
	    co.do_value_as_ip);

	switch (i->type) {
	case IPFW_TABLE_ADDR:
		/* IPv4 or IPv6 prefixes */
		inet_ntop(tent->subtype, &tent->k, tbuf, sizeof(tbuf));
		printf("%s/%u %s\n", tbuf, tent->masklen, pval);
		break;
	case IPFW_TABLE_INTERFACE:
		/* Interface names */
		printf("%s %s\n", tent->k.iface, pval);
		break;
	case IPFW_TABLE_NUMBER:
		/* numbers */
		printf("%u %s\n", tent->k.key, pval);
		break;
	case IPFW_TABLE_FLOW:
		/* flows */
		tfe = &tent->k.flow;
		comma = "";

		if ((i->tflags & IPFW_TFFLAG_SRCIP) != 0) {
			if (tfe->af == AF_INET)
				paddr = &tfe->a.a4.sip;
			else
				paddr = &tfe->a.a6.sip6;

			inet_ntop(tfe->af, paddr, tbuf, sizeof(tbuf));
			printf("%s%s", comma, tbuf);
			comma = ",";
		}

		if ((i->tflags & IPFW_TFFLAG_PROTO) != 0) {
			printf("%s%d", comma, tfe->proto);
			comma = ",";
		}

		if ((i->tflags & IPFW_TFFLAG_SRCPORT) != 0) {
			printf("%s%d", comma, ntohs(tfe->sport));
			comma = ",";
		}
		if ((i->tflags & IPFW_TFFLAG_DSTIP) != 0) {
			if (tfe->af == AF_INET)
				paddr = &tfe->a.a4.dip;
			else
				paddr = &tfe->a.a6.dip6;

			inet_ntop(tfe->af, paddr, tbuf, sizeof(tbuf));
			printf("%s%s", comma, tbuf);
			comma = ",";
		}

		if ((i->tflags & IPFW_TFFLAG_DSTPORT) != 0) {
			printf("%s%d", comma, ntohs(tfe->dport));
			comma = ",";
		}

		printf(" %s\n", pval);
	}
}

static int
table_do_get_stdlist(uint16_t opcode, ipfw_obj_lheader **polh)
{
	ipfw_obj_lheader req, *olh;
	size_t sz;

	memset(&req, 0, sizeof(req));
	sz = sizeof(req);

	if (do_get3(opcode, &req.opheader, &sz) != 0)
		if (errno != ENOMEM)
			return (errno);

	sz = req.size;
	if ((olh = calloc(1, sz)) == NULL)
		return (ENOMEM);

	olh->size = sz;
	if (do_get3(opcode, &olh->opheader, &sz) != 0) {
		free(olh);
		return (errno);
	}

	*polh = olh;
	return (0);
}

static int
table_do_get_algolist(ipfw_obj_lheader **polh)
{

	return (table_do_get_stdlist(IP_FW_TABLES_ALIST, polh));
}

static int
table_do_get_vlist(ipfw_obj_lheader **polh)
{

	return (table_do_get_stdlist(IP_FW_TABLE_VLIST, polh));
}

void
ipfw_list_ta(int ac, char *av[])
{
	ipfw_obj_lheader *olh;
	ipfw_ta_info *info;
	int error, i;
	const char *atype;

	error = table_do_get_algolist(&olh);
	if (error != 0)
		err(EX_OSERR, "Unable to request algorithm list");

	info = (ipfw_ta_info *)(olh + 1);
	for (i = 0; i < olh->count; i++) {
		if ((atype = match_value(tabletypes, info->type)) == NULL)
			atype = "unknown";
		printf("--- %s ---\n", info->algoname);
		printf(" type: %s\n refcount: %u\n", atype, info->refcnt);

		info = (ipfw_ta_info *)((caddr_t)info + olh->objsize);
	}

	free(olh);
}


/* Copy of current kernel table_value structure */
struct _table_value {
	uint32_t	tag;		/* O_TAG/O_TAGGED */
	uint32_t	pipe;		/* O_PIPE/O_QUEUE */
	uint16_t	divert;		/* O_DIVERT/O_TEE */
	uint16_t	skipto;		/* skipto, CALLRET */
	uint32_t	netgraph;	/* O_NETGRAPH/O_NGTEE */
	uint32_t	fib;		/* O_SETFIB */
	uint32_t	nat;		/* O_NAT */
	uint32_t	nh4;
	uint8_t		dscp;
	uint8_t		spare0[3];
	/* -- 32 bytes -- */
	struct in6_addr	nh6;
	uint32_t	limit;		/* O_LIMIT */
	uint32_t	spare1;
	uint64_t	refcnt;		/* Number of references */
};

int
compare_values(const void *_a, const void *_b)
{
	struct _table_value *a, *b;

	a = (struct _table_value *)_a;
	b = (struct _table_value *)_b;

	if (a->spare1 < b->spare1)
		return (-1);
	else if (a->spare1 > b->spare1)
		return (1);

	return (0);
}

void
ipfw_list_values(int ac, char *av[])
{
	ipfw_obj_lheader *olh;
	struct _table_value *v;
	int error, i;
	uint32_t vmask;
	char buf[128];

	error = table_do_get_vlist(&olh);
	if (error != 0)
		err(EX_OSERR, "Unable to request value list");

	vmask = 0x7FFFFFFF; /* Similar to IPFW_VTYPE_LEGACY */

	table_print_valheader(buf, sizeof(buf), vmask);
	printf("HEADER: %s\n", buf);
	v = (struct _table_value *)(olh + 1);
	qsort(v, olh->count, olh->objsize, compare_values);
	for (i = 0; i < olh->count; i++) {
		table_show_value(buf, sizeof(buf), (ipfw_table_value *)v,
		    vmask, 0);
		printf("[%u] refs=%lu %s\n", v->spare1, v->refcnt, buf);
		v = (struct _table_value *)((caddr_t)v + olh->objsize);
	}

	free(olh);
}

int
compare_ntlv(const void *_a, const void *_b)
{
	ipfw_obj_ntlv *a, *b;

	a = (ipfw_obj_ntlv *)_a;
	b = (ipfw_obj_ntlv *)_b;

	if (a->set < b->set)
		return (-1);
	else if (a->set > b->set)
		return (1);

	if (a->idx < b->idx)
		return (-1);
	else if (a->idx > b->idx)
		return (1);

	return (0);
}

int
compare_kntlv(const void *k, const void *v)
{
	ipfw_obj_ntlv *ntlv;
	uint16_t key;

	key = *((uint16_t *)k);
	ntlv = (ipfw_obj_ntlv *)v;

	if (key < ntlv->idx)
		return (-1);
	else if (key > ntlv->idx)
		return (1);
	
	return (0);
}

/*
 * Finds table name in @ctlv by @idx.
 * Uses the following facts:
 * 1) All TLVs are the same size
 * 2) Kernel implementation provides already sorted list.
 *
 * Returns table name or NULL.
 */
char *
table_search_ctlv(ipfw_obj_ctlv *ctlv, uint16_t idx)
{
	ipfw_obj_ntlv *ntlv;

	ntlv = bsearch(&idx, (ctlv + 1), ctlv->count, ctlv->objsize,
	    compare_kntlv);

	if (ntlv != 0)
		return (ntlv->name);

	return (NULL);
}

void
table_sort_ctlv(ipfw_obj_ctlv *ctlv)
{

	qsort(ctlv + 1, ctlv->count, ctlv->objsize, compare_ntlv);
}

int
table_check_name(char *tablename)
{
	int c, i, l;

	/*
	 * Check if tablename is null-terminated and contains
	 * valid symbols only. Valid mask is:
	 * [a-zA-Z0-9\-_\.]{1,63}
	 */
	l = strlen(tablename);
	if (l == 0 || l >= 64)
		return (EINVAL);
	for (i = 0; i < l; i++) {
		c = tablename[i];
		if (isalpha(c) || isdigit(c) || c == '_' ||
		    c == '-' || c == '.')
			continue;
		return (EINVAL);	
	}

	/* Restrict some 'special' names */
	if (strcmp(tablename, "all") == 0)
		return (EINVAL);

	return (0);
}

