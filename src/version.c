/*
 * version.c: the ver() and atexit()/on_exit() routines.
 *  
 * Copyright (C) 1994, 1995 Graeme W. Wilford. (Wilf.)
 * Copyright (C) 1996, 2000 Fabrizio Polacco.
 * Copyright (C) 2001 Colin Watson.
 *
 * This file is part of man-db.
 *
 * man-db is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * man-db is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with man-db; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif

#include <libintl.h>
#define _(String) gettext (String)

#include "manconfig.h"
#include "lib/error.h"

/* print the version message, then exit */
void ver (void)
{
	printf (_("%s, version %s, %s\n"), program_name, VERSION, DATE);
	exit (OK);
}
