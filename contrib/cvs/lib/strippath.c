/* strippath.c -- remove unnecessary components from a path specifier
   Copyright (C) 1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if STDC_HEADERS || HAVE_STRING_H
#include <string.h>
/* An ANSI string.h and pre-ANSI memory.h might conflict. */
#if !STDC_HEADERS && HAVE_MEMORY_H
#include <memory.h>
#endif /* not STDC_HEADERS and HAVE_MEMORY_H */
#else /* not STDC_HJEADERS and not HAVE_STRING_H */
#include <strings.h>
/* memory.h and strings.h conflict on some systems. */
#endif /* not STDC_HEADERS and not HAVE_STRING_H */

#include <stdio.h>

#if __STDC__
static void remove_component(char *beginc, char *endc);
void strip_trailing_slashes(char *path);
#else
static void remove_component();
void strip_trailing_slashes();
#endif /* __STDC__ */

/* Remove unnecessary components from PATH. */

void
strip_path (path)
     char *path;
{
  int stripped = 0;
  char *cp, *slash;

  for (cp = path; (slash = strchr(cp, '/')) != NULL; cp = slash)
    {
      *slash = '\0';
      if ((!*cp && (cp != path || stripped)) ||
	  strcmp(cp, ".") == 0 || strcmp(cp, "/") == 0)
	{
	  stripped = 1;
	  remove_component(cp, slash);
	  slash = cp;
	}
      else
	{
	  *slash++ = '/';
	}
    }
  strip_trailing_slashes(path);
}

/* Remove the component delimited by BEGINC and ENDC from the path */

static void
remove_component (beginc, endc)
     char *beginc;
     char *endc;
{
  for (endc++; *endc; endc++)
    *beginc++ = *endc;
  *beginc = '\0';
}
