/*
 * check_mandirs.c: used to auto-update the database caches
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
 * Mon May  2 17:36:33 BST 1994  Wilf. (G.Wilford@ee.surrey.ac.uk)
 *
 * CJW: Many changes to whatis parsing. Added database purging.
 * See docs/ChangeLog for details.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

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

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if defined(STDC_HEADERS)
#  include <string.h>
#  include <stdlib.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#else /* no string(s) header */
extern char *strchr(), *strrchr(), *strstr(), *strpbrk();
#endif /* no string(s) header */

#ifndef STDC_HEADERS
extern time_t time();
extern int errno;
#endif

#ifdef HAVE_LIBGEN_H
#  include <libgen.h>
#endif /* HAVE_LIBGEN_H */

#include <libintl.h>
#define _(String) gettext (String)

#include "manconfig.h"
#include "libdb/mydbm.h"
#include "libdb/db_storage.h"
#include "lib/error.h"
#include "lib/hashtable.h"
#include "descriptions.h"
#include "filenames.h"
#include "globbing.h"
#include "manp.h"
#include "ult_src.h"
#include "security.h"
#include "check_mandirs.h"

int opt_test;		/* don't update db */
int pages;
int force_rescan = 0;

static struct hashtable *whatis_hash = NULL;

static void gripe_multi_extensions (const char *path, const char *sec, 
				    const char *name, const char *ext)
{
	if (quiet < 2)
		error (0, 0,
		       _("warning: %s/man%s/%s.%s*: competing extensions"),
		       path, sec, name, ext);
}

static void gripe_rwopen_failed (const char *database)
{
	if (errno == EACCES || errno == EROFS) {
		if (debug)
			fprintf (stderr, "database %s is read-only\n",
				 database);
	} else {
#ifdef MAN_DB_UPDATES
		if (!quiet)
#endif /* MAN_DB_UPDATES */
			error (0, errno, _("can't update index cache %s"),
			       database);
	}
}

/* take absolute filename and path (for ult_src) and do sanity checks on 
   file. Also check that file is non-zero in length and is not already in
   the db. If not, find its ult_src() and see if we have the whatis cached, 
   otherwise cache it in case we trace another manpage back to it. Next,
   store it in the db along with any references found in the whatis. */
