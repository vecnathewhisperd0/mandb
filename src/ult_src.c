/*
 * ult_src.c: Find the ultimate source of a page
 *
 * Copyright (C) 1994, 1995 Graeme W. Wilford. (Wilf.)
 * Copyright (C) 2001, 2002 Colin Watson.
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
 * code to seek out the original (ultimate) source man file for
 * any specified man file. Soft and hard links and .so inclusions
 * are traced. Use: reduce amount of cat files to a minimum.
 *
 * Mon May  2 11:14:28 BST 1994 Wilf. (G.Wilford@ee.surrey.ac.uk)
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

#ifndef STDC_HEADERS
extern int errno;
#endif

#ifndef S_ISLNK /* losing sys/stat.h */
#  if defined(S_IFMT) && defined(S_IFLNK)
#    define S_ISLNK(mode) (((mode) & S_IFMT) == S_IFLNK)
#  endif
#endif

#ifdef HAVE_DIRENT_H
#  include <dirent.h>
#else /* not HAVE_DIRENT_H */
#  define dirent direct
#  ifdef HAVE_SYS_NDIR_H
#    include <sys/ndir.h>
#  endif /* HAVE_SYS_NDIR_H */
#  ifdef HAVE_SYS_DIR_H
#    include <sys/dir.h>
#  endif /* HAVE_SYS_DIR_H */
#  ifdef HAVE_NDIR_H
#    include <ndir.h>
#  endif /* HAVE_NDIR_H */
#endif /* HAVE_DIRENT_H  */

#if defined(STDC_HEADERS)
#  include <string.h>
#  include <stdlib.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#else /* no string(s) header */
extern char *strrchr();
extern char *realpath();
#endif /* no string(s) header */

#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if defined(HAVE_LIMITS_H) && defined(_POSIX_VERSION)
#  include <limits.h>                     /* for PATH_MAX */
#else /* !(HAVE_LIMITS_H && _POSIX_VERSION) */
#  include <sys/param.h>                  /* for MAXPATHLEN */
#endif /* HAVE_LIMITS_H */

#ifndef PATH_MAX
#  if defined(_POSIX_VERSION) && defined(_POSIX_PATH_MAX)
#    define PATH_MAX _POSIX_PATH_MAX
#  else /* !_POSIX_VERSION */
#    ifdef MAXPATHLEN
#      define PATH_MAX MAXPATHLEN
#    else /* !MAXPATHLEN */
#      define PATH_MAX 1024
#    endif /* MAXPATHLEN */
#  endif /* _POSIX_VERSION */
#endif /* !PATH_MAX */

#include <libintl.h>
#define _(String) gettext (String)

#include "manconfig.h"
#include "lib/error.h"
#include "security.h"
#include "ult_src.h"

/* Find minimum value hard link filename for given file and inode.
 * Returns a newly allocated string.
 */
static char *ult_hardlink (const char *fullpath, ino_t inode)
{
	DIR *mdir;
	struct dirent *manlist;
	char *link, *dir, *ret;
	const char *slash;

	slash = strrchr (fullpath, '/');
	assert (slash);
	dir = xstrndup (fullpath, slash - fullpath);
	link = xstrdup (++slash);

	mdir = opendir (dir);
	if (mdir == NULL) {
		error (0, errno, _("can't search directory %s"), dir);
		free (dir);
		free (link);
		return NULL;
	}

	while ((manlist = readdir (mdir))) {
		if (manlist->d_ino == inode &&
		    strcmp (link, manlist->d_name) > 0) {
			free (link);
			link = xstrdup (manlist->d_name);
			if (debug)
				fprintf (stderr, "ult_hardlink: (%s)\n", link);
		}
	}
	closedir (mdir);

	/* If we already are the link with the smallest name value */
	/* return NULL */

	if (strcmp (link, slash) == 0) {
		free (dir);
		free (link);
		return NULL;
	}

	ret = strappend (NULL, dir, "/", link, NULL);
	free (dir);
	free (link);
	return ret;
}

#ifdef S_ISLNK
/* Use realpath() to resolve all sym links within 'fullpath'.
 * Returns a newly allocated string.
 */
static char *ult_softlink (const char *fullpath)
{
	char resolved_path[PATH_MAX];

	if (realpath (fullpath, resolved_path) == NULL) {
		/* discard the unresolved path */
		if (errno == ENOENT)
			error (0, 0, _("warning: %s is a dangling symlink"),
			       fullpath);
		else
			error (0, errno, _("can't resolve %s"), fullpath);
		return NULL;
	}

	if (debug)
		fprintf (stderr, "ult_softlink: (%s)\n", resolved_path);

	return xstrdup (resolved_path);
}
#endif /* S_ISLNK */

/* Test 'buffer' to see if it contains a .so include. If so and it's not an 
 * absolute filename, return newly allocated string whose contents are the
 * include.
 */
