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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>	/* for chmod() */
#include <dirent.h>
#include <unistd.h>

#ifdef SECURE_MAN_UID
#  include <pwd.h>
#endif /* SECURE_MAN_UID */

#include "argp.h"
#include "dirname.h"
#include "xgetcwd.h"

#include "gettext.h"
#include <locale.h>
#define _(String) gettext (String)
#define N_(String) gettext_noop (String)

#include "manconfig.h"

#include "error.h"
#include "cleanup.h"
#include "pipeline.h"

#include "mydbm.h"

#include "check_mandirs.h"
#include "filenames.h"
#include "manp.h"
#include "security.h"

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
#ifdef SECURE_MAN_UID
struct passwd *man_owner;
#endif
static int purged = 0;
static int strays = 0;

static int check_for_strays = 1;
static int purge = 1;
static int user;
static int create;
static const char *arg_manp;

const char *argp_program_version = "mandb " PACKAGE_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;
error_t argp_err_exit_status = FAIL;

static const char args_doc[] = N_("[MANPATH]");

static struct argp_option options[] = {
	{ "debug",		'd',	0,		0,	N_("emit debugging messages") },
	{ "quiet",		'q',	0,		0,	N_("work quietly, except for 'bogus' warning") },
	{ "no-straycats",	's',	0,		0,	N_("don't look for or add stray cats to the dbs") },
	{ "no-purge",		'p',	0,		0,	N_("don't purge obsolete entries from the dbs") },
	{ "user-db",		'u',	0,		0,	N_("produce user databases only") },
	{ "create",		'c',	0,		0,	N_("create dbs from scratch, rather than updating") },
	{ "test",		't',	0,		0,	N_("check manual pages for correctness") },
	{ "filename",		'f',	N_("FILENAME"),	0,	N_("update just the entry for this filename") },
	{ "config-file",	'C',	N_("FILE"),	0,	N_("use this user configuration file") },
	{ 0, 'h', 0, OPTION_HIDDEN, 0 }, /* compatibility for --help */
	{ 0 }
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	static int quiet_temp = 0;

	switch (key) {
		case 'd':
			debug_level = 1;
			return 0;
		case 'q':
			++quiet_temp;
			return 0;
		case 's':
			check_for_strays = 0;
			return 0;
		case 'p':
			purge = 0;
			return 0;
		case 'u':
			user = 1;
			return 0;
		case 'c':
			create = 1;
			purge = 0;
			return 0;
		case 't':
			opt_test = 1;
			return 0;
		case 'f':
			single_filename = arg;
			create = 0;
			purge = 0;
			check_for_strays = 0;
			return 0;
		case 'C':
			user_config_file = arg;
			return 0;
		case 'h':
			argp_state_help (state, state->out_stream,
					 ARGP_HELP_STD_HELP);
			break;
		case ARGP_KEY_ARG:
			if (arg_manp)
				argp_usage (state);
			arg_manp = arg;
			return 0;
		case ARGP_KEY_SUCCESS:
			if (opt_test && !debug_level)
				quiet = 1;
			else if (quiet_temp == 1)
				quiet = 2;
			else
				quiet = quiet_temp;
			return 0;
	}
	return ARGP_ERR_UNKNOWN;
}

static struct argp argp = { options, parse_opt, args_doc };

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

static char *manpathlist[MAXDIRS];

extern int pages;

/* remove() with error checking */
static inline void xremove (const char *path)
{
	if (remove (path) == -1 && errno != ENOENT)
		error (0, errno, _("can't remove %s"), path);
}

/* rename() with error checking */
static inline void xrename (const char *from, const char *to)
{
	if (rename (from, to) == -1 && errno != ENOENT) {
		error (0, errno, _("can't rename %s to %s"), from, to);
		xremove (from);
	}
}

