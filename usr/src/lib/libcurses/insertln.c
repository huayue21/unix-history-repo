/*
 * Copyright (c) 1981 Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "@(#)insertln.c	5.9 (Berkeley) %G%";
#endif	/* not lint */

#include <curses.h>
#include <string.h>

/*
 * winsertln --
 *	Do an insert-line on the window, leaving (cury, curx) unchanged.
 */
int
winsertln(win)
	register WINDOW *win;
{

	register int y;
	register char *end;
	register LINE *temp;

#ifdef DEBUG
	__TRACE("insertln: (%0.2o)\n", win);
#endif
	if (win->orig == NULL)
		temp = win->lines[win->maxy - 1];
	for (y = win->maxy - 1; y > win->cury; --y) {
		win->lines[y]->flags &= ~__ISPASTEOL;
		win->lines[y - 1]->flags &= ~__ISPASTEOL;
		if (win->orig == NULL)
			win->lines[y] = win->lines[y - 1];
		else
			bcopy(win->lines[y - 1]->line, 
			      win->lines[y]->line, win->maxx);
		touchline(win, y, 0, win->maxx - 1);
	}
	if (win->orig == NULL)
		win->lines[y] = temp;
	else
		temp = win->lines[y];
	(void)memset(temp->line, ' ', &temp->line[win->maxx] - temp->line);
	touchline(win, y, 0, win->maxx - 1);
	if (win->orig == NULL)
		__id_subwins(win);
	return (OK);
}