void test_manfile (const char *file, const char *path)
{
	char *base_name;
	const char *ult;
	struct lexgrog lg;
	char *manpage;
	struct mandata info, *exists;
	struct stat buf;
	size_t len;

	memset (&lg, 0, sizeof (struct lexgrog));
	memset (&info, 0, sizeof (struct mandata));

	manpage = filename_info (file, &info, NULL);
	if (!manpage)
		return;
	base_name = manpage + strlen (manpage) + 1;

	len  = strlen (manpage) + 1;		/* skip over directory name */
	len += strlen (manpage + len) + 1;	/* skip over base name */
	len += strlen (manpage + len);		/* skip over section ext */

	/* to get mtime info */
	(void) lstat (file, &buf);
	info._st_mtime = buf.st_mtime;

	/* check that our file actually contains some data */
	if (buf.st_size == 0) {
		/* man-db pre 2.3 place holder ? */
		free (manpage);
		return;
	}

	/* see if we already have it, before going any further, this will
	   save both an ult_src() a find_name(), amongst other time wastes */
	exists = dblookup_exact (base_name, info.ext, 1);

	/* Ensure we really have the actual page. Gzip keeps the mtime
	   the same when it compresses, so we have to compare comp 
	   extensions also */

	if (exists) {
		if (strcmp (exists->comp, info.comp ? info.comp : "-") == 0) {
			if (exists->_st_mtime == info._st_mtime 
			    && exists->id < WHATIS_MAN) {
				free_mandata_struct (exists);
				free (manpage);
				return;
			}
		} else {
			struct stat physical;
			char *abs_filename;
			
			/* see if the cached file actually exists. It's 
			   evident at this point that we have multiple 
			   comp extensions */
			abs_filename = make_filename (path, NULL,
						      exists, "man");
			if (debug)
				fprintf (stderr, "test_manfile(): stat %s\n",
					 abs_filename);
			if (stat (abs_filename, &physical) == -1) {
				if (!opt_test)
					dbdelete (base_name, exists);
			} else {
				gripe_multi_extensions (path, exists->sec,
							base_name, exists->ext);
				free_mandata_struct (exists);
				free (manpage);
				return;
			}
		}
		free_mandata_struct (exists);
	}

	/* Check if it happens to be a symlink/hardlink to something already
	   in our cache. This just does some extra checks to avoid scanning
	   links quite so many times. */
	{
		/* Avoid too much noise in debug output */
		int save_debug = debug;
		debug = 0;
		ult = ult_src (file, path, &buf, SOFT_LINK | HARD_LINK);
		debug = save_debug;
	}

	if (!ult) {
		/* already warned about this, don't do so again */
		if (debug)
			fprintf (stderr,
				 "test_manfile(): bad link %s\n", file);
		free (manpage);
		return;
	}

	if (!whatis_hash)
		whatis_hash = hash_create (&plain_hash_free);

	if (hash_lookup (whatis_hash, ult, strlen (ult)) == NULL) {
		if (debug && strncmp (ult, file, len) != 0)
			fprintf (stderr,
				 "\ntest_manfile(): link not in cache:\n"
				 " source = %s\n"
				 " target = %s\n", file, ult);
		/* Trace the file to its ultimate source, else we'll be
		   looking for whatis info in files containing only
		   '.so manx/foo.x', which will give us an unobtainable
		   whatis for the entry. */
		ult = ult_src (file, path, &buf,
			       SO_LINK | SOFT_LINK | HARD_LINK);
	}

	if (!ult) {
		error (0, 0,
		       _("warning: %s: bad symlink or ROFF `.so' request"),
		       file);
		free (manpage);
		return;
	}

	pages++;			/* pages seen so far */

	if (strncmp (ult, file, len) == 0)
		info.id = ULT_MAN;	/* ultimate source file */
	else
		info.id = SO_MAN;	/* .so, sym or hard linked file */

	/* Ok, here goes: Use a hash tree to store the ult_srcs with
	 * their whatis. Anytime after, check the hash tree, if it's there, 
	 * use it. This saves us a find_name() which is a real hog.
	 *
	 * Use the full path in ult as the hash key so we don't have to
	 * clear the hash between calls.
	 */

	lg.whatis = xstrdup (hash_lookup (whatis_hash, ult, strlen (ult)));

	if (!lg.whatis) {	/* cache miss */
		/* go get the whatis info in its raw state */
		char *file_copy = xstrdup (file);
#ifdef COMP_SRC
		/* if the nroff was compressed, an uncompressed version is
		   shown by a call to get_ztemp(), grog this for a whatis
		   rather than ult. This is a bit difficult to follow, sorry:
		   ult_src() will leave the last uncompressed nroff file it
		   has to deal with in get_ztemp() */
		char *ztemp;
#endif /* COMP_SRC */

		lg.type = MANPAGE;
		drop_effective_privs ();
#ifdef COMP_SRC
		ztemp = get_ztemp ();
		if (ztemp) {
			find_name (ztemp, basename (file_copy), &lg);
			remove_ztemp ();  /* get rid of temp file identifier */
		} else
#endif /* COMP_SRC */
			find_name (ult, basename (file_copy), &lg);
		free (file_copy);
		regain_effective_privs ();

		hash_install (whatis_hash, ult, strlen (ult),
			      xstrdup (lg.whatis));
	}

	if (debug)
		fprintf (stderr, "\"%s\"\n", lg.whatis);

	/* split up the raw whatis data and store references */
	info.pointer = NULL;	/* direct page, so far */
	info.filter = lg.filters;
	if (lg.whatis) {
		struct page_description *descs =
			parse_descriptions (base_name, lg.whatis);
		if (descs) {
			if (!opt_test)
				store_descriptions (descs, &info, base_name);
			free_descriptions (descs);
		}
	} else if (quiet < 2) {
		(void) stat (ult, &buf);
		if (buf.st_size == 0)
			error (0, 0, _("warning: %s: ignoring empty file"),
			       ult);
		else
			error (0, 0,
			       _("warning: %s: whatis parse for %s(%s) failed"),
			       ult, base_name, info.ext);
	}

	free (manpage);
	if (lg.whatis)
		free (lg.whatis);
}

