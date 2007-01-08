/*
 * mandb.c: used to create and initialise global man database.
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
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Tue Apr 26 12:56:44 BST 1994  Wilf. (G.Wilford@ee.surrey.ac.uk) 
 *
 * CJW: Security fixes. Make --test work. Purge old database entries.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>	/* for chmod() */

#if defined(STDC_HEADERS)
#  include <string.h>
#  include <stdlib.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#else /* no string(s) header */
#endif /* STDC_HEADERS */

#ifndef STDC_HEADERS
extern int errno;
#endif

#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#else
#  define W_OK	2
#endif /* HAVE_UNISTD_H */

#ifdef SECURE_MAN_UID
#  include <pwd.h>
#endif /* SECURE_MAN_UID */

#ifdef HAVE_LIBGEN_H
#  include <libgen.h>
#endif /* HAVE_LIBGEN_H */

#ifdef HAVE_GETOPT_H
#  include <getopt.h>
#else /* !HAVE_GETOPT_H */
#  include "lib/getopt.h"
#endif /* HAVE_GETOPT_H */

#include "lib/gettext.h"
#include <locale.h>
#define _(String) gettext (String)

#include "manconfig.h"
#include "libdb/mydbm.h"
#include "lib/error.h"
#include "lib/cleanup.h"
#include "lib/pipeline.h"
#include "lib/getcwdalloc.h"
#include "check_mandirs.h"
#include "filenames.h"
#include "manp.h"
#include "security.h"

int debug = 0;
char *program_name;
int quiet = 1;
extern int opt_test;		/* don't update db */
MYDBM_FILE dbf;
char *manp;
char *database = NULL;
extern char *extension;		/* for globbing.c */
extern int force_rescan;	/* for check_mandirs.c */
static char *single_filename = NULL;
extern char *user_config_file;	/* for manp.c */

/* default options */
static const struct option long_options[] =
{
    {"create", 		no_argument,		0, 'c'},
    {"debug",		no_argument,		0, 'd'},
    {"filename",	required_argument,	0, 'f'},
    {"help",		no_argument,		0, 'h'},
    {"no-purge",	no_argument,		0, 'p'},
    {"quiet",		no_argument,		0, 'q'},
    {"user-db",		no_argument,		0, 'u'},
    {"no-straycats",    no_argument,		0, 's'},
    {"test",		no_argument,		0, 't'},
    {"config-file",	required_argument,	0, 'C'},
    {"version",		no_argument,		0, 'V'},
    {0, 0, 0, 0}
};

static const char args[] = "cdf:hpqstuC:V";
static int check_for_strays = 1;
static int purge = 1;
static int user;
static int create;

#ifdef NDBM
#  ifdef BERKELEY_DB
static char *dbfile;
static char *tmpdbfile;
#  else /* !BERKELEY_DB NDBM */
static char *dirfile;
static char *pagfile;
static char *tmpdirfile;
static char *tmppagfile;
#  endif /* BERKELEY_DB */
#else /* !NDBM */
static char *xfile;
static const char *xtmpfile;
#endif /* NDBM */

#ifdef SECURE_MAN_UID
extern uid_t ruid;
extern uid_t euid;
#endif /* SECURE_MAN_UID */

extern char *optarg;
extern int optind, opterr, optopt;
extern char *manpathlist[];
extern int pages;

static void usage (int status)
{
	printf (_("usage: %s [-dqspuct|-h|-V] [-C file] [-f filename] [manpath]\n"),
		program_name);
	printf (_(
		"-d, --debug                 produce debugging info.\n"
		"-q, --quiet                 work quietly, except for 'bogus' warning.\n"
		"-s, --no-straycats          don't look for or add stray cats to the dbs.\n"
		"-p, --no-purge              don't purge obsolete entries from the dbs.\n"
		"-u, --user-db               produce user databases only.\n"
		"-c, --create                create dbs from scratch, rather than updating.\n"
		"-t, --test                  check manual pages for correctness.\n"
		"-f, --filename              update just the entry for this filename.\n"
		"-C, --config-file           use this user configuration file.\n"
		"-V, --version               show version.\n"
		"-h, --help                  show this usage message.\n")
	);

	exit (status);
}

/* remove() with error checking */
static __inline__ void xremove (const char *path)
{
	if (remove (path) == -1 && errno != ENOENT)
		error (0, errno, _("can't remove %s"), path);
}

