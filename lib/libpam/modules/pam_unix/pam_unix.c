/*-
 * Copyright 1998 Juniper Networks, Inc.
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
 *
 *	$FreeBSD$
 */

#include <sys/types.h>
#include <sys/time.h>
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yppasswd.h>
#endif
#include <login_cap.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <pw_copy.h>
#include <pw_util.h>

#ifdef YP
#include <pw_yp.h>
#include "yppasswd_private.h"
#endif

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define	PAM_SM_SESSION
#define	PAM_SM_PASSWORD

#include <security/pam_modules.h>

#include "pam_mod_misc.h"

#define USER_PROMPT		"Username: "
#define PASSWORD_PROMPT		"Password: "
#define PASSWORD_PROMPT_EXPIRED	"\nPassword expired\nOld Password: "
#define NEW_PASSWORD_PROMPT_1	"New Password: "
#define NEW_PASSWORD_PROMPT_2	"New Password (again): "
#define PASSWORD_HASH		"md5"
#define DEFAULT_WARN		(2L * 7L * 86400L)  /* Two weeks */
#define	MAX_TRIES		3

enum { PAM_OPT_AUTH_AS_SELF=PAM_OPT_STD_MAX, PAM_OPT_NULLOK, PAM_OPT_LOCAL_PASS, PAM_OPT_NIS_PASS };

static struct opttab other_options[] = {
	{ "auth_as_self",	PAM_OPT_AUTH_AS_SELF },
	{ "nullok",		PAM_OPT_NULLOK },
	{ "local_pass",		PAM_OPT_LOCAL_PASS },
	{ "nis_pass",		PAM_OPT_NIS_PASS },
	{ NULL, 0 }
};

#ifdef YP
int pam_use_yp = 0;
int yp_errno = YP_TRUE;
#endif

char *tempname = NULL;
static int local_passwd(const char *user, const char *pass);
#ifdef YP
static int yp_passwd(const char *user, const char *pass);
#endif

/*
 * authentication management
 */
PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	login_cap_t *lc;
	struct options options;
	struct passwd *pwd;
	int retval;
	const char *pass, *user;
	char *encrypted, *password_prompt;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	if (pam_test_option(&options, PAM_OPT_AUTH_AS_SELF, NULL))
		pwd = getpwnam(getlogin());
	else {
		retval = pam_get_user(pamh, &user, NULL);
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);
		pwd = getpwnam(user);
	}

	PAM_LOG("Got user: %s", user);

	lc = login_getclass(NULL);
	password_prompt = login_getcapstr(lc, "passwd_prompt",
	    PASSWORD_PROMPT, PASSWORD_PROMPT);
	login_close(lc);
	lc = NULL;

	if (pwd != NULL) {

		PAM_LOG("Doing real authentication");

		if (pwd->pw_passwd[0] == '\0'
		    && pam_test_option(&options, PAM_OPT_NULLOK, NULL)) {
			/*
			 * No password case. XXX Are we giving too much away
			 * by not prompting for a password?
			 */
			PAM_LOG("No password, and null password OK");
			PAM_RETURN(PAM_SUCCESS);
		}
		else {
			retval = pam_get_pass(pamh, &pass, password_prompt,
			    &options);
			if (retval != PAM_SUCCESS)
				PAM_RETURN(retval);
			PAM_LOG("Got password");
		}
		encrypted = crypt(pass, pwd->pw_passwd);
		if (pass[0] == '\0' && pwd->pw_passwd[0] != '\0')
			encrypted = ":";

		PAM_LOG("Encrypted password 1 is: %s", encrypted);
		PAM_LOG("Encrypted password 2 is: %s", pwd->pw_passwd);

		retval = strcmp(encrypted, pwd->pw_passwd) == 0 ?
		    PAM_SUCCESS : PAM_AUTH_ERR;
	}
	else {

		PAM_LOG("Doing dummy authentication");

		/*
		 * User unknown.
		 * Encrypt a dummy password so as to not give away too much.
		 */
		retval = pam_get_pass(pamh, &pass, password_prompt,
		    &options);
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);
		PAM_LOG("Got password");
		crypt(pass, "xx");
		retval = PAM_AUTH_ERR;
	}

	/*
	 * The PAM infrastructure will obliterate the cleartext
	 * password before returning to the application.
	 */
	if (retval != PAM_SUCCESS)
		PAM_VERBOSE_ERROR("UNIX authentication refused");

	PAM_RETURN(retval);
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_SUCCESS);
}

/* 
 * account management
 */
PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options options;
	struct passwd *pw;
	struct timeval tp;
	login_cap_t *lc;
	time_t warntime;
	int retval;
	const char *user;
	char buf[128];

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	retval = pam_get_item(pamh, PAM_USER, (const void **)&user);
	if (retval != PAM_SUCCESS || user == NULL)
		/* some implementations return PAM_SUCCESS here */
		PAM_RETURN(PAM_USER_UNKNOWN);

	pw = getpwnam(user);
	if (pw == NULL)
		PAM_RETURN(PAM_USER_UNKNOWN);

	PAM_LOG("Got user: %s", user);

	retval = PAM_SUCCESS;
	lc = login_getpwclass(pw);

	if (pw->pw_change || pw->pw_expire)
		gettimeofday(&tp, NULL);

	warntime = login_getcaptime(lc, "warnpassword", DEFAULT_WARN,
	    DEFAULT_WARN);

	PAM_LOG("Got login_cap");

	if (pw->pw_change) {
		if (tp.tv_sec >= pw->pw_change)
			/* some implementations return PAM_AUTHTOK_EXPIRED */
			retval = PAM_NEW_AUTHTOK_REQD;
		else if (pw->pw_change - tp.tv_sec < warntime) {
			snprintf(buf, sizeof(buf),
			    "Warning: your password expires on %s",
			    ctime(&pw->pw_change));
			pam_prompt(pamh, PAM_ERROR_MSG, buf, NULL);
		}
	}

	warntime = login_getcaptime(lc, "warnexpire", DEFAULT_WARN,
	    DEFAULT_WARN);

	if (pw->pw_expire) {
		if (tp.tv_sec >= pw->pw_expire)
			retval = PAM_ACCT_EXPIRED;
		else if (pw->pw_expire - tp.tv_sec < warntime) {
			snprintf(buf, sizeof(buf),
			    "Warning: your account expires on %s",
			    ctime(&pw->pw_expire));
			pam_prompt(pamh, PAM_ERROR_MSG, buf, NULL);
		}
	}

	login_close(lc);

	PAM_RETURN(retval);
}

/* 
 * session management
 *
 * logging only
 */
PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_SUCCESS);
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options options;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	PAM_RETURN(PAM_SUCCESS);
}

/* 
 * password management
 *
 * standard Unix and NIS password changing
 */
PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	struct options options;
	struct passwd *pwd;
	int retval, retry, res, got;
	const char *user, *pass;
	char *new_pass, *new_pass_, *encrypted;

	pam_std_option(&options, other_options, argc, argv);

	PAM_LOG("Options processed");

	if (pam_test_option(&options, PAM_OPT_AUTH_AS_SELF, NULL))
		pwd = getpwnam(getlogin());
	else {
		retval = pam_get_user(pamh, &user, NULL);
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);
		pwd = getpwnam(user);
	}

	PAM_LOG("Got user: %s", user);

	if (flags & PAM_PRELIM_CHECK) {

		PAM_LOG("PRELIM round; checking user password");

		if (pwd->pw_passwd[0] == '\0'
		    && pam_test_option(&options, PAM_OPT_NULLOK, NULL)) {
			/*
			 * No password case. XXX Are we giving too much away
			 * by not prompting for a password?
			 */
			PAM_LOG("No password, and null password OK");
			PAM_RETURN(PAM_SUCCESS);
		}
		else {
			retval = pam_get_pass(pamh, &pass,
			    PASSWORD_PROMPT_EXPIRED, &options);
			if (retval != PAM_SUCCESS)
				PAM_RETURN(retval);
			PAM_LOG("Got password: %s", pass);
		}
		encrypted = crypt(pass, pwd->pw_passwd);
		if (pass[0] == '\0' && pwd->pw_passwd[0] != '\0')
			encrypted = ":";

		PAM_LOG("Encrypted password 1 is: %s", encrypted);
		PAM_LOG("Encrypted password 2 is: %s", pwd->pw_passwd);

		if (strcmp(encrypted, pwd->pw_passwd) != 0)
			PAM_RETURN(PAM_AUTH_ERR);

		retval = pam_set_item(pamh, PAM_OLDAUTHTOK, (const void *)pass);
		pass = NULL;
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);

		PAM_LOG("Stashed old password");

		retval = pam_set_item(pamh, PAM_AUTHTOK, (const void *)pass);
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);

		PAM_LOG("Voided old password");

		PAM_RETURN(PAM_SUCCESS);
	}
	else if (flags & PAM_UPDATE_AUTHTOK) {
		PAM_LOG("UPDATE round; checking user password");

		retval = pam_get_item(pamh, PAM_OLDAUTHTOK,
		    (const void **)&pass);
		if (retval != PAM_SUCCESS)
			PAM_RETURN(retval);

		PAM_LOG("Got old password: %s", pass);

		got = 0;
		retry = 0;
		while (retry++ < MAX_TRIES) {
			new_pass = NULL;
			retval = pam_prompt(pamh, PAM_PROMPT_ECHO_OFF,
			    NEW_PASSWORD_PROMPT_1, &new_pass);

			if (new_pass == NULL)
				new_pass = "";

			if (retval == PAM_SUCCESS) {
				new_pass_ = NULL;
				retval = pam_prompt(pamh, PAM_PROMPT_ECHO_OFF,
				    NEW_PASSWORD_PROMPT_2, &new_pass_);

				if (new_pass_ == NULL)
					new_pass_ = "";

				if (retval == PAM_SUCCESS) {
					if (strcmp(new_pass, new_pass_) == 0) {
						got = 1; 
						break;
					}
					else
						PAM_VERBOSE_ERROR("Password mismatch");
				}
			}
		}

		if (!got) {
			PAM_VERBOSE_ERROR("Unable to get valid password");
			PAM_RETURN(PAM_PERM_DENIED);
		}

		PAM_LOG("Got new password: %s", new_pass);

#ifdef YP
		/* If NIS is set in the passwd database, use it */
		res = use_yp((char *)user, 0, 0);
		if (res == USER_YP_ONLY) {
			if (!pam_test_option(&options, PAM_OPT_LOCAL_PASS,
			    NULL))
				retval = yp_passwd(user, new_pass);
			else {
				/* Reject 'local' flag if NIS is on and the user
				 * is not local
				 */
				retval = PAM_PERM_DENIED;
				PAM_LOG("Unknown local user: %s", user);
			}
		}
		else if (res == USER_LOCAL_ONLY) {
			if (!pam_test_option(&options, PAM_OPT_NIS_PASS, NULL))
				retval = local_passwd(user, new_pass);
			else {
				/* Reject 'nis' flag if user is only local */
				retval = PAM_PERM_DENIED;
				PAM_LOG("Unknown NIS user: %s", user);
			}
		}
		else if (res == USER_YP_AND_LOCAL) {
			if (pam_test_option(&options, PAM_OPT_NIS_PASS, NULL))
				retval = yp_passwd(user, new_pass);
			else
				retval = local_passwd(user, new_pass);
		}
		else
			retval = PAM_ABORT; /* Bad juju */
#else
		retval = local_passwd(user, new_pass);
#endif

		/* XXX wipe the mem as well */
		pass = NULL;
		new_pass = NULL;
	}
	else {
		/* Very bad juju */
		retval = PAM_ABORT;
		PAM_LOG("Illegal 'flags'");
	}

	PAM_RETURN(retval);
}

