/*
 * accessdb.c: show every key/content pair in the database.
 *
 * Copyright (C) 1994, 1995 Graeme W. Wilford. (Wilf.)
 * Copyright (C) 2002 Colin Watson.
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
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Tue Apr 26 12:56:44 BST 1994  Wilf. (G.Wilford@ee.surrey.ac.uk) 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <errno.h>

#ifndef STDC_HEADERS
extern int errno;
#endif

#if defined(STDC_HEADERS)
#  include <string.h>
#  include <stdlib.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#else /* no string(s) header */
extern char *strchr();
#endif /* STDC_HEADERS */

#ifdef HAVE_LIBGEN_H
#  include <libgen.h>
#endif /* HAVE_LIBGEN_H */

#include "lib/gettext.h"
#define _(String) gettext (String)

#include "manconfig.h"
#include "libdb/mydbm.h"
#include "lib/error.h"

#ifdef HAVE_GETOPT_H
#  include <getopt.h>
#else /* !HAVE_GETOPT_H */
#  include "lib/getopt.h"
#endif /* HAVE_GETOPT_H */

char *program_name;
const char *cat_root;

static const struct option long_options[] =
{
	{"help",	no_argument,		0, 'h'},
	{"version",	no_argument,		0, 'V'},
	{0, 0, 0, 0}
};

static const char args[] = "hV";

/* for db_storage.c */
char *database;
MYDBM_FILE dbf;

static void usage (int status)
{
	printf (_("usage: %s [-hV] [man database]\n"), program_name);
	printf (_(
		"-V, --version               show version.\n"
		"-h, --help                  show this usage message.\n"
		"\n"
		"The man database defaults to %s%s.\n"), cat_root, MAN_DB);

	exit (status);
}

int main (int argc, char *argv[])
{
	int c;
	datum key;

	program_name = xstrdup (basename (argv[0]));
	if (is_directory (FHS_CAT_ROOT) == 1)
		cat_root = FHS_CAT_ROOT;
	else if (is_directory (CAT_ROOT) == 1)
		cat_root = CAT_ROOT;

	while ((c = getopt_long (argc, argv, args,
				 long_options, NULL)) != -1) {
		switch (c) {
			case 'h':
				usage (OK);
				break;
			case 'V':
				ver ();
				break;
			default:
				usage (FAIL);
				break;
		}
	}
	if (argc - optind > 1)
		usage (FAIL);
	else if (argc - optind == 1) 
		database = argv[1];
	else
		database = strappend (NULL, cat_root, MAN_DB, NULL);
		
	dbf = MYDBM_RDOPEN (database);
	if (dbf && dbver_rd (dbf)) {
		MYDBM_CLOSE (dbf);
		dbf = NULL;
	}
	if (!dbf) {
		error (0, errno, _("can't open %s for reading"), database);
		usage (FAIL);
	}

	key = MYDBM_FIRSTKEY (dbf);

	while (MYDBM_DPTR (key) != NULL) {
		datum content, nextkey;
		char *t, *nicekey;

		content = MYDBM_FETCH (dbf, key);
		if (!MYDBM_DPTR (content))
			exit (FATAL);
		nicekey = xstrdup (MYDBM_DPTR (key));
		while ( (t = strchr (nicekey, '\t')) )
			*t = '~';
		while ( (t = strchr (MYDBM_DPTR (content), '\t')) )
			*t = ' ';
		printf ("%s -> \"%s\"\n", nicekey, MYDBM_DPTR (content));
		free (nicekey); 
		MYDBM_FREE (MYDBM_DPTR (content));
		nextkey = MYDBM_NEXTKEY (dbf, key);
		MYDBM_FREE (MYDBM_DPTR (key));
		key = nextkey;
	}

	MYDBM_CLOSE (dbf);
	exit (OK);
}