/* rename() with error checking */
static __inline__ void xrename (const char *from, const char *to)
{
	if (rename (from, to) == -1 && errno != ENOENT) {
		error (0, errno, _("can't rename %s to %s"), from, to);
		xremove (from);
	}
}

/* CPhipps 2000/02/24 - Copy a file. */
static int xcopy (const char *from, const char *to)
{
	FILE *ifp, *ofp;
	int ret = 0;

	ifp = fopen (from, "r");
	if (!ifp) {
		ret = -errno;
		if (errno == ENOENT)
			return 0;
		perror ("fopen");
		return ret;
	}

	ofp = fopen (to, "w");
	if (!ofp) {
		ret = -errno;
		perror ("fopen");
		fclose (ifp);
		return ret;
	}

	while (!feof (ifp) && !ferror (ifp)) {
		char buf[1024];
		size_t in = fread (buf, 1, sizeof (buf), ifp);
		if (in > 0) {
			if (fwrite (buf, 1, in, ofp) == 0 && ferror (ofp)) {
				ret = -errno;
				error (0, errno, _("can't write to %s"), to);
				break;
			}
		} else if (ferror (ifp)) {
			ret = -errno;
			error (0, errno, _("can't read from %s"), from);
			break;
		}
	}

	fclose (ifp);
	fclose (ofp);

	if (ret < 0)
		xremove (to);

	return ret;
}

/* chmod() with error checking */
static __inline__ void xchmod (const char *path, mode_t mode)
{
	if (chmod (path, mode) == -1) {
		error (0, errno, _("can't chmod %s"), path);
		xremove (path);
	}
}

/* rename and chmod the database */
static __inline__ void finish_up (void)
{
#ifdef NDBM
#  ifdef BERKELEY_DB
	xrename (tmpdbfile, dbfile);
	xchmod (dbfile, DBMODE);
	free (tmpdbfile);
	tmpdbfile = NULL;
#  else /* not BERKELEY_DB */
	xrename (tmpdirfile, dirfile);
	xchmod (dirfile, DBMODE);
	xrename (tmppagfile, pagfile);
	xchmod (pagfile, DBMODE);
	free (tmpdirfile);
	free (tmppagfile);
	tmpdirfile = tmppagfile = NULL;
#  endif /* BERKELEY_DB */
#else /* not NDBM */
	xrename (xtmpfile, xfile);
	xchmod (xfile, DBMODE);
	/* xtmpfile == database, so freed elsewhere */
	xtmpfile = NULL;
#endif /* NDBM */
}

#ifdef SECURE_MAN_UID
/* chown() with error checking */
static __inline__ void xchown (const char *path, uid_t owner, uid_t group)
{
	if (chown (path, owner, group) == -1) {
		error (0, errno, _("can't chown %s"), path);
		xremove (path);
	}
}

/* change the owner of global man databases */
static __inline__ void do_chown (uid_t uid)
{
#  ifdef NDBM
#    ifdef BERKELEY_DB
	xchown (dbfile, uid, -1);
#    else /* not BERKELEY_DB */
	xchown (dirfile, uid, -1);
	xchown (pagfile, uid, -1);
#    endif /* BERKELEY_DB */
#  else /* not NDBM */
	xchown (xfile, uid, -1);
#  endif /* NDBM */
}
#endif /* SECURE_MAN_UID */

/* Update a single file in an existing database. */
static short update_one_file (const char *manpath, const char *filename)
{
	dbf = MYDBM_RWOPEN (database);
	if (dbf) {
		struct mandata info;
		char *manpage;

		memset (&info, 0, sizeof (struct mandata));
		manpage = filename_info (filename, &info, "");
		if (info.name) {
			dbdelete (info.name, &info);
			purge_pointers (info.name);
			free (info.name);
		}
		free (manpage);

		test_manfile (filename, manpath);
	}
	MYDBM_CLOSE (dbf);

	return 1;
}

/* dont actually create any dbs, just do an update */
static __inline__ short update_db_wrapper (const char *manpath)
{
	short amount;

	if (single_filename)
		return update_one_file (manpath, single_filename);

	amount = update_db (manpath);
	if (amount != EOF)
		return amount;

	return create_db (manpath);
}

