/*
 * dbver.c: code to read, write and identify the database version no.
 *
 * Copyright (C) 1994, 1995 Graeme W. Wilford. (Wilf.)
 * Copyright (C) 2001, 2002 Colin Watson.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Mon Aug 18 20:35:30 BST 1994  Wilf. (G.Wilford@ee.surrey.ac.uk)
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>

#if defined(STDC_HEADERS)
#include <string.h>
#include <stdlib.h>
#elif defined(HAVE_STRING_H)
#include <string.h>
#elif defined(HAVE_STRINGS_H)
#include <strings.h>
#endif /* STDC_HEADERS */

#include "lib/gettext.h"
#define _(String) gettext (String)

#include "manconfig.h"
#include "lib/error.h"
#include "mydbm.h"

static datum content;

static int dbver (MYDBM_FILE dbf)
{
	datum key;

	key.dptr = xstrdup (VER_KEY);
	key.dsize = sizeof VER_KEY;

	content = MYDBM_FETCH (dbf, key);

	free (key.dptr);

	if (content.dptr == NULL)
		return -1;
	else if (!STREQ (content.dptr, VER_ID))
		return 1;
	else
		return 0;
}

void dbver_wr (MYDBM_FILE dbf)
{
	datum key, content;

	key.dptr = xstrdup (VER_KEY);
	key.dsize = sizeof VER_KEY;
	content.dptr = xstrdup (VER_ID);
	content.dsize = sizeof VER_ID;

	if (MYDBM_INSERT (dbf, key, content) != 0)
		error (FATAL, 0,
		       _("fatal: unable to insert version identifier into %s"),
		       database);

	free (key.dptr);
	free (content.dptr);
}

int dbver_rd (MYDBM_FILE dbf)
{
	int status;

	status = dbver (dbf);

	if (status == -1) {
		if (debug)
			fprintf (stderr,
				 _("warning: %s has no version identifier\n"),
				 database);
	} else if (status == 1) {
		if (debug)
			fprintf (stderr,
				 _("warning: %s is version %s, expecting %s\n"),
				 database, content.dptr, VER_ID);
	} else {
		MYDBM_FREE (content.dptr);
		return 0;
	}

	return 1;
}
