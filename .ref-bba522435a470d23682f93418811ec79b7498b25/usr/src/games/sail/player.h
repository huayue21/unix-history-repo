/*
 * sccsid = "@(#)player.h	1.2 %G%";
 */
#include <curses.h>
#include "externs.h"

/* sizes and coordinates for the screen */

#define LINE_T		0
#define LINE_L		0
#define LINE_X		COLS
#define LINE_Y		1
#define LINE_B		(LINE_T+LINE_Y-1)
#define LINE_R		(LINE_L+LINE_X-1)

#define BOX_T		1
#define BOX_L		0
#define BOX_X		65
#define BOX_Y		16
#define BOX_B		(BOX_T+BOX_Y-1)
#define BOX_R		(BOX_L+BOX_X-1)

#define TURN_T		BOX_B
#define TURN_X		7
#define TURN_L		((BOX_L+BOX_R-TURN_X)/2)
#define TURN_Y		1
#define TURN_B		(TURN_T+TURN_Y+1)
#define TURN_R		(TURN_L+TURN_X+1)

#define STAT_T		0
#define STAT_L		(BOX_R+2)
#define STAT_B		BOX_B
#define STAT_R		(COLS-1)
#define STAT_X		(STAT_R-STAT_L+1)
#define STAT_Y		(STAT_B-STAT_T+1)
#define STAT_1		0
#define STAT_2          (STAT_1+4)
#define STAT_3          (STAT_2+7)

#define SCROLL_T	(BOX_B+1)
#define SCROLL_L	0
#define SCROLL_B	(LINES-1)
#define SCROLL_R	(COLS-1)
#define SCROLL_X	COLS
#define SCROLL_Y	(LINES-SCROLL_T)

#define VIEW_T		(BOX_T+1)
#define VIEW_L		(BOX_L+1)
#define VIEW_X		(BOX_X-5)
#define VIEW_Y		(BOX_Y-2)
#define VIEW_B		(VIEW_T+VIEW_Y-1)
#define VIEW_R		(VIEW_L+VIEW_X-1)

#define SLOT_T		VIEW_T
#define SLOT_L		(VIEW_R+1)
#define SLOT_X		3
#define SLOT_Y		VIEW_Y
#define SLOT_B		VIEW_B
#define SLOT_R		(SLOT_L+SLOT_X-1)

WINDOW *view_w;
WINDOW *slot_w;
WINDOW *scroll_w;
WINDOW *stat_w;
WINDOW *turn_w;

char done_curses;
char loaded, fired, changed, repaired;
char dont_adjust;
int viewrow, viewcol;
char movebuf[10];
char version[];
int player;
struct ship *ms;		/* memorial structure, &cc->ship[player] */
struct File *mf;		/* ms->file */
struct shipspecs *mc;		/* ms->specs */

/* condition codes for leave() */
#define LEAVE_QUIT	0
#define LEAVE_CAPTURED	1
#define LEAVE_HURRICAN	2
#define LEAVE_DRIVER	3