/* remove incomplete databases */
static void cleanup (void *dummy)
{
	dummy = dummy; /* not used */

#ifdef NDBM
#  ifdef BERKELEY_DB
	if (tmpdbfile) {
		unlink (tmpdbfile);
		free (tmpdbfile);
		tmpdbfile = NULL;
	}
#  else /* !BERKELEY_DB NDBM */
	if (tmpdirfile) {
		unlink (tmpdirfile);
		free (tmpdirfile);
		tmpdirfile = NULL;
	}
	if (tmppagfile) {
		unlink (tmppagfile);
		free (tmppagfile);
		tmppagfile = NULL;
	}
#  endif /* BERKELEY_DB NDBM */
#else /* !NDBM */
	if (xtmpfile) {
		unlink (xtmpfile);
		/* xtmpfile == database, so freed elsewhere */
		xtmpfile = NULL;
	}
	free (xfile);
	xfile = NULL;
#endif /* NDBM */
}

/* sort out the database names */
static short mandb (const char *catpath, const char *manpath)
{
	char pid[23];
	short amount;
	char *dbname;

	dbname = mkdbname (catpath);
	sprintf (pid, "%d", getpid ());
	database = strappend (NULL, catpath, "/", pid, NULL);
	
	if (!quiet) 
		printf (_("Processing manual pages under %s...\n"), manpath);
#ifdef NDBM
#  ifdef BERKELEY_DB
	dbfile = strappend (NULL, dbname, ".db", NULL);
	free (dbname);
	tmpdbfile = strappend (NULL, database, ".db", NULL);
	if (create || force_rescan || opt_test) {
		xremove (tmpdbfile);
		amount = create_db (manpath);
	} else {
		if (xcopy (dbfile, tmpdbfile) < 0)
			return 0;
		amount = update_db_wrapper (manpath);
	}
#  else /* !BERKELEY_DB NDBM */
	dirfile = strappend (NULL, dbname, ".dir", NULL);
	pagfile = strappend (NULL, dbname, ".pag", NULL);
	free (dbname);
	tmpdirfile = strappend (NULL, database, ".dir", NULL);
	tmppagfile = strappend (NULL, database, ".pag", NULL);
	if (create || force_rescan || opt_test) {
		xremove (tmpdirfile);
		xremove (tmppagfile);
		amount = create_db (manpath);
	} else {
		if (xcopy (dirfile, tmpdirfile) < 0)
			return 0;
		if (xcopy (pagfile, tmppagfile) < 0)
			return 0;
		amount = update_db_wrapper (manpath);
	}
#  endif /* BERKELEY_DB NDBM */
#else /* !NDBM */
	xfile = dbname; /* steal memory */
	xtmpfile = database;
	if (create || force_rescan || opt_test) {
		xremove (xtmpfile);
		amount = create_db (manpath);
	} else {
		if (xcopy (xfile, xtmpfile) < 0)
			return 0;
		amount = update_db_wrapper (manpath);
	}
#endif /* NDBM */

	return amount;
}

