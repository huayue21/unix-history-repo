#ifndef lint
static char sccsid[] = "@(#)versys.c	5.2 (Berkeley) %G%";
#endif

#include "uucp.h"

#define SNAMESIZE 7

/*******
 *	versys(name)	verify system names n1 and n2
 *	char *name;
 *
 *	return codes:  0  |  FAIL
 */

versys(name)
register char *name;
{
	register FILE *fp;
	char line[1000];
	char s1[SNAMESIZE + 1];
	char myname[SNAMESIZE + 1];

	sprintf(myname, "%.7s", Myname);
	sprintf(s1, "%.7s", name);
	if (strcmp(s1, myname) == 0)
		return SUCCESS;

	fp = fopen(SYSFILE, "r");
	ASSERT(fp != NULL, CANTOPEN, SYSFILE, 0);
	while (cfgets(line, sizeof(line), fp) != NULL) {
		char *targs[100];

		getargs(line, targs, 100);
		targs[0][7] = '\0';
		if (strcmp(s1, targs[0]) == SAME) {
			fclose(fp);
			return SUCCESS;
		}
	}
	fclose(fp);
	return FAIL;
}