/* Mostly stolen from passwd(1)'s local_passwd.c - markm */

static unsigned char itoa64[] =		/* 0 ... 63 => ascii - 64 */
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void
to64(char *s, long v, int n)
{
	while (--n >= 0) {
		*s++ = itoa64[v&0x3f];
		v >>= 6;
	}
}

static int
local_passwd(const char *user, const char *pass)
{
	login_cap_t * lc;
	struct passwd *pwd;
	struct timeval tv;
	int pfd, tfd;
	char *crypt_type, salt[32];

	pwd = getpwnam(user);
	if (pwd == NULL)
		return(PAM_ABORT); /* Really bad things */

#ifdef YP
	pwd = (struct passwd *)&local_password;
#endif
	pw_init();

	pwd->pw_change = 0;
	lc = login_getclass(NULL);
	crypt_type = login_getcapstr(lc, "passwd_format",
		PASSWORD_HASH, PASSWORD_HASH);
	if (login_setcryptfmt(lc, crypt_type, NULL) == NULL)
		syslog(LOG_ERR, "cannot set password cipher");
	login_close(lc);
	/* Salt suitable for anything */
	srandomdev();
	gettimeofday(&tv, 0);
	to64(&salt[0], random(), 3);
	to64(&salt[3], tv.tv_usec, 3);
	to64(&salt[6], tv.tv_sec, 2);
	to64(&salt[8], random(), 5);
	to64(&salt[13], random(), 5);
	to64(&salt[17], random(), 5);
	to64(&salt[22], random(), 5);
	salt[27] = '\0';

	pwd->pw_passwd = crypt(pass, salt);

	pfd = pw_lock();
	tfd = pw_tmp();
	pw_copy(pfd, tfd, pwd);

	if (!pw_mkdb((char *)user))
		pw_error((char *)NULL, 0, 1);

	return PAM_SUCCESS;
}

#ifdef YP
/* Stolen from src/usr.bin/passwd/yp_passwd.c, carrying copyrights of:
 * Copyright (c) 1992/3 Theo de Raadt <deraadt@fsa.ca>
 * Copyright (c) 1994 Olaf Kirch <okir@monad.swb.de>
 * Copyright (c) 1995 Bill Paul <wpaul@ctr.columbia.edu>
 */