static __inline__ void add_dir_entries (const char *path, char *infile)
{
	char *manpage;
	int len;
	struct dirent *newdir;
	DIR *dir;

	manpage = strappend (NULL, path, "/", infile, "/", NULL);
	len = strlen (manpage);

	/*
	 * All filename entries in this dir should either be valid manpages
	 * or . files (such as current, parent dir).
	 */

	dir = opendir (infile);
	if (!dir) {
		error (0, errno, _("can't search directory %s"), manpage);
		free (manpage);
                return;
        }
        
        /* strlen(newdir->d_name) could be replaced by newdir->d_reclen */
        
	while ( (newdir = readdir (dir)) )
		if (!(*newdir->d_name == '.' && 
		      strlen (newdir->d_name) < (size_t) 3)) {
			manpage = strappend (manpage, newdir->d_name, NULL);
			test_manfile (manpage, path);
			*(manpage + len) = '\0';
		}
		
	free (manpage);
	closedir (dir);
}

/*
 * accepts the raw man dir tree eg. "/usr/man" and the time stored in the db
 * any dirs of the tree that have been modified (ie added to) will then be
 * scanned for new files, which are then added to the db.
 */
static short testmandirs (const char *path, time_t last)
{
	DIR *dir;
	struct dirent *mandir;
	struct stat stbuf;
	short amount = 0;

	if (debug)
		fprintf (stderr, "Testing %s for new files\n", path);

	dir = opendir (path);
	if (!dir) {
		error (0, errno, _("can't search directory %s"), path);
		return 0;
	}

	chdir (path);

	while( (mandir = readdir (dir)) ) {
		if (strncmp (mandir->d_name, "man", 3) != 0)
			continue;

		if (debug)
			fprintf (stderr, "Examining %s\n", mandir->d_name);

		if (stat (mandir->d_name, &stbuf) != 0)	/* stat failed */
			continue;
		if (!S_ISDIR(stbuf.st_mode))		/* not a directory */
			continue;
		if (stbuf.st_mtime <= last) {
			/* scanned already */
			if (debug)
				fprintf (stderr,
					 "%s modified %ld, db modified %ld\n",
					 mandir->d_name, stbuf.st_mtime, last);
			continue;
		}

		if (debug)
			fprintf (stderr,
			  "\tsubdirectory %s has been 'modified'\n",
			  mandir->d_name);

		dbf = MYDBM_RWOPEN(database);

		if (!dbf) {
			gripe_rwopen_failed (database);
			return 0;
		}

		if (!quiet) {
			fprintf (stderr, "\r");
			fprintf (stderr,
				 _("Updating index cache for path "
				   "`%s/%s'. Wait..."), path, mandir->d_name);
		}
		add_dir_entries (path, mandir->d_name);
		MYDBM_CLOSE (dbf);
		amount++;
	}
	closedir (dir);

	return amount;
}

/* update the time key stored within `database' */
void update_db_time (void)
{
	datum key, content;
#ifdef FAST_BTREE
	datum key1, content1;
#endif /* FAST_BTREE */

	key.dptr = xstrdup (KEY);
	key.dsize = sizeof KEY;
	content.dptr = (char *) xmalloc (16); /* 11 is max long with '\0' */
	(void) sprintf (content.dptr, "%ld", (long) time (NULL));
	content.dsize = strlen (content.dptr) + 1;

	/* Open the db in RW to store the $mtime$ ID */
	/* we know that this should succeed because we just updated the db! */
	dbf = MYDBM_RWOPEN (database);
	if (dbf == NULL) {
#ifdef MAN_DB_UPDATES
		if (!quiet)
#endif /* MAN_DB_UPDATES */
			error (0, errno, _("can't update index cache %s"),
			       database);
		free (content.dptr);
		return;
	}
#ifndef FAST_BTREE
	MYDBM_REPLACE (dbf, key, content);
#else /* FAST_BTREE */
	key1.dptr = KEY;
	key1.dsize = sizeof KEY;

	(dbf->seq) (dbf, (DBT *) &key1, (DBT *) &content1, R_CURSOR);
	
	if (strcmp (key1.dptr, key.dptr) == 0)
		(dbf->put) (dbf, (DBT *) &key, (DBT *) &content, R_CURSOR);
	else
		(dbf->put) (dbf, (DBT *) &key, (DBT *) &content, 0);
#endif /* !FAST_BTREE */

	MYDBM_CLOSE (dbf);
	free (key.dptr);
	free (content.dptr);
}

