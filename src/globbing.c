/*
 * globbing.c: interface to the POSIX glob routines
 *  
 * Copyright (C), 1995, Graeme W. Wilford. (Wilf.)
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with this
 * distribution.
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

#include <dirent.h>

#include "manconfig.h"
#include "lib/error.h"

char *extension;
static char *mandir_layout = MANDIR_LAYOUT;

#ifdef TEST
char *program_name;
int debug = 1;
#endif

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

static int parse_layout (const char *layout)
{
	if (!*layout)
		return LAYOUT_GNU | LAYOUT_HPUX | LAYOUT_SOLARIS | LAYOUT_IRIX;
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

		return flags;
	}
}

int match_in_directory (const char *path, const char *pattern, int ignore_case,
			glob_t *pglob)
{
	DIR *dir;
	struct dirent *entry;
	int allocated = 4;
	int flags;

	pglob->gl_pathc = 0;
	pglob->gl_pathv = NULL;
	pglob->gl_offs = 0;

	dir = opendir (path);
	if (!dir) {
		if (debug)
			fprintf (stderr, "can't open directory %s: %s\n",
				 path, strerror (errno));
		return -1;
	}

	pglob->gl_pathv = xmalloc (allocated * sizeof (char *));
	flags = FNM_NOESCAPE | (ignore_case ? FNM_CASEFOLD : 0);

	for (entry = readdir (dir); entry; entry = readdir (dir)) {
		int fnm = fnmatch (pattern, entry->d_name, flags);
		if (fnm)
			continue;

		if (debug)
			fprintf (stderr, "matched: %s/%s\n",
				 path, entry->d_name);

		if (pglob->gl_pathc >= allocated) {
			allocated *= 2;
			pglob->gl_pathv = xrealloc (
				pglob->gl_pathv, allocated * sizeof (char *));
		}
		pglob->gl_pathv[pglob->gl_pathc++] =
			strappend (NULL, path, "/", entry->d_name, NULL);
	}

	if (pglob->gl_pathc >= allocated) {
		allocated *= 2;
		pglob->gl_pathv = xrealloc (pglob->gl_pathv,
					    allocated * sizeof (char *));
	}
	pglob->gl_pathv[pglob->gl_pathc] = NULL;
	closedir (dir);

	return 0;
}

char **look_for_file (const char *hier, const char *sec,
		      const char *name, int cat, int match_case)
{
	char *pattern = NULL, *path = NULL;
	static glob_t gbuf;
	int status = 1;
	static int layout = -1;

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

	/* allow lookups like "3x foo" to match "../man3/foo.3x" */

	if ((layout & LAYOUT_GNU) && isdigit (*sec) && sec[1] != '\0') {
		path = strappend (path, hier, cat ? "/cat" : "/man", "\t",
				  NULL);
		*strrchr (path, '\t') = *sec;
		pattern = end_pattern (strappend (pattern, name, NULL), sec);

		if (debug)
			fprintf (stderr, "globbing pattern in %s: %s\n",
				 path, pattern);
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

		if (debug)
			fprintf (stderr, "globbing pattern in %s: %s\n",
				 path, pattern);
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

		if (debug)
			fprintf (stderr, "globbing pattern in %s: %s\n",
				 path, pattern);
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

		if (debug)
			fprintf (stderr, "globbing pattern in %s: %s\n",
				 path, pattern);
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

		if (debug)
			fprintf (stderr, "globbing pattern in %s: %s\n",
				 path, pattern);
		status = match_in_directory (path, pattern, !match_case,
					     &gbuf);
	}

	free (path);
	free (pattern);

	if (status != 0 || gbuf.gl_pathc == 0)
		return NULL;
	else
		return gbuf.gl_pathv;
}		

#ifdef TEST
int main (int argc, char *argv[])
{
	int i;
	
	program_name = xstrdup (basename (argv[0]));
	if (argc != 4)
		error (FAIL, 0, "usage: %s path sec name", program_name);

	for (i = 0; i <= 1; i++) {
		char **files;

		files = look_for_file (argv[1], argv[2], argv[3], i);
		if (files)
			while (*files)
				printf ("%s\n", *files++);
	}
	return 0;
}
#endif /* TEST */