int main (int argc, char *argv[])
{
	int c;
	char *sys_manp;
	short amount = 0;
	int strays = 0;
	int purged = 0;
	int quiet_temp = 0;
	char **mp;

	int option_index; /* not used, but required by getopt_long() */

#ifdef __profile__
	char *cwd;
#endif /* __profile__ */

#ifdef SECURE_MAN_UID
	struct passwd *man_owner;
#endif

	program_name = xstrdup (basename (argv[0]));

	/* initialise the locale */
	if (!setlocale (LC_ALL, ""))
		/* Obviously can't translate this. */
		error (0, 0, "can't set the locale; make sure $LC_* and $LANG "
			     "are correct");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);

	while ((c = getopt_long (argc, argv, args,
				 long_options, &option_index)) != EOF) {
		switch (c) {
			case 'd':
				debug = 1;
				break;
			case 'q':
				quiet_temp++;
				break;
			case 'u':
				user = 1;
				break;
			case 'c':
				create = 1;
				purge = 0;
				break;
			case 'p':
				purge = 0;
				break;
			case 's':
				check_for_strays = 0;
				break;
			case 't':
				opt_test = 1;
				break;
			case 'f':
				single_filename = optarg;
				create = 0;
				purge = 0;
				check_for_strays = 0;
				break;
			case 'C':
				user_config_file = optarg;
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


#ifdef __profile__
	cwd = getcwd_allocated ();
	if (!cwd) {
		cwd = xmalloc (1);
		cwd[0] = '\0';
	}
#endif /* __profile__ */

	pipeline_install_sigchld ();

#ifdef SECURE_MAN_UID
	/* record who we are and drop effective privs for later use */
	init_security ();
#endif /* SECURE_MAN_UID */

#ifdef SECURE_MAN_UID
	man_owner = getpwnam (MAN_OWNER);
	if (man_owner == NULL)
		error (FAIL, 0,
		       _("the setuid man user \"%s\" does not exist"),
		       MAN_OWNER);
	if (!user && euid != 0 && euid != man_owner->pw_uid)
		user = 1;
#endif /* SECURE_MAN_UID */


	/* This is required for get_catpath(), regardless */
	manp = manpath (NULL);	/* also calls read_config_file() */

	if (opt_test && !debug)
		quiet = 1;
	else if (quiet_temp == 1)
		quiet = 2;
	else
		quiet = quiet_temp;

	/* pick up the system manpath or use the supplied one */
	if (argc != optind) {
		free (manp);
		manp = xstrdup (argv[optind]);
	} else if (!user) {
		sys_manp = get_mandb_manpath ();
		if (sys_manp) {
			free (manp);
			manp = sys_manp;
		} else
			error (0, 0,
			       _("warning: no MANDB_MAP directives in %s, "
				 "using your manpath"),
			       CONFIG_FILE);
	}

	if (debug)
		fprintf (stderr, "manpath=%s\n", manp);

	/* get the manpath as an array of pointers */
	create_pathlist (manp, manpathlist); 

	/* finished manpath processing, regain privs */
	regain_effective_privs ();

	for (mp = manpathlist; *mp; mp++) {
		int global_manpath = is_global_mandir (*mp);
		char *catpath;
		short amount_changed = 0;

		if (global_manpath) { 	/* system db */
		/*	if (access (catpath, W_OK) == 0 && !user) */
			if (user)
				continue;
			catpath = get_catpath (*mp, SYSTEM_CAT);
			assert (catpath);
		} else {		/* user db */
			catpath = get_catpath (*mp, USER_CAT);
			if (!catpath)
				catpath = *mp;
			drop_effective_privs ();
		}

		force_rescan = 0;
		if (purge) {
			database = mkdbname (catpath);
			purged += purge_missing (*mp, catpath);
			free (database);
			database = NULL;
		}

		push_cleanup (cleanup, NULL);
		if (single_filename) {
			if (STRNEQ (*mp, single_filename, strlen (*mp)))
				amount_changed += mandb (catpath, *mp);
			/* otherwise try the next manpath */
		} else
			amount_changed += mandb (catpath, *mp);

		amount += amount_changed;

		if (!opt_test && amount_changed) {
			finish_up ();
#ifdef SECURE_MAN_UID
			if (global_manpath && euid == 0)
				do_chown (man_owner->pw_uid);
#endif /* SECURE_MAN_UID */
		}
		cleanup (NULL);
		pop_cleanup ();
		free (database);
		database = NULL;

		if (check_for_strays && amount_changed) {
			database = mkdbname (catpath);
			strays += straycats (*mp);
			free (database);
			database = NULL;
		}

		if (!global_manpath)
			regain_effective_privs ();

		if (catpath != *mp)
			free (catpath);

		chkr_garbage_detector ();
	}

	if (!quiet) {
		printf (_(
		        "%d man subdirectories contained newer manual pages.\n"
		        "%d manual pages were added.\n"), 
		        amount, pages);
		if (check_for_strays)
			printf (_("%d stray cats were added.\n"), strays);
		if (purge)
			printf (_("%d old database entries were purged.\n"),
				purged);
	}

#ifdef __profile__
	/* For profiling */
	if (cwd[0])
		chdir (cwd);
#endif /* __profile__ */

	free_pathlist (manpathlist);
	free (manp);
	if (create && !amount) {
		if (!quiet)
			/* TODO: should be "No databases created." but we're
			 * string-frozen at the moment.
			 */
			fprintf (stderr, _("No databases updated."));
		exit (FAIL);
	}
	free (program_name);
	exit (OK);
}