/* remove the db's time key - called prior to update_db if we want
   to `force' a full consistency check */
void reset_db_time (void)
{
	datum key;

	key.dptr = xstrdup (KEY);
	key.dsize = sizeof KEY;

	/* we don't really care if we can't open it RW - it's not fatal */
	dbf = MYDBM_RWOPEN (database);
	if (dbf == NULL) {
		if (debug) {
			fprintf (stderr, "reset_db_time(): ");
			perror ("can't open db");
		}
		return;
	}

	MYDBM_DELETE (dbf, key);
	if (debug)
		fprintf (stderr, "reset_db_time()\n");
	MYDBM_CLOSE (dbf);
	free (key.dptr);
}

/* routine to prepare/create the db prior to calling testmandirs() */
short create_db (const char *manpath)
{
	short amount;
	
	if (debug)
		fprintf (stderr, "create_db(%s): %s\n", manpath, database);

	/* Open the db in CTRW mode to store the $ver$ ID */

	dbf = MYDBM_CTRWOPEN (database);
	if (dbf == NULL) {
		if (errno == EACCES || errno == EROFS) {
			if (debug)
				fprintf (stderr, "database %s is read-only\n",
					 database);
		} else
			error (0, errno, _("can't create index cache %s"),
			       database);
		return 0;
		/* should really return EOF */
	}

	dbver_wr (dbf);
	MYDBM_CLOSE (dbf);

	amount = testmandirs (manpath, (time_t) 0);

	if (amount) {
		update_db_time ();
		if (!quiet)
			fputs (_("done.\n"), stderr);
	}

	return amount;
}

/* routine to update the db, ensure that it is consistent with the 
   filesystem */
short update_db (const char *manpath)
{
	dbf = MYDBM_RDOPEN (database);
	if (dbf && dbver_rd (dbf)) {
		MYDBM_CLOSE (dbf);
		dbf = NULL;
	}
	if (dbf) {
		datum key, content;
		short new;

		key.dptr = xstrdup (KEY);
		key.dsize = sizeof KEY;
		content = MYDBM_FETCH (dbf, key);
		MYDBM_CLOSE (dbf);
		free (key.dptr);

		if (debug)
			fprintf (stderr, "update_db(): %ld\n",
				 content.dptr ? atol (content.dptr) : 0L);
		if (content.dptr) {
			new = testmandirs (manpath,
					   (time_t) atol (content.dptr));
			MYDBM_FREE (content.dptr);
		} else
			new = testmandirs (manpath, (time_t) 0);

		if (new) {
			update_db_time ();
			if (!quiet)
				fputs (_("done.\n"), stderr);
		}
		
		return new;
	}
		
	if (debug)
		fprintf (stderr, "failed to open %s O_RDONLY\n", database);
		
	return EOF;
}

/* Purge any entries pointing to name. This currently assumes that pointers
 * are always shallow, which may not be a good assumption yet; it should be
 * close, though.
 *
 * Assumes that the appropriate database is already open on dbf.
 */
