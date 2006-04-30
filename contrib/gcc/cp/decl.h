/* Variables and structures for declaration processing.
   Copyright (C) 1993, 2000, 2002 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* In grokdeclarator, distinguish syntactic contexts of declarators.  */
enum decl_context
{ NORMAL,			/* Ordinary declaration */
  FUNCDEF,			/* Function definition */
  PARM,				/* Declaration of parm before function body */
  CATCHPARM,			/* Declaration of catch parm */
  FIELD,			/* Declaration inside struct or union */
  BITFIELD,			/* Likewise but with specified width */
  TYPENAME,			/* Typename (inside cast or sizeof)  */
  MEMFUNCDEF			/* Member function definition */
};

/* We need this in here to get the decl_context definition.  */
extern tree grokdeclarator (tree, tree, enum decl_context, int, tree*);

#ifdef DEBUG_CP_BINDING_LEVELS
/* Purely for debugging purposes.  */
extern int debug_bindings_indentation;
#endif
