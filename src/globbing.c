/*
 * globbing.c: interface to the POSIX glob routines
 *  
 * Copyright (C) 1995 Graeme W. Wilford. (Wilf.)
 * Copyright (C) 2001, 2002, 2003 Colin Watson.
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
 *
 * Mon Mar 13 20:27:36 GMT 1995  Wilf. (G.Wilford@ee.surrey.ac.uk) 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

/* Need _GNU_SOURCE for FNM_CASEFOLD. */
#define _GNU_SOURCE

#include <stdio.h>

#if defined(STDC_HEADERS)
#  include <string.h>
#  include <stdlib.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#else /* no string(s) header */
extern char *strrchr();
#endif /* STDC_HEADERS */

#include <ctype.h>

#ifdef HAVE_GLOB_H
#  include <glob.h>
#else 
#  include "lib/glob.h"
#endif

#ifdef HAVE_FNMATCH_H
#  include <fnmatch.h>
#else
#  include "lib/fnmatch.h"
#endif

#include <sys/types.h>
#include <dirent.h>

#include "manconfig.h"
#include "lib/error.h"
#include "lib/hashtable.h"
#include "globbing.h"

const char *extension;
static const char *mandir_layout = MANDIR_LAYOUT;

#ifdef TEST

#  ifdef HAVE_GETOPT_H
#    include <getopt.h>
#  else /* !HAVE_GETOPT_H */
#    include "lib/getopt.h"
#  endif /* HAVE_GETOPT_H */

char *program_name;
int debug = 0;

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

#endif /* TEST */

static __inline__ char *end_pattern (char *pattern, const char *sec)
{
	if (extension)
		pattern = strappend (pattern, ".*", extension, "*", NULL);
	else
		pattern = strappend (pattern, ".", sec, "*", NULL);

	return pattern;
}

#define LAYOUT_GNU	1
#define LAYOUT_HPUX	2
#define LAYOUT_IRIX	4
#define LAYOUT_SOLARIS	8
#define LAYOUT_BSD	16

static int parse_layout (const char *layout)
{
	if (!*layout)
		return LAYOUT_GNU | LAYOUT_HPUX | LAYOUT_IRIX |
		       LAYOUT_SOLARIS | LAYOUT_BSD;
	else {
		int flags = 0;

		char *upper_layout = xstrdup (layout);
		char *layoutp;
		for (layoutp = upper_layout; *layoutp; layoutp++)
			*layoutp = toupper (*layoutp);

		if (strstr (layout, "GNU"))
			flags |= LAYOUT_GNU;
		if (strstr (layout, "HPUX"))
			flags |= LAYOUT_HPUX;
		if (strstr (layout, "IRIX"))
			flags |= LAYOUT_IRIX;
		if (strstr (layout, "SOLARIS"))
			flags |= LAYOUT_SOLARIS;
		if (strstr (layout, "BSD"))
			flags |= LAYOUT_BSD;

		return flags;
	}
}

struct dirent_hashent {
	char **names;
	size_t names_len, names_max;
};

static void dirent_hash_free (void *defn)
{
	struct dirent_hashent *hashent = defn;
	size_t i;

	for (i = 0; i < hashent->names_len; ++i)
		free (hashent->names[i]);
	free (hashent->names);
	free (hashent);
}

static struct hashtable *dirent_hash = NULL;

static struct dirent_hashent *update_directory_cache (const char *path)
{
	struct dirent_hashent *cache;
	DIR *dir;
	struct dirent *entry;

	if (!dirent_hash)
		dirent_hash = hash_create (&dirent_hash_free);
	cache = hash_lookup (dirent_hash, path, strlen (path));

	/* Check whether we've got this one already. */
	if (cache) {
		if (debug)
			fprintf (stderr, "update_directory_cache %s: hit\n",
				 path);
		return cache;
	}

	if (debug)
		fprintf (stderr, "update_directory_cache %s: miss\n", path);

	dir = opendir (path);
	if (!dir) {
		if (debug)
			fprintf (stderr, "can't open directory %s: %s\n",
				 path, strerror (errno));
		return NULL;
	}

	cache = xmalloc (sizeof (struct dirent_hashent));
	cache->names_len = 0;
	cache->names_max = 1024;
	cache->names = xmalloc (sizeof (char *) * cache->names_max);

	/* Dump all the entries into cache->names, resizing if necessary. */
	for (entry = readdir (dir); entry; entry = readdir (dir)) {
		if (cache->names_len >= cache->names_max) {
			cache->names_max *= 2;
			cache->names =
				xrealloc (cache->names,
					  sizeof (char *) * cache->names_max);
		}
		cache->names[cache->names_len++] = xstrdup (entry->d_name);
	}

	hash_install (dirent_hash, path, strlen (path), cache);
	closedir (dir);

	return cache;
}