void purge_pointers (const char *name)
{
	datum key = MYDBM_FIRSTKEY (dbf);

	if (debug)
		fprintf (stderr, "Purging pointers to vanished page \"%s\"\n",
			 name);

	while (key.dptr != NULL) {
		datum content, nextkey;
		struct mandata entry;
		char *nicekey, *tab;

		/* Ignore db identifier keys. */
		if (*key.dptr == '$')
			goto pointers_next;

		content = MYDBM_FETCH (dbf, key);
		if (!content.dptr)
			return;

		/* Get just the name. */
		nicekey = xstrdup (key.dptr);
		tab = strchr (nicekey, '\t');
		if (tab)
			*tab = '\0';

		if (*content.dptr == '\t')
			goto pointers_contentnext;

		split_content (content.dptr, &entry);
		if (entry.id != SO_MAN && entry.id != WHATIS_MAN)
			goto pointers_contentnext;

		if (STREQ (entry.pointer, name)) {
			if (!opt_test)
				dbdelete (nicekey, &entry);
			else if (debug)
				fprintf (stderr,
					 "%s(%s): pointer vanished, "
					 "would delete\n", nicekey, entry.ext);
		}

pointers_contentnext:
		free (nicekey);
		MYDBM_FREE (content.dptr);
pointers_next:
		nextkey = MYDBM_NEXTKEY (dbf, key);
		MYDBM_FREE (key.dptr);
		key = nextkey;
	}
}

/* Count the number of exact extension matches returned from look_for_file()
 * (which may return inexact extension matches in some cases). It may turn
 * out that this is better handled in look_for_file() itself.
 */
static int count_glob_matches (const char *name, const char *ext,
			       char **source)
{
	char **walk;
	int count = 0;

	for (walk = source; walk && *walk; ++walk) {
		struct mandata info;
		struct stat statbuf;
		char *buf;

		memset (&info, 0, sizeof (struct mandata));

		if (stat (*walk, &statbuf) == -1) {
			if (debug)
				fprintf (stderr,
					 "count_glob_matches: excluding %s "
					 "because stat failed\n",
					 *walk);
			continue;
		}

		buf = filename_info (*walk, &info, name);
		if (buf) {
			if (STREQ (ext, info.ext))
				++count;
			free (buf);
		}
	}

	return count;
}

/* Decide whether to purge a reference to a "normal" (ULT_MAN or SO_MAN)
 * page.
 */
static short purge_normal (const char *name, struct mandata *info,
			   char **found)
{
	/* TODO: On some systems, the cat page extension differs from the
	 * man page extension, so this may be too strict.
	 */
	if (count_glob_matches (name, info->ext, found))
		return 0;

	if (!opt_test)
		dbdelete (name, info);
	else if (debug)
		fprintf (stderr, "%s(%s): missing page, would delete\n",
			 name, info->ext);

	return 1;
}

/* Decide whether to purge a reference to a WHATIS_MAN or WHATIS_CAT page. */
static short purge_whatis (const char *path, int cat, const char *name,
			   struct mandata *info, char **found)
{
	/* TODO: On some systems, the cat page extension differs from the
	 * man page extension, so this may be too strict.
	 */
	if (count_glob_matches (name, info->ext, found)) {
		/* If the page exists and didn't beforehand, then presumably
		 * we're about to rescan, which will replace the WHATIS_MAN
		 * entry with something better. However, there have been
		 * bugs that created false WHATIS_MAN entries, so force the
		 * rescan just to be sure; since in the absence of a bug we
		 * would rescan anyway, this isn't a problem.
		 */
		if (debug && !force_rescan)
			fprintf (stderr,
				 "%s(%s): whatis replaced by real page; "
				 "forcing a rescan just in case\n",
				 name, info->ext);
		force_rescan = 1;
		return 0;
	} else if (STREQ (info->pointer, "-")) {
		/* This is broken; a WHATIS_MAN should never have an empty
		 * pointer field. This might have happened due to the first
		 * name in a page being different from what the file name
		 * says; that's fixed now, so delete and force a rescan.
		 */
		if (!opt_test)
			dbdelete (name, info);
		else if (debug)
			fprintf (stderr,
				 "%s(%s): whatis with empty pointer, "
				 "would delete\n",
				 name, info->ext);

		if (debug && !force_rescan)
			fprintf (stderr,
				 "%s(%s): whatis had empty pointer; "
				 "forcing a rescan just in case\n",
				 name, info->ext);
		force_rescan = 1;
		return 1;
	} else {
		/* Does the real page still exist? */
		char **real_found;
		int save_debug = debug;
		debug = 0;
		real_found = look_for_file (path, info->ext,
					    info->pointer, cat, 1);
		debug = save_debug;

		if (count_glob_matches (info->pointer, info->ext, real_found))
			return 0;

		if (!opt_test)
			dbdelete (name, info);
		else if (debug)
			fprintf (stderr,
				 "%s(%s): whatis target was deleted, "
				 "would delete\n",
				 name, info->ext);
		return 1;
	}
}