static char *test_for_include (const char *buffer)
{
	/* strip out any leading whitespace (if any) */
	while (CTYPE (isspace, *buffer))
		buffer++;

	/* see if the `command' is a .so */
	if (strncmp (buffer, ".so", 3) == 0) {
		buffer += 3;

		/* strip out any whitespace between the command and 
		   it's argumant */
		while (CTYPE (isspace, *buffer))
			buffer++;

		/* If .so's argument is an absolute filename, it could be
		 * either (i) a macro inclusion, (ii) a non local manual page
		 * or (iii) a (somewhat bogus) reference to a local manual 
		 * page.
		 * 
		 * If (i) or (ii), we must not follow the reference. (iii) is
		 * a problem with the manual page, thus we don't want to 
		 * follow any absolute inclusions in our quest for the 
		 * ultimate source file */
		if (*buffer != '/') {
			const char *end = buffer;
			while (*end && !CTYPE (isspace, *end))
				++end;
			return xstrndup (buffer, end - buffer);
		}
	}
	return NULL;
}

/*
 * recursive function which finds the ultimate source file by following
 * any ".so filename" directives in the first line of the man pages.
 * Also (optionally) traces symlinks and hard links(!).
 *
 * name is full pathname, path is the MANPATH directory (/usr/man)
 * flags is a combination of SO_LINK | SOFT_LINK | HARD_LINK
 */
const char *ult_src (const char *name, const char *path,
		     struct stat *buf, int flags)
{
	static char *basename;		/* must be static */
	static short recurse; 		/* must be static */

	/* initialise the function */

	/* as ult_softlink() & ult_hardlink() do all of their respective
	 * resolving in one call, only need to sort them out once
	 */

	if (recurse == 0) {
		struct stat new_buf;
		if (basename)
			free (basename);
		basename = xstrdup (name);

		if (debug)
			fprintf (stderr, "\nult_src: File %s in mantree %s\n",
				 name, path);

		/* If we don't have a buf, allocate and assign one */
		if (!buf && ((flags & SOFT_LINK) || (flags & HARD_LINK))) {
			buf = &new_buf;
			if (lstat (basename, buf) == -1) {
				error (0, errno, _("can't resolve %s"),
				       basename);
				return NULL;
			}
		}

#ifdef S_ISLNK
		/* Permit semi local (inter-tree) soft links */
		if (flags & SOFT_LINK) {
			if (S_ISLNK (buf->st_mode)) {
				/* Is a symlink, resolve it. */
				char *softlink = ult_softlink (basename);
				if (softlink) {
					free (basename);
					basename = softlink;
				} else
					return NULL;
			}
		}
#endif /* S_ISLNK */

		/* Only deal with local (inter-dir) HARD links */
		if (flags & HARD_LINK) {
			if (buf->st_nlink > 1) {
				/* Has HARD links, find least value */
				char *hardlink = ult_hardlink (basename,
							       buf->st_ino);
				if (hardlink) {
					free (basename);
					basename = hardlink;
				}
			}
		}
	}

	/* keep a check on recursion level */
	else if (recurse == 10) {
		error (0, 0, _("%s is self referencing"), name);
		return NULL;
	}

	if (flags & SO_LINK) {
		char buffer[1024], *bptr;
		FILE *fp;
#ifdef COMP_SRC
		struct compression *comp;

		/* get rid of the previous ztemp file (if any) */
		remove_ztemp ();

		/* if we are handed the name of a compressed file, remove
		   the compression extension? */
		comp = comp_info (basename, 1);
		if (comp) {
			free (basename);
			basename = comp->stem;
			comp->stem = NULL; /* steal memory */
		}

		/* if the open fails, try looking for compressed */
		fp = fopen (basename, "r");
		if (fp == NULL) {
			char *filename;

			comp = comp_file (basename);
			if (comp) {
				filename = decompress (comp->stem, comp);
				free (comp->stem);
				if (!filename)
					return NULL;
				basename = strappend (basename, ".", comp->ext,
						      NULL);
				drop_effective_privs ();
				fp = fopen (filename, "r");
				regain_effective_privs ();
			} else
				filename = basename;

			if (!fp) {
				error (0, errno, _("can't open %s"), filename);
				return NULL;
			}
		}
#else
		fp = fopen (basename, "r");
		if (fp == NULL) {
			error (0, errno, _("can't open %s"), basename);
			return NULL;
		}
#endif
		/* make sure that we skip over any comments */
		do {
			bptr = fgets (buffer, 1024, fp);
		} while (bptr && STRNEQ (buffer, ".\\\"", 3));

		fclose(fp);

		if (buffer) {
			char *include = test_for_include (buffer);
			if (include) {
				const char *ult;

				/* Restore the original path from before
				 * ult_softlink() etc., in case it went
				 * outside the mantree.
				 */
				free (basename);
				basename = strappend (NULL, path, "/", include,
						      NULL);
				free (include);

				if (debug)
					fprintf (stderr,
						 "ult_src: points to %s\n",
						 basename);

				recurse++;
				ult = ult_src (basename, path, NULL, flags);
				recurse--;

				return ult;
			}
		}
	}

	/* We have the ultimate source */
	return basename;
}
