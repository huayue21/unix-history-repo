/*
 * And thus spoke RPCGEN:
 *    Please do not edit this file.
 *    It was generated using rpcgen.
 *
 * And thus replied Lpd@NannyMUD:
 *    Who cares? :-) /Peter Eriksson <pen@signum.se>
 *
 *
 * Modification history:
 *    940616 pen@signum.se	Major cleanups.
 *    940713 pen@signum.se	Added SunOS 4 prototypes.
 *
 * $FreeBSD$
 */

#include "system.h"


#include "yp.h"

#ifndef NULL
#define NULL 0
#endif

__xdr_ypall_cb_t __xdr_ypall_cb;

bool_t
__xdr_ypstat(XDR *xdrs, ypstat *objp)
{
    if (!xdr_enum(xdrs, (enum_t *)objp))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_ypxfrstat(XDR *xdrs, ypxfrstat *objp)
{
    if (!xdr_enum(xdrs, (enum_t *)objp))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_domainname(XDR *xdrs, domainname *objp)
{
    if (!xdr_string(xdrs, objp, YPMAXDOMAIN))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_mapname(XDR *xdrs, mapname *objp)
{
    if (!xdr_string(xdrs, objp, YPMAXMAP))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_peername(XDR *xdrs, peername *objp)
{
    if (!xdr_string(xdrs, objp, YPMAXPEER))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_keydat(XDR *xdrs, keydat *objp)
{
    if (!xdr_bytes(xdrs, (char **)&objp->keydat_val,
		   (u_int *)&objp->keydat_len, YPMAXRECORD))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_valdat(XDR *xdrs, valdat *objp)
{
    if (!xdr_bytes(xdrs, (char **)&objp->valdat_val,
		   (u_int *)&objp->valdat_len, YPMAXRECORD))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_ypmap_parms(XDR *xdrs, ypmap_parms *objp)
{
    if (!__xdr_domainname(xdrs, &objp->domain))
	return FALSE;

    if (!__xdr_mapname(xdrs, &objp->map))
	return FALSE;

    if (!xdr_u_int(xdrs, &objp->ordernum))
	return FALSE;

    if (!__xdr_peername(xdrs, &objp->peer))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_ypreq_key(XDR *xdrs, ypreq_key *objp)
{
    if (!__xdr_domainname(xdrs, &objp->domain))
	return FALSE;

    if (!__xdr_mapname(xdrs, &objp->map))
	return FALSE;

    if (!__xdr_keydat(xdrs, &objp->key))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_ypreq_nokey(XDR *xdrs, ypreq_nokey *objp)
{
    if (!__xdr_domainname(xdrs, &objp->domain))
	return FALSE;

    if (!__xdr_mapname(xdrs, &objp->map))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_ypreq_xfr(XDR *xdrs, ypreq_xfr *objp)
{
    if (!__xdr_ypmap_parms(xdrs, &objp->map_parms))
	return FALSE;

    if (!xdr_u_int(xdrs, &objp->transid))
	return FALSE;

    if (!xdr_u_int(xdrs, &objp->prog))
	return FALSE;

    if (!xdr_u_int(xdrs, &objp->port))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_ypresp_val(XDR *xdrs, ypresp_val *objp)
{
    if (!__xdr_ypstat(xdrs, &objp->stat))
	return FALSE;

    if (!__xdr_valdat(xdrs, &objp->val))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_ypresp_key_val(XDR *xdrs, ypresp_key_val *objp)
{
    if (!__xdr_ypstat(xdrs, &objp->stat))
	return FALSE;

#if 0 /* The Sun-supplied yp.x RPC input file have these in the wrong order */
    if (!__xdr_keydat(xdrs, &objp->key))
	return FALSE;

    if (!__xdr_valdat(xdrs, &objp->val))
	return FALSE;
#else
    if (!__xdr_valdat(xdrs, &objp->val))
	return FALSE;

    if (!__xdr_keydat(xdrs, &objp->key))
	return FALSE;
#endif
    return TRUE;
}

bool_t
__xdr_ypresp_master(XDR *xdrs, ypresp_master *objp)
{
    if (!__xdr_ypstat(xdrs, &objp->stat))
	return FALSE;

    if (!__xdr_peername(xdrs, &objp->peer))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_ypresp_order(XDR *xdrs, ypresp_order *objp)
{
    if (!__xdr_ypstat(xdrs, &objp->stat))
	return FALSE;

    if (!xdr_u_int(xdrs, &objp->ordernum))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_ypresp_all(XDR *xdrs, ypresp_all *objp)
{
    if (xdrs->x_op == XDR_ENCODE)
    {
	while (1)
	{
	    if (!xdr_bool(xdrs, &objp->more))
	    {
		if (__xdr_ypall_cb.u.close != NULL)
		    (*(__xdr_ypall_cb.u.close))(__xdr_ypall_cb.data);

		__xdr_ypall_cb.data = NULL;

		return FALSE;
	    }

	    if (!__xdr_ypresp_key_val(xdrs, &objp->ypresp_all_u.val))
	    {
		if (__xdr_ypall_cb.u.close != NULL)
		    (*(__xdr_ypall_cb.u.close))(__xdr_ypall_cb.data);

		__xdr_ypall_cb.data = NULL;

		return FALSE;
	    }

	    if (objp->ypresp_all_u.val.stat != YP_TRUE)
	    {
		objp->more = FALSE;

		if (__xdr_ypall_cb.u.close != NULL)
		    (*(__xdr_ypall_cb.u.close))(__xdr_ypall_cb.data);

		__xdr_ypall_cb.data = NULL;

		if (!xdr_bool(xdrs, &objp->more))
		    return FALSE;

		return TRUE;
	    }

	    if ((*__xdr_ypall_cb.u.encode)(&objp->ypresp_all_u.val,
					 __xdr_ypall_cb.data) == YP_NOKEY)
		objp->more = FALSE;
	}
    }

#ifdef NOTYET /* This code isn't needed in the server */
    else if (xdrs->x_op == XDR_DECODE)
    {
	int more = 0;


	while (1)
	{
	    if (!xdr_bool(xdrs, &objp->more))
		return FALSE;

	    switch (objp->more)
	    {
	      case TRUE:
		if (!__xdr_ypresp_key_val(xdrs, &objp->ypresp_all_u.val))
		    return FALSE;

		if (more == 0)
		    more = (*__xdr_ypall_callback->foreach.decoder)
			(&objp->ypresp_all_u.val, __xdr_ypall_callback->data);
		break;

	      case FALSE:
		return TRUE;

	      default:
		return FALSE;
	    }
	}
	return FALSE;
    }
#endif

    return TRUE;
}

bool_t
__xdr_ypresp_xfr(XDR *xdrs, ypresp_xfr *objp)
{
    if (!xdr_u_int(xdrs, &objp->transid))
	return FALSE;

    if (!__xdr_ypxfrstat(xdrs, &objp->xfrstat))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_ypmaplist(XDR *xdrs, ypmaplist *objp)
{
    if (!__xdr_mapname(xdrs, &objp->map))
	return FALSE;

    if (!xdr_pointer(xdrs, (char **)&objp->next, sizeof(ypmaplist),
		     (xdrproc_t)__xdr_ypmaplist))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_ypresp_maplist(XDR *xdrs, ypresp_maplist *objp)
{
    if (!__xdr_ypstat(xdrs, &objp->stat))
	return FALSE;

    if (!xdr_pointer(xdrs, (char **)&objp->maps, sizeof(ypmaplist),
		     (xdrproc_t)__xdr_ypmaplist))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_yppush_status(XDR *xdrs, yppush_status *objp)
{
    if (!xdr_enum(xdrs, (enum_t *)objp))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_yppushresp_xfr(XDR *xdrs, yppushresp_xfr *objp)
{
    if (!xdr_u_int(xdrs, &objp->transid))
	return FALSE;

    if (!__xdr_yppush_status(xdrs, &objp->status))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_ypbind_resptype(XDR *xdrs, ypbind_resptype *objp)
{
    if (!xdr_enum(xdrs, (enum_t *)objp))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_ypbind_binding(XDR *xdrs, ypbind_binding *objp)
{
    if (!xdr_opaque(xdrs, objp->ypbind_binding_addr, 4))
	return FALSE;

    if (!xdr_opaque(xdrs, objp->ypbind_binding_port, 2))
	return FALSE;

    return TRUE;
}

bool_t
__xdr_ypbind_resp(XDR *xdrs, ypbind_resp *objp)
{
    if (!__xdr_ypbind_resptype(xdrs, &objp->ypbind_status))
	return FALSE;

    switch (objp->ypbind_status)
    {
      case YPBIND_FAIL_VAL:
	if (!xdr_u_int(xdrs, &objp->ypbind_resp_u.ypbind_error))
	    return FALSE;
	break;

      case YPBIND_SUCC_VAL:
	if (!__xdr_ypbind_binding(xdrs, &objp->ypbind_resp_u.ypbind_bindinfo))
	    return FALSE;
	break;

      default:
	return FALSE;
    }
    return TRUE;
}

bool_t
__xdr_ypbind_setdom(XDR *xdrs, ypbind_setdom *objp)
{
    if (!__xdr_domainname(xdrs, &objp->ypsetdom_domain))
	return FALSE;

    if (!__xdr_ypbind_binding(xdrs, &objp->ypsetdom_binding))
	return FALSE;

    if (!xdr_u_int(xdrs, &objp->ypsetdom_vers))
	return FALSE;

    return TRUE;
}
