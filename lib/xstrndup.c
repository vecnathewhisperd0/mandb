/* xstrndup.c -- copy a portion of a string with out of memory checking
   Copyright (C) 2001 Colin Watson.
   Based on strndup.c,
   copyright (C) 1996, 1997, 1998 Free Software Foundation, Inc.

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
#  include "config.h"
#endif

#if defined(STDC_HEADERS) || defined(HAVE_STRING_H)
#include <string.h>
#else
#include <strings.h>
#endif

#if __STDC__
#define VOID void
#else
#define VOID char
#endif

VOID *xmalloc ();
extern size_t strnlen (const char *string, size_t maxlen);

/* Return a newly allocated copy of the first SIZE bytes of STRING.  */

char *
xstrndup (string, size)
     const char *string;
     size_t size;
{
  size_t len;
  char *new;
  if (!string)
    return NULL;
  len = strnlen (string, size);
  new = xmalloc (len + 1);
  new[len] = '\0';
  return (char *) memcpy (new, string, len);
}
