/*-
 * Copyright (c) 2001 Mark R V Murray
 * All rights reserved.
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed for the FreeBSD Project by
 * ThinkSec AS and NAI Labs, the Security Research Division of Network
 * Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#define _BSD_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#define PAM_SM_AUTH

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_mod_misc.h>


/* Is member in list? */
static int
in_list(char *const *list, const char *member)
{
	for (; *list; list++)
		if (strcmp(*list, member) == 0)
			return 1;
	return 0;
}

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t * pamh, int flags __unused,
    int argc __unused, const char *argv[] __unused)
{
	struct passwd *pwd;
	struct group *grp;
	int retval;
	uid_t tuid;
	const char *user, *targetuser;
	const char *use_group;

	retval = pam_get_user(pamh, &targetuser, NULL);
	if (retval != PAM_SUCCESS)
		return (retval);
	pwd = getpwnam(targetuser);
	if (pwd != NULL)
		tuid = pwd->pw_uid;
	else
		return (PAM_AUTH_ERR);

	PAM_LOG("Got target user: %s   uid: %d", targetuser, tuid);

	if (openpam_get_option(pamh, "auth_as_self")) {
		pwd = getpwnam(getlogin());
		user = strdup(pwd->pw_name);
	}
	else {
		user = targetuser;
		pwd = getpwnam(user);
	}
	if (pwd == NULL)
		return (PAM_AUTH_ERR);

	PAM_LOG("Got user: %s", user);
	PAM_LOG("User's primary uid, gid: %d, %d", pwd->pw_uid, pwd->pw_gid);

	/* Ignore if already uid 0 */
	if (pwd->pw_uid == 0)
		return (PAM_IGNORE);

	PAM_LOG("Not superuser");

	/* If authenticating as something non-superuser, return OK */
	if (openpam_get_option(pamh, "noroot_ok"))
		if (tuid != 0)
			return (PAM_SUCCESS);

	PAM_LOG("Checking group");

	if ((use_group = openpam_get_option(pamh, "group")) == NULL) {
		if ((grp = getgrnam("wheel")) == NULL)
			grp = getgrgid(0);
	}
	else
		grp = getgrnam(use_group);

	if (grp == NULL || grp->gr_mem == NULL) {
		if (openpam_get_option(pamh, "deny"))
			return (PAM_IGNORE);
		else {
			PAM_VERBOSE_ERROR("Permission denied");
			return (PAM_AUTH_ERR);
		}
	}

	PAM_LOG("Got group: %s", grp->gr_name);

	/* If the group is empty, see if we exempt empty groups. */
	if (*(grp->gr_mem) == NULL) {
		if (openpam_get_option(pamh, "exempt_if_empty"))
			return (PAM_IGNORE);
	}

	if (pwd->pw_gid == grp->gr_gid || in_list(grp->gr_mem, pwd->pw_name)) {
		if (openpam_get_option(pamh, "deny")) {
			PAM_VERBOSE_ERROR("Member of group %s; denied",
			    grp->gr_name);
			return (PAM_PERM_DENIED);
		}
		if (openpam_get_option(pamh, "trust"))
			return (PAM_SUCCESS);
		return (PAM_IGNORE);
	}

	if (openpam_get_option(pamh, "deny"))
		return (PAM_SUCCESS);

	PAM_VERBOSE_ERROR("Not member of group %s; denied", grp->gr_name);

	return (PAM_PERM_DENIED);
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t * pamh __unused, int flags __unused,
    int argc __unused, const char *argv[] __unused)
{

	return (PAM_SUCCESS);
}

PAM_MODULE_ENTRY("pam_wheel");