/* chmod() with error checking */
static inline void xchmod (const char *path, mode_t mode)
{
	if (chmod (path, mode) == -1) {
		error (0, errno, _("can't chmod %s"), path);
		xremove (path);
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
	else
		xchmod (to, DBMODE);

	return ret;
}

/* rename and chmod the database */
static inline void finish_up (void)
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
static inline void xchown (const char *path, uid_t owner, uid_t group)
{
	if (chown (path, owner, group) == -1) {
		error (0, errno, _("can't chown %s"), path);
		xremove (path);
	}
}

/* change the owner of global man databases */
static inline void do_chown (uid_t uid)
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
static inline short update_db_wrapper (const char *manpath)
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
static void cleanup_sigsafe (void *dummy)
{
	dummy = dummy; /* not used */

#ifdef NDBM
#  ifdef BERKELEY_DB
	if (tmpdbfile)
		unlink (tmpdbfile);
#  else /* !BERKELEY_DB NDBM */
	if (tmpdirfile)
		unlink (tmpdirfile);
	if (tmppagfile)
		unlink (tmppagfile);
#  endif /* BERKELEY_DB NDBM */
#else /* !NDBM */
	if (xtmpfile)
		unlink (xtmpfile);
#endif /* NDBM */
}

/* remove incomplete databases */
static void cleanup (void *dummy)
{
	dummy = dummy; /* not used */

#ifdef NDBM
#  ifdef BERKELEY_DB
	if (tmpdbfile) {
		free (tmpdbfile);
		tmpdbfile = NULL;
	}
#  else /* !BERKELEY_DB NDBM */
	if (tmpdirfile) {
		free (tmpdirfile);
		tmpdirfile = NULL;
	}
	if (tmppagfile) {
		free (tmppagfile);
		tmppagfile = NULL;
	}
#  endif /* BERKELEY_DB NDBM */
#else /* !NDBM */
	if (xtmpfile) {
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
	database = appendstr (NULL, catpath, "/", pid, NULL);
	
	if (!quiet) 
		printf (_("Processing manual pages under %s...\n"), manpath);
#ifdef NDBM
#  ifdef BERKELEY_DB
	dbfile = appendstr (NULL, dbname, ".db", NULL);
	free (dbname);
	tmpdbfile = appendstr (NULL, database, ".db", NULL);
	if (create || force_rescan || opt_test) {
		xremove (tmpdbfile);
		amount = create_db (manpath);
	} else {
		if (xcopy (dbfile, tmpdbfile) < 0)
			return 0;
		amount = update_db_wrapper (manpath);
	}
#  else /* !BERKELEY_DB NDBM */
	dirfile = appendstr (NULL, dbname, ".dir", NULL);
	pagfile = appendstr (NULL, dbname, ".pag", NULL);
	free (dbname);
	tmpdirfile = appendstr (NULL, database, ".dir", NULL);
	tmppagfile = appendstr (NULL, database, ".pag", NULL);
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

static short process_manpath (const char *manpath, int global_manpath)
{
	char *catpath;
	short amount = 0;

	if (global_manpath) { 	/* system db */
		catpath = get_catpath (manpath, SYSTEM_CAT);
		assert (catpath);
	} else {		/* user db */
		catpath = get_catpath (manpath, USER_CAT);
		if (!catpath)
			catpath = xstrdup (manpath);
	}

	force_rescan = 0;
	if (purge) {
		database = mkdbname (catpath);
		purged += purge_missing (manpath, catpath);
		free (database);
		database = NULL;
	}

	push_cleanup (cleanup, NULL, 0);
	push_cleanup (cleanup_sigsafe, NULL, 1);
	if (single_filename) {
		/* The file might be in a per-locale subdirectory that we
		 * aren't processing right now.
		 */
		char *manpath_prefix = appendstr (NULL, manpath, "/man", NULL);
		if (STRNEQ (manpath_prefix, single_filename,
		    strlen (manpath_prefix)))
			amount += mandb (catpath, manpath);
		free (manpath_prefix);
		/* otherwise try the next manpath */
	} else
		amount += mandb (catpath, manpath);

	if (!opt_test && amount) {
		finish_up ();
#ifdef SECURE_MAN_UID
		if (global_manpath && euid == 0)
			do_chown (man_owner->pw_uid);
#endif /* SECURE_MAN_UID */
	}
	cleanup_sigsafe (NULL);
	pop_cleanup ();
	cleanup (NULL);
	pop_cleanup ();
	free (database);
	database = NULL;

	if (check_for_strays && amount) {
		database = mkdbname (catpath);
		strays += straycats (manpath);
		free (database);
		database = NULL;
	}

	free (catpath);

	return amount;
}

int main (int argc, char *argv[])
{
	char *sys_manp;
	short amount = 0;
	char **mp;

#ifdef __profile__
	char *cwd;
#endif /* __profile__ */

	program_name = base_name (argv[0]);

	/* initialise the locale */
	if (!setlocale (LC_ALL, ""))
		/* Obviously can't translate this. */
		error (0, 0, "can't set the locale; make sure $LC_* and $LANG "
			     "are correct");
	bindtextdomain (PACKAGE, LOCALEDIR);
	bindtextdomain (PACKAGE "-gnulib", LOCALEDIR);
	textdomain (PACKAGE);

	if (argp_parse (&argp, argc, argv, 0, 0, 0))
		exit (FAIL);

#ifdef __profile__
	cwd = xgetcwd ();
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
	manp = get_manpath (NULL);	/* also calls read_config_file() */

	/* pick up the system manpath or use the supplied one */
	if (arg_manp) {
		free (manp);
		manp = xstrdup (arg_manp);
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

	debug ("manpath=%s\n", manp);

	/* get the manpath as an array of pointers */
	create_pathlist (manp, manpathlist); 

	/* finished manpath processing, regain privs */
	regain_effective_privs ();

	for (mp = manpathlist; *mp; mp++) {
		int global_manpath = is_global_mandir (*mp);
		DIR *dir;
		struct dirent *subdirent;
		struct stat st;

		if (global_manpath) {	/* system db */
			if (user)
				continue;
		} else {		/* user db */
			drop_effective_privs ();
		}

		amount += process_manpath (*mp, global_manpath);

		dir = opendir (*mp);
		if (!dir) {
			error (0, errno, _("can't search directory %s"), *mp);
			goto next_manpath;
		}

		while ((subdirent = readdir (dir)) != NULL) {
			char *subdirpath;

			/* Look for per-locale subdirectories. */
			if (STREQ (subdirent->d_name, ".") ||
			    STREQ (subdirent->d_name, ".."))
				continue;
			if (STRNEQ (subdirent->d_name, "man", 3))
				continue;

			subdirpath = appendstr (NULL, *mp, "/",
						subdirent->d_name, NULL);
			if (stat (subdirpath, &st) == 0 &&
			    S_ISDIR (st.st_mode))
				amount += process_manpath (subdirpath,
							   global_manpath);
			free (subdirpath);
		}

		closedir (dir);

next_manpath:
		if (!global_manpath)
			regain_effective_privs ();

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
			fprintf (stderr, _("No databases created."));
		exit (FAIL);
	}
	free (program_name);
	exit (OK);
}
