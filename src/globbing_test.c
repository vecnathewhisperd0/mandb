/*
 * globbing.c: interface to the POSIX glob routines
 *  
 * Copyright (C) 1995 Graeme W. Wilford. (Wilf.)
 * Copyright (C) 2001, 2002, 2003, 2006, 2007 Colin Watson.
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
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#ifdef STDC_HEADERS
#  include <stdlib.h>
#endif /* STDC_HEADERS */

#include "gettext.h"
#define _(String) gettext (String)

#include <getopt.h>

#include "manconfig.h"
#include "globbing.h"

char *program_name;

extern const char *extension;

static const struct option long_options[] =
{
	{"debug",	no_argument,		0,	'd'},
	{"extension",	required_argument,	0,	'e'},
	{"ignore-case",	no_argument,		0,	'i'},
	{"match-case",	no_argument,		0,	'I'},
	{"help",	no_argument,		0,	'h'},
	{"version",	no_argument,		0,	'V'},
	{0, 0, 0, 0}
};

static const char args[] = "de:iIhV";

static void usage (int status)
{
	printf (_("usage: %s [-deiIhV] path section name\n"), program_name);
	printf (_(
		"-d, --debug                 emit debugging messages.\n"
		"-e, --extension             limit search to extension type `extension'.\n"
		"-i, --ignore-case           look for pages case-insensitively (default).\n"
		"-I, --match-case            look for pages case-sensitively.\n"
		"-V, --version               show version.\n"
		"-h, --help                  show this usage message.\n"));

	exit (status);
}

int main (int argc, char **argv)
{
	int c;
	int i;
	int match_case = 0;

	program_name = xstrdup (basename (argv[0]));

	while ((c = getopt_long (argc, argv, args,
				 long_options, NULL)) != -1) {
		switch (c) {
			case 'd':
				debug_level = 1;
				break;
			case 'e':
				extension = optarg;
				break;
			case 'i':
				match_case = 0;
				break;
			case 'I':
				match_case = 1;
				break;
			case 'V':
				ver ();
				break;
			case 'h':
				usage (OK);
				break;
			default:
				usage (FAIL);
				break;
		}
	}

	if (argc - optind != 3)
		usage (FAIL);

	for (i = 0; i <= 1; i++) {
		char **files;

		files = look_for_file (argv[optind], argv[optind + 1],
				       argv[optind + 2], i, match_case);
		if (files)
			while (*files)
				printf ("%s\n", *files++);
	}
	return 0;
}
