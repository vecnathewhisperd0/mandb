/*
 * lexgrog_test.c: test whatis extraction from man/cat pages
 *  
 * Copyright (C) 1994, 1995 Graeme W. Wilford. (Wilf.)
 * Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007 Colin Watson.
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

#include <sys/stat.h>

#if defined(STDC_HEADERS)
#  include <string.h>
#  include <stdlib.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#endif /* STDC_HEADERS */

#ifdef HAVE_LIBGEN_H
#  include <libgen.h>
#endif /* HAVE_LIBGEN_H */

#include <getopt.h>

#include "gettext.h"
#define _(String) gettext (String)

#include "manconfig.h"

#include "error.h"
#include "pipeline.h"

#include "descriptions.h"
#include "ult_src.h"

char *program_name;
int quiet = 1;

static const struct option long_options[] =
{
	{"man",		no_argument,		0,	'm'},
	{"cat",		no_argument,		0,	'c'},
	{"whatis",	no_argument,		0,	'w'},
	{"filters",	no_argument,		0,	'f'},
	{"encoding",	required_argument,	0,	'E'},
	{"help",	no_argument,		0,	'h'},
	{"version",	no_argument,		0,	'V'},
	{0, 0, 0, 0}
};

static const char args[] = "mcwfE:hV";

static void usage (int status)
{
	printf (_("usage: %s [-mcwfhV] [-E encoding] file ...\n"), program_name);
	printf (_(
		"-m, --man                   parse as man page.\n"
		"-c, --cat                   parse as cat page.\n"
		"-w, --whatis                show whatis information.\n"
		"-f, --filters               show guessed series of "
					    "preprocessing filters.\n"
		"-E, --encoding encoding     override character set.\n"
		"-V, --version               show version.\n"
		"-h, --help                  show this usage message.\n"
		"\n"
		"The defaults are --man and --whatis.\n"));
	exit (status);
}

int main (int argc, char **argv)
{
	int c;
	int type = 0;
	int parse_man = 0, parse_cat = 0, show_whatis = 0, show_filters = 0;
	const char *encoding = NULL;
	int some_failed = 0;

	program_name = xstrdup (basename (argv[0]));

	while ((c = getopt_long (argc, argv, args,
				 long_options, NULL)) != -1) {
		switch (c) {
			case 'm':
				parse_man = 1;
				break;
			case 'c':
				parse_cat = 1;
				break;
			case 'w':
				show_whatis = 1;
				break;
			case 'f':
				show_filters = 1;
				break;
			case 'E':
				encoding = optarg;
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
	if (parse_man) {
		if (parse_cat) {
			error (0, 0, _("-m -c: incompatible options"));
			usage (FAIL);
		} else
			type = 0;
	} else {
		if (parse_cat)
			type = 1;
		else
			type = 0;	/* default = man */
	}
	if (!show_whatis && !show_filters)
		show_whatis = 1;

	pipeline_install_sigchld ();

	while (optind != argc) {
		lexgrog lg;
		const char *file;
		int found = 0;

		lg.type = type;

		if (STREQ (argv[optind], "-"))
			file = argv[optind];
		else {
			char *path, *pathend;
			struct stat statbuf;

			path = xstrdup (argv[optind]);
			pathend = strrchr (path, '/');
			if (pathend) {
				*pathend = '\0';
				pathend = strrchr (path, '/');
				if (pathend && STRNEQ (pathend + 1, "man", 3))
					*pathend = '\0';
				else {
					free (path);
					path = NULL;
				}
			} else {
				free (path);
				path = NULL;
			}

			file = ult_src (argv[optind], path ? path : ".",
					&statbuf, SO_LINK);
			if (path)
				free (path);
		}

		if (file && find_name (file, "-", &lg, encoding)) {
			struct page_description *descs =
				parse_descriptions (NULL, lg.whatis);
			const struct page_description *desc;
			for (desc = descs; desc; desc = desc->next) {
				if (!desc->name || !desc->whatis)
					continue;
				found = 1;
				printf ("%s", argv[optind]);
				if (show_filters)
					printf (" (%s)", lg.filters);
				if (show_whatis)
					printf (": \"%s - %s\"",
						desc->name, desc->whatis);
				printf ("\n");
			}
			free_descriptions (descs);
		}

		if (!found) {
			printf ("%s: parse failed\n", argv[optind]);
			some_failed = 1;
		}

		++optind;
	}

	if (some_failed)
		return FATAL;
	else
		return OK;
}
