/*
 * db_gdbm.c: low level gdbm interface routines for man.
 *  
 * Copyright (C) 1994, 1995 Graeme W. Wilford. (Wilf.)
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Mon Aug  8 20:35:30 BST 1994  Wilf. (G.Wilford@ee.surrey.ac.uk) 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#if defined(GDBM) && !defined(HAVE_GDBM_EXISTS)

#ifdef STDC_HEADERS
#  include <stdlib.h>
#endif /* STDC_HEADERS */

#include "manconfig.h"
#include "mydbm.h"

int gdbm_exists(GDBM_FILE dbf, datum key)
{
	char *memory;

	memory = gdbm_fetch(dbf, key).dptr;
	if (memory) {
		free(memory);
		return 1;
	}

	return 0;
}
#endif /* GDBM && !HAVE_GDBM_EXISTS*/