static int match_in_directory (const char *path, const char *pattern,
			       int ignore_case, glob_t *pglob)
{
	struct dirent_hashent *cache;
	size_t allocated = 4;
	int flags;
	size_t i;

	pglob->gl_pathc = 0;
	pglob->gl_pathv = NULL;
	pglob->gl_offs = 0;

	cache = update_directory_cache (path);
	if (!cache) {
		if (debug)
			fprintf (stderr, "directory cache update failed\n");
		return -1;
	}

	if (debug)
		fprintf (stderr, "globbing pattern in %s: %s\n",
			 path, pattern);

	pglob->gl_pathv = xmalloc (allocated * sizeof (char *));
	flags = ignore_case ? FNM_CASEFOLD : 0;

	for (i = 0; i < cache->names_len; ++i) {
		int fnm = fnmatch (pattern, cache->names[i], flags);
		if (fnm)
			continue;

		if (debug)
			fprintf (stderr, "matched: %s/%s\n",
				 path, cache->names[i]);

		if (pglob->gl_pathc >= allocated) {
			allocated *= 2;
			pglob->gl_pathv = xrealloc (
				pglob->gl_pathv, allocated * sizeof (char *));
		}
		pglob->gl_pathv[pglob->gl_pathc++] =
			strappend (NULL, path, "/", cache->names[i], NULL);
	}

	if (pglob->gl_pathc >= allocated) {
		allocated *= 2;
		pglob->gl_pathv = xrealloc (pglob->gl_pathv,
					    allocated * sizeof (char *));
	}
	pglob->gl_pathv[pglob->gl_pathc] = NULL;

	return 0;
}

char **look_for_file (const char *unesc_hier, const char *sec,
		      const char *unesc_name, int cat, int match_case)
{
	char *pattern = NULL, *path = NULL;
	static glob_t gbuf;
	int status = 1;
	static int layout = -1;
	char *hier, *name;

	/* As static struct is allocated and contains NULLs we don't need 
	   to check it before attempting a free. Let globfree() do that */

	globfree (&gbuf);

	/* This routine only does a minimum amount of matching. It does not
	   find cat files in the alternate cat directory. */

	if (layout == -1) {
		layout = parse_layout (mandir_layout);
		if (debug)
			fprintf (stderr, "Layout is %s (%d)\n",
				 mandir_layout, layout);
	}

	hier = escape_shell (unesc_hier);
	name = escape_shell (unesc_name);

	/* allow lookups like "3x foo" to match "../man3/foo.3x" */

	if ((layout & LAYOUT_GNU) && isdigit (*sec) && sec[1] != '\0') {
		path = strappend (path, hier, cat ? "/cat" : "/man", "\t",
				  NULL);
		*strrchr (path, '\t') = *sec;
		pattern = end_pattern (strappend (pattern, name, NULL), sec);

		status = match_in_directory (path, pattern, !match_case,
					     &gbuf);
	}

	/* AIX glob.h doesn't define GLOB_NOMATCH and the manpage is vague
	   regarding return status if there are no matches so check the
	   path count member also */
	   
	if ((layout & LAYOUT_GNU) && (status != 0 || gbuf.gl_pathc == 0)) {
		if (path)
			*path = '\0';
		if (pattern)
			*pattern = '\0';
		path = strappend (path, hier, cat ? "/cat" : "/man", sec,
				  NULL);
		pattern = end_pattern (strappend (pattern, name, NULL), sec);

		status = match_in_directory (path, pattern, !match_case,
					     &gbuf);
	}

	/* Try HPUX style compressed man pages */
	if ((layout & LAYOUT_HPUX) && (status != 0 || gbuf.gl_pathc == 0)) {
		if (path)
			*path = '\0';
		if (pattern)
			*pattern = '\0';
		path = strappend (path, hier, cat ? "/cat" : "/man",
				  sec, ".Z", NULL);
		pattern = end_pattern (strappend (pattern, name, NULL), sec);

		status = match_in_directory (path, pattern, !match_case,
					     &gbuf);
	}

	/* Try man pages without the section extension --- IRIX man pages */
	if ((layout & LAYOUT_IRIX) && (status != 0 || gbuf.gl_pathc == 0)) {
		if (path)
			*path = '\0';
		if (pattern)
			*pattern = '\0';
		path = strappend (path, hier, cat ? "/cat" : "/man", sec,
				  NULL);
		pattern = strappend (pattern, name, ".*", NULL);

		status = match_in_directory (path, pattern, !match_case,
					     &gbuf);
	}

	/* Try Solaris style man page directories */
	if ((layout & LAYOUT_SOLARIS) && (status != 0 || gbuf.gl_pathc == 0)) {
		if (path)
			*path = '\0';
		if (pattern)
			*pattern = '\0';
		/* TODO: This needs to be man/sec*, not just man/sec. */
		path = strappend (path, hier, cat ? "/cat" : "/man", sec,
				  NULL);
		pattern = end_pattern (strappend (pattern, name, NULL), sec);

		status = match_in_directory (path, pattern, !match_case,
					     &gbuf);
	}

	/* BSD cat pages take the extension .0 */
	if ((layout & LAYOUT_BSD) && (status != 0 || gbuf.gl_pathc == 0)) {
		if (path)
			*path = '\0';
		if (pattern)
			*pattern = '\0';
		if (cat) {
			path = strappend (path, hier, "/cat", sec, NULL);
			pattern = strappend (pattern, name, ".0*", NULL);
		} else {
			path = strappend (path, hier, "/man", sec, NULL);
			pattern = end_pattern (strappend (pattern, name, NULL),
					       sec);
		}
		status = match_in_directory (path, pattern, !match_case,
					     &gbuf);
	}

	free (name);
	free (hier);
	free (path);
	free (pattern);

	if (status != 0 || gbuf.gl_pathc == 0)
		return NULL;
	else
		return gbuf.gl_pathv;
}		

#ifdef TEST

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
	int c, option_index;
	int i;
	int match_case = 0;

	while ((c = getopt_long (argc, argv, args,
				 long_options, &option_index)) != -1) {
		switch (c) {
			case 'd':
				debug = 1;
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

	program_name = xstrdup (basename (argv[0]));
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

#endif /* TEST */
