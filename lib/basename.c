/*
 * basename.c: take path as argument, return basename.
 *  
 * Copyright (C) 1994, 1995 Graeme W. Wilford. (Wilf.)
 *
 * This file is part of man-db.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA  02111-1307  USA.
 *
 * Thu Dec  8 20:43:47 GMT 1994  Wilf. (G.Wilford@ee.surrey.ac.uk) 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#if defined(STDC_HEADERS)
#  include <string.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#else /* no string(s) header */
extern char *strrchr();
#endif /* STDC_HEADERS */

/* return basename of given filename */
char *basename(char *filename)
{
	char *base;

	base = strrchr(filename, '/');
	return base ? base + 1 : filename;
}