/* Check that multi keys are correctly constructed. */
static short check_multi_key (const char *name, const char *content)
{
	const char *walk, *next;

	if (!*content)
		return 0;

	for (walk = content; walk && *walk; walk = next) {
		/* The name in the multi key should only differ from the
		 * name of the key itself in its case, if at all.
		 */
		int valid = 1;
		++walk; /* skip over initial tab */
		next = strchr (walk, '\t');
		if (next) {
			if (strncasecmp (name, walk, next - walk))
				valid = 0;
		} else {
			if (strcasecmp (name, walk))
				valid = 0;
		}
		if (!valid) {
			if (debug)
				fprintf (stderr,
					 "%s: broken multi key \"%s\", "
					 "forcing a rescan\n",
					 name, content);
			force_rescan = 1;
			return 1;
		}

		/* If the name was valid, skip over the extension and
		 * continue the scan.
		 */
		walk = next;
		next = walk ? strchr (walk + 1, '\t') : NULL;
	}

	return 0;
}

/* Go through the database and purge references to man pages that no longer
 * exist.
 */
short purge_missing (const char *manpath, const char *catpath)
{
	datum key;
	short count = 0;

	if (!quiet)
		printf (_("Purging old database entries in %s...\n"), manpath);

	dbf = MYDBM_RWOPEN (database);
	if (!dbf) {
		gripe_rwopen_failed (database);
		return 0;
	}

	key = MYDBM_FIRSTKEY (dbf);

	while (key.dptr != NULL) {
		datum content, nextkey;
		struct mandata entry;
		char *nicekey, *tab;
		int save_debug;
		char **found;

		/* Ignore db identifier keys. */
		if (*key.dptr == '$') {
			nextkey = MYDBM_NEXTKEY (dbf, key);
			MYDBM_FREE (key.dptr);
			key = nextkey;
			continue;
		}

		content = MYDBM_FETCH (dbf, key);
		if (!content.dptr)
			return count;

		/* Get just the name. */
		nicekey = xstrdup (key.dptr);
		tab = strchr (nicekey, '\t');
		if (tab)
			*tab = '\0';

		/* Deal with multi keys. */
		if (*content.dptr == '\t') {
			if (check_multi_key (nicekey, content.dptr))
				MYDBM_DELETE (dbf, key);
			free (nicekey);
			MYDBM_FREE (content.dptr);
			nextkey = MYDBM_NEXTKEY (dbf, key);
			MYDBM_FREE (key.dptr);
			key = nextkey;
			continue;
		}

		split_content (content.dptr, &entry);
		content.dptr = entry.addr;

		save_debug = debug;
		debug = 0;	/* look_for_file() is quite noisy */
		if (entry.id <= WHATIS_MAN)
			found = look_for_file (manpath, entry.ext,
					       entry.name ? entry.name
							  : nicekey,
					       0, 1);
		else
			found = look_for_file (catpath, entry.ext,
					       entry.name ? entry.name
							  : nicekey,
					       1, 1);
		debug = save_debug;

		/* Now actually decide whether to purge, depending on the
		 * type of entry.
		 */
		if (entry.id == ULT_MAN || entry.id == SO_MAN ||
		    entry.id == STRAY_CAT)
			count += purge_normal (nicekey, &entry, found);
		else if (entry.id == WHATIS_MAN)
			count += purge_whatis (manpath, 0, nicekey,
					       &entry, found);
		else	/* entry.id == WHATIS_CAT */
			count += purge_whatis (catpath, 1, nicekey,
					       &entry, found);

		free (nicekey);

		MYDBM_FREE (content.dptr);
		nextkey = MYDBM_NEXTKEY (dbf, key);
		MYDBM_FREE (key.dptr);
		key = nextkey;
	}

	MYDBM_REORG (dbf);
	MYDBM_CLOSE (dbf);
	return count;
}