int
yp_passwd(const char *user, const char *pass)
{
	struct master_yppasswd master_yppasswd;
	struct passwd *pwd;
	struct rpc_err err;
	struct timeval tv;
	struct yppasswd yppasswd;
	CLIENT *clnt;
	login_cap_t *lc;
	int    *status;
	uid_t	uid;
	char   *master, *sockname = YP_SOCKNAME, salt[32];

	_use_yp = 1;

	uid = getuid();

	master = get_yp_master(1);
	if (master == NULL)
		return PAM_ABORT; /* Major disaster */

	/*
	 * It is presumed that by the time we get here, use_yp()
	 * has been called and that we have verified that the user
	 * actually exists. This being the case, the yp_password
	 * stucture has already been filled in for us.
	 */

	/* Use the correct password */
	pwd = (struct passwd *)&yp_password;

	pwd->pw_change = 0;

	/* Initialize password information */
	if (suser_override) {
		master_yppasswd.newpw.pw_passwd = strdup(pwd->pw_passwd);
		master_yppasswd.newpw.pw_name = strdup(pwd->pw_name);
		master_yppasswd.newpw.pw_uid = pwd->pw_uid;
		master_yppasswd.newpw.pw_gid = pwd->pw_gid;
		master_yppasswd.newpw.pw_expire = pwd->pw_expire;
		master_yppasswd.newpw.pw_change = pwd->pw_change;
		master_yppasswd.newpw.pw_fields = pwd->pw_fields;
		master_yppasswd.newpw.pw_gecos = strdup(pwd->pw_gecos);
		master_yppasswd.newpw.pw_dir = strdup(pwd->pw_dir);
		master_yppasswd.newpw.pw_shell = strdup(pwd->pw_shell);
		master_yppasswd.newpw.pw_class = pwd->pw_class != NULL ?
					strdup(pwd->pw_class) : "";
		master_yppasswd.oldpass = "";
		master_yppasswd.domain = yp_domain;
	} else {
		yppasswd.newpw.pw_passwd = strdup(pwd->pw_passwd);
		yppasswd.newpw.pw_name = strdup(pwd->pw_name);
		yppasswd.newpw.pw_uid = pwd->pw_uid;
		yppasswd.newpw.pw_gid = pwd->pw_gid;
		yppasswd.newpw.pw_gecos = strdup(pwd->pw_gecos);
		yppasswd.newpw.pw_dir = strdup(pwd->pw_dir);
		yppasswd.newpw.pw_shell = strdup(pwd->pw_shell);
		yppasswd.oldpass = "";
	}

	if (login_setcryptfmt(lc, "md5", NULL) == NULL)
		syslog(LOG_ERR, "cannot set password cipher");
	login_close(lc);
	/* Salt suitable for anything */
	srandomdev();
	gettimeofday(&tv, 0);
	to64(&salt[0], random(), 3);
	to64(&salt[3], tv.tv_usec, 3);
	to64(&salt[6], tv.tv_sec, 2);
	to64(&salt[8], random(), 5);
	to64(&salt[13], random(), 5);
	to64(&salt[17], random(), 5);
	to64(&salt[22], random(), 5);
	salt[27] = '\0';

	if (suser_override)
		master_yppasswd.newpw.pw_passwd = crypt(pass, salt);
	else
		yppasswd.newpw.pw_passwd = crypt(pass, salt);

	if (suser_override) {
		if ((clnt = clnt_create(sockname, MASTER_YPPASSWDPROG,
		    MASTER_YPPASSWDVERS, "unix")) == NULL) {
			syslog(LOG_ERR,
			    "Cannot contact rpc.yppasswdd on host %s: %s",
			    master, clnt_spcreateerror(""));
			return PAM_ABORT;
		}
	}
	else {
		if ((clnt = clnt_create(master, YPPASSWDPROG,
		    YPPASSWDVERS, "udp")) == NULL) {
			syslog(LOG_ERR,
			    "Cannot contact rpc.yppasswdd on host %s: %s",
			    master, clnt_spcreateerror(""));
			return PAM_ABORT;
		}
	}
	/*
	 * The yppasswd.x file said `unix authentication required',
	 * so I added it. This is the only reason it is in here.
	 * My yppasswdd doesn't use it, but maybe some others out there
	 * do. 					--okir
	 */
	clnt->cl_auth = authunix_create_default();

	if (suser_override)
		status = yppasswdproc_update_master_1(&master_yppasswd, clnt);
	else
		status = yppasswdproc_update_1(&yppasswd, clnt);

	clnt_geterr(clnt, &err);

	auth_destroy(clnt->cl_auth);
	clnt_destroy(clnt);

	if (err.re_status != RPC_SUCCESS || status == NULL || *status)
		return PAM_ABORT;

	return (err.re_status || status == NULL || *status)
	    ? PAM_ABORT : PAM_SUCCESS;
}
#endif /* YP */

PAM_MODULE_ENTRY("pam_unix");
