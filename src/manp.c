/*
 * manp.c: Manpath calculations
 *
 * Copyright (C) 1990, 1991 John W. Eaton.
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
 * John W. Eaton
 * jwe@che.utexas.edu
 * Department of Chemical Engineering
 * The University of Texas at Austin
 * Austin, Texas  78712
 *
 * Wed May  4 15:44:47 BST 1994 Wilf. (G.Wilford@ee.surrey.ac.uk): changes
 * to get_dirlist() and manpath().
 *
 * This whole code segment is unfriendly and could do with a complete 
 * overhaul.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>

#if defined(STDC_HEADERS)
#  include <stdlib.h>
#  include <string.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#else /* no string(s) header */
extern char *strtok();
extern char *strchr();
extern char *strstr();
#endif

#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifndef STDC_HEADERS
extern char *getenv();
extern int errno;
#endif

#include <libintl.h>
#define _(String) gettext (String)

#include "manconfig.h"
#include "lib/error.h"
#include "lib/getcwdalloc.h"
#include "security.h"
#include "manp.h"

struct list {
	char *key;
	char *cont;
	int flag;
	struct list *next;
};

static struct list *namestore, *tailstore;

#define SECTION		-5
#define DEFINE_USER	-4
#define DEFINE		-3
#define MANDB_MAP_USER	-2
#define MANDB_MAP	-1
#define MANPATH_MAP	 0
#define MANDATORY	 1

/* DIRLIST list[MAXDIRS]; */
char *tmplist[MAXDIRS];
char *manpathlist[MAXDIRS];

char *user_config_file = NULL;

static void mkcatdirs (const char *mandir, const char *catdir);
static __inline__ char *get_manpath (char *path);
static __inline__ char *has_mandir (const char *p);
static __inline__ char *fsstnd (const char *path);
static char *def_path (int flag);
static void add_dir_to_list (char **lp, const char *dir);
static char **add_dir_to_path_list (char **mphead, char **mp, const char *p);


static void add_to_list (const char *key, const char *cont, int flag)
{
	struct list *list = (struct list *) malloc (sizeof (struct list));
	list->key = xstrdup (key);
	list->cont = xstrdup (cont);
	list->flag = flag;
	list->next = NULL;
	if (tailstore)
		tailstore->next = list;
	tailstore = list;
	if (!namestore)
		namestore = list;
}

static const char *get_from_list (const char *key, int flag)
{
	struct list *list;

	for (list = namestore; list; list = list->next)
		if (flag == list->flag && STREQ (key, list->key))
			return list->cont;

	return NULL;
}

static struct list *iterate_over_list (struct list *prev, char *key, int flag)
{
	struct list *list;

	for (list = prev ? prev->next : namestore; list; list = list->next)
		if (flag == list->flag && STREQ (key, list->key))
			return list;

	return NULL;
}

/* Must not return DEFINEs set in ~/.manpath. This is used to fetch
 * definitions used in raised-privilege code; if in doubt, be conservative!
 */
const char *get_def (const char *thing, const char *def)
{
	const char *config_def = get_from_list (thing, DEFINE);
	return config_def ? config_def : def;
}

const char *get_def_user (const char *thing, const char *def)
{
	const char *config_def = get_from_list (thing, DEFINE_USER);
	if (!config_def)
		config_def = get_from_list (thing, DEFINE);
	return config_def ? config_def : def;
}

static void print_list (void)
{
	struct list *list;

	for (list = namestore; list; list = list->next)
		fprintf (stderr, "`%s'\t`%s'\t`%d'\n", list->key, 
			 list->cont, list->flag);
}

static void add_sections (char *sections)
{
	/* No need to free section_list; it's tokenized and each element is
	 * put into a linked list, which is kept around for later.
	 */
	char *section_list = xstrdup (sections);
	char *sect;

	for (sect = strtok (section_list, " "); sect;
	     sect = strtok (NULL, " ")) {
		add_to_list (sect, "", SECTION);
		if (debug)
			fprintf (stderr, "Added section `%s'.\n", sect);
	}
}

const char **get_sections (void)
{
	struct list *list;
	int length = 0;
	const char **sections, **sectionp;

	for (list = namestore; list; list = list->next)
		if (list->flag == SECTION)
			length++;
	sections = xmalloc ((length + 1) * sizeof *sections);
	sectionp = sections;
	for (list = namestore; list; list = list->next)
		if (list->flag == SECTION)
			*sectionp++ = list->key;
	*sectionp = NULL;
	return sections;
}

static void add_def (char *thing, char *config_def, int flag, int user)
{
	add_to_list (thing, flag == 2 ? config_def : "",
		     user ? DEFINE_USER : DEFINE);

	if (debug)
		fprintf (stderr, "Defined `%s' as `%s'.\n", thing, config_def);
}

static void add_manpath_map (const char *path, const char *mandir)
{
	if (!path || !mandir)
		return;

	add_to_list (path, mandir, MANPATH_MAP);

	if (debug)
		fprintf (stderr, "Path `%s' mapped to mandir `%s'.\n",
			 path, mandir);
}

static void add_mandb_map (const char *mandir, const char *catdir,
			   int flag, int user)
{
	char *tmpcatdir;

	assert (flag > 0);

	if (!mandir)
		return;

	if (flag == 1)
		tmpcatdir = xstrdup (mandir);
	else if (STREQ (catdir, "FSSTND"))
		tmpcatdir = fsstnd (mandir);
	else
		tmpcatdir = xstrdup (catdir);

	if (!tmpcatdir)
		return;

	add_to_list (mandir, tmpcatdir, user ? MANDB_MAP_USER : MANDB_MAP);

	if (debug)
		fprintf (stderr, "%s mandir `%s', catdir `%s'.\n",
			 user ? "User" : "Global", mandir, tmpcatdir);

	/* create the catman hierarchy if it doesn't exist */
	if (strcmp (program_name, "mandb") == 0)
		mkcatdirs (mandir, tmpcatdir);

	free (tmpcatdir);
}

static void add_mandatory (const char *mandir)
{
	if (!mandir)
		return;

	add_to_list (mandir, "", MANDATORY);

	if (debug)
		fprintf (stderr, "Mandatory mandir `%s'.\n", mandir);
}

/* accept (NULL or oldpath) and new path component. return new path */
static char *pathappend (char *oldpath, const char *appendage)
{
	assert ((!oldpath || *oldpath) && appendage);
	/* Remove duplicates */
	if (oldpath) {
		char *oldpathtok = xstrdup (oldpath), *tok;
		char *app_dedup = xstrdup (appendage);
		char *oldpathtok_ptr = oldpathtok;
		for (tok = strsep (&oldpathtok_ptr, ":"); tok;
		     tok = strsep (&oldpathtok_ptr, ":")) {
			char *search;
			if (!*tok)	    /* ignore empty fields */
				continue;
			search = strstr (app_dedup, tok);
			while (search) {
				char *terminator = search + strlen (tok);
				if (!*terminator) {
					/* End of the string, so chop here. */
					*search = 0;
					while (search > app_dedup &&
					       *--search == ':')
						*search = 0;
					break;
				} else if (*terminator == ':') {
					char *newapp;
					*search = 0;
					newapp = strappend (NULL, app_dedup,
							    terminator + 1,
							    NULL);
					free (app_dedup);
					app_dedup = newapp;
				}
				search = strstr (terminator, tok);
			}
		}
		free (oldpathtok);
		if (debug && !STREQ (appendage, app_dedup))
			fprintf (stderr, "%s:%s reduced to %s%s%s\n",
				 oldpath, appendage,
				 oldpath, *app_dedup ? ":" : "", app_dedup);
		if (*app_dedup)
			oldpath = strappend (oldpath, ":", app_dedup, NULL);
		free (app_dedup);
		return oldpath;
	} else
		return xstrdup (appendage);
}

static __inline__ void gripe_reading_mp_config (const char *file)
{
	error (FAIL, 0,
	       _("can't make sense of the manpath configuration file %s"),
	       file);
}

static __inline__ void gripe_stat_file (const char *file)
{
	if (debug)
		error (0, errno, _("warning: %s"), file);
}

static __inline__ void gripe_not_directory (const char *dir)
{
	if (!quiet)
		error (0, 0, _("warning: %s isn't a directory"), dir);
}

static void gripe_overlong_list (void)
{
	error (FAIL, 0, _("manpath list too long"));
}

/* accept a manpath list, separated with ':', return the associated 
   catpath list */
char *cat_manpath (char *manp)
{
	char *catp = NULL;
	const char *path, *catdir;

	for (path = strsep (&manp, ":"); path; path = strsep (&manp, ":")) {
		catdir = get_from_list (path, MANDB_MAP_USER);
		if (!catdir)
			catdir = get_from_list (path, MANDB_MAP);
		catp = catdir ? pathappend(catp, catdir) 
			      : pathappend(catp, path);
	}

	return catp;
}		

static char *
check_and_give (const char *path, const char *locale)
{
	char *result = NULL, *testdir;
	int test;

	testdir = strappend (NULL, path, "/", locale, NULL);
	test = is_directory (testdir);

	if (test == 1) {
		if (debug)
			fprintf (stderr, "check_and_give(): adding %s\n",
				 testdir);

		result = xstrdup (testdir);	/* I do not like side effects */
	} else if (test == 0) 
		gripe_not_directory (testdir);
	
	free (testdir);

	return result;
}

static char *
add_to_manpath (char *manpath, const char *path)
{
	return pathappend (manpath, path);
}


char *add_nls_manpath (char *manpathlist, const char *locale)
{
#ifdef HAVE_SETLOCALE
	char *manpath = NULL;
	char *path;
	char *temp_locale;
	char *omanpathlist = xstrdup (manpathlist);
	char *manpathlist_ptr = manpathlist;

	if (debug)
		fprintf (stderr, "add_nls_manpath(): processing %s\n",
			 manpathlist);

	if (locale == NULL || *locale == '\0' || *locale == 'C')
		return manpathlist;

	temp_locale = xstrdup (locale);

	for (path = strsep (&manpathlist_ptr, ":"); path;
	     path = strsep (&manpathlist_ptr, ":") ) {

		static char locale_delims[] = "@,._";
		char *delim, *tempo;
		char *testpath;

		strcpy (temp_locale, locale);

		testpath = check_and_give (path, temp_locale);
		if (testpath) {
			manpath = add_to_manpath (manpath, testpath);
			free (testpath);
		}
		for (delim = locale_delims; *delim != '\0'; ++delim) {
			tempo = strchr (temp_locale, *delim);
			if (tempo) {
				/* Strip out the rest of the line */
				*tempo = '\0';
				testpath = check_and_give (path, temp_locale);
				if (testpath) {
					manpath = add_to_manpath (manpath,
								  testpath);
					free (testpath);
				}
			}
		}
	}
	/* After doing all the locale stuff we add the manpath to the *END*
	 * so the locale dirs are checked first on each section */
	manpath = add_to_manpath (manpath, omanpathlist);
	free (omanpathlist);

	free (manpathlist);
	return manpath;

#else /* !HAVE_SETLOCALE */
	return manpathlist;
#endif /* HAVE_SETLOCALE */
}

static char *add_system_manpath (const char *systems, const char *manpathlist)
{
	char *system;
	char *manpath = NULL;
	char *tmpsystems;

	if (!systems)
		systems = getenv ("SYSTEM");

	if (!systems || !*systems)
		return xstrdup (manpathlist);

	/* Avoid breaking the environment. */
	tmpsystems = xstrdup (systems);

	/* For each systems component */

	for (system = strtok (tmpsystems, ",:"); system;
	     system = strtok (NULL, ",:")) {

		/* For each manpathlist component */

		if (!STREQ (system, "man")) {
			const char *next, *path;
			char *newdir = NULL;
			for (path = manpathlist; path; path = next) {
				int status;
				char *element;

				next = strchr (path, ':');
				if (next) {
					element = xstrndup (path, next - path);
					++next;
				} else
					element = xstrdup (path);
				newdir = strappend (newdir, element, "/",
						    system, NULL);
				free (element);

				status = is_directory (newdir);

				if (status == 0)
					gripe_not_directory (newdir);
				else if (status == 1) {
					if (debug)
						fprintf (stderr,
							 "adding %s to "
							 "manpathlist\n",
							 newdir);
					manpath = pathappend (manpath, newdir);
				} else if (debug) {
					fputs ("can't stat ", stderr);
					perror (newdir);
				}
				/* reset newdir */
				*newdir = '\0';
			}
			if (newdir)
				free (newdir);
		} else
			manpath = pathappend (manpath, manpathlist);
	}
	free (tmpsystems);

	/*
	 * Thu, 21 Nov 1996 22:24:19 +0200 fpolacco@debian.org
	 * bug#5534 (man fails if env var SYSTEM is defined)
	 * with error [man: internal manpath equates to NULL]
	 * the reason: is_directory (newdir); returns -1
	 */
	if (!manpath) {
		if (debug)
			fprintf (stderr, "add_system_manpath(): %s\n",
				 "internal manpath equates to NULL");
		return xstrdup (manpathlist);
	}
	return manpath;
}

/*
 * Always add system and locale directories to pathlist.
 * If the environment variable MANPATH is set, return it.
 * If the environment variable PATH is set and has a nonzero length,
 * try to determine the corresponding manpath, otherwise, return the
 * default manpath.
 *
 * The man_db.config file is used to map system wide /bin directories
 * to top level man page directories.
 *
 * For directories which are in the user's path but not in the
 * man_db.config file, see if there is a subdirectory `man' or `MAN'.
 * If so, add that directory to the path.  Example:  user has
 * $HOME/bin in his path and the directory $HOME/bin/man exists -- the
 * directory $HOME/bin/man will be added to the manpath.
 */
static char *guess_manpath (const char *systems)
{
	char *path = getenv ("PATH");

	if (path == NULL) {
		/* Things aren't going to work well, but hey... */
		if (!quiet)
			error (0, 0, _("warning: $PATH not set"));

		return add_system_manpath (systems, def_path (MANDATORY));
	} else {
		if (strlen (path) == 0) {
			/* Things aren't going to work well here either... */
			if (!quiet)
				error (0, 0, _("warning: empty $PATH"));
			
			return add_system_manpath (systems,
						   def_path (MANDATORY));
		}

		return add_system_manpath (systems, get_manpath (path));
	}
}

char *manpath (const char *systems)
{
	char *manpathlist;

	/* need to read config file even if MANPATH set, for mandb(8) */
	if (!namestore)
		read_config_file ();

	manpathlist = getenv ("MANPATH");
	if (manpathlist && *manpathlist) {
		char *pos;
		/* This must be it. */
		if (manpathlist[0] == ':') {
			if (!quiet)
				error (0, 0,
				       _("warning: $MANPATH set, "
					 "prepending %s"),
				       CONFIG_FILE);
			manpathlist = strappend (NULL,
						 guess_manpath (systems),
						 add_system_manpath
							(systems, manpathlist),
						 NULL);
		} else if (manpathlist[strlen (manpathlist) - 1] == ':') {
			if (!quiet)
				error (0, 0,
				       _("warning: $MANPATH set, "
					 "appending %s"),
				       CONFIG_FILE);
			manpathlist = strappend (NULL,
						 add_system_manpath
							(systems, manpathlist),
						 guess_manpath (systems),
						 NULL);
		} else if ((pos = strstr (manpathlist,"::"))) {
			*(pos++) = '\0';
			if (!quiet)
				error (0, 0,
				       _("warning: $MANPATH set, "
					 "inserting %s"),
				       CONFIG_FILE);
			manpathlist = strappend (NULL,
						 add_system_manpath
							(systems, manpathlist),
						 ":", guess_manpath (systems),
						 add_system_manpath
							(systems, pos),
						 NULL);
		} else {
			if (!quiet)
				error (0, 0,
				       _("warning: $MANPATH set, ignoring %s"),
				       CONFIG_FILE);
			manpathlist = add_system_manpath (systems,
							  manpathlist);
		}
	} else
		manpathlist = guess_manpath (systems);

	return manpathlist;
}

#ifdef SECURE_MAN_UID
extern uid_t ruid;			/* initial real user id */
extern uid_t euid;			/* initial effective user id */
#endif /* SECURE_MAN_UID */

/* create the catman hierarchy if it doesn't exist */
static void
mkcatdirs (const char *mandir, const char *catdir)
{
	char *manname, *catname;
#ifdef SECURE_MAN_UID
	struct passwd *man_owner = get_man_owner ();
#endif

	if (catdir) {
		int oldmask = umask (022);
		/* first the base catdir */
		if (is_directory (catdir) != 1) {
			regain_effective_privs ();
			if (mkdir (catdir, S_ISGID | 0755) < 0) {
				if (!quiet)
					error (0, 0,
					       _("warning: cannot create catdir %s"),
					       catdir);
				if (debug)
					fprintf (stderr,
						 "warning: cannot create catdir %s\n",
						 catdir);
			} else if (debug)
				fprintf (stderr, "created base catdir %s\n",
					 catdir);
#ifdef SECURE_MAN_UID
			if (ruid == 0)
				chown (catdir, man_owner->pw_uid, 0);
#endif /* SECURE_MAN_UID */
			drop_effective_privs ();
		}
		/* then the hierarchy */
		catname = strappend (NULL, catdir, "/cat1", NULL);
		manname = strappend (NULL, mandir, "/man1", NULL);
		if (is_directory (catdir) == 1) {
			int j;
			regain_effective_privs ();
			if (debug)
				fprintf (stderr, 
					 "creating catdir hierarchy %s	",
					 catdir);
			for (j = 1; j <= 9; j++) {
				catname[strlen (catname) - 1] = '0' + j;
				manname[strlen (manname) - 1] = '0' + j;
				if ((is_directory (manname) == 1)
				 && (is_directory (catname) != 1)) {
					if (mkdir (catname,
						   S_ISGID | 0755) < 0) {
						if (!quiet)
							error (0, 0, _("warning: cannot create catdir %s"), catname);
						if (debug)
							fprintf (stderr, "warning: cannot create catdir %s\n", catname);
					} else if (debug)
						fprintf (stderr, " cat%d", j);
#ifdef SECURE_MAN_UID
					if (ruid == 0)
						chown (catname,
						       man_owner->pw_uid, 0);
#endif /* SECURE_MAN_UID */
				}
			}
			if (debug)
				fprintf (stderr, "\n");
			drop_effective_privs ();
		}
		free (catname);
		free (manname);
		umask (oldmask);
	}
}

extern char *program_name;
/* Parse the manpath.config file, extracting appropriate information. */
static void add_to_dirlist (FILE *config, int user)
{
	char *bp;
	char buf[BUFSIZ];
	char key[50], cont[512];
	int c;

	while ((bp = fgets (buf, BUFSIZ, config))) {

		while (CTYPE (isspace, *bp))
			bp++;

		/* TODO: would like a (limited) replacement for sscanf()
		 * here that allocates its own memory. At that point check
		 * everything that sprintf()s manpath et al!
		 */
		if (*bp == '#' || *bp == '\0')
			continue;
		else if (strncmp (bp, "NO", 2) == 0)
			continue;	/* match any word starting with NO */
		else if (sscanf (bp, "MANBIN %*s") == 1)
			continue;
		else if (sscanf (bp, "MANDATORY_MANPATH %49s", key) == 1)
			add_mandatory (key);	
		else if (sscanf (bp, "MANPATH_MAP %49s %511s", key, cont) == 2) 
			add_manpath_map (key, cont);
		else if ((c = sscanf (bp, "MANDB_MAP %49s %511s",
				      key, cont)) > 0) 
			add_mandb_map (key, cont, c, user);
		else if ((c = sscanf (bp, "DEFINE %49s %511[^\n]",
				      key, cont)) > 0)
			add_def (key, cont, c, user);
		else if (sscanf (bp, "SECTION %511[^\n]", cont) == 1)
			add_sections (cont);
		else if (sscanf (bp, "SECTIONS %511[^\n]", cont) == 1)
			/* Since I keep getting it wrong ... */
			add_sections (cont);
	 	else {
			error (0, 0, _("can't parse directory list `%s'"), bp);
			gripe_reading_mp_config (CONFIG_FILE);
		}
	}
}

void read_config_file(void)
{
	char *home;
	FILE *config;

	home = xstrdup (getenv ("HOME"));
	if (home) {
		char *dotmanpath;
		if (!user_config_file)
			dotmanpath = strappend (home, "/.manpath", NULL);
		else
			dotmanpath = xstrdup (user_config_file);
		config = fopen (dotmanpath, "r");
		if (config != NULL) {
			if (debug)
				fprintf (stderr,
					 "From the config file %s:\n\n",
					 dotmanpath);
			add_to_dirlist (config, 1);
			fclose (config);
		}
		free (dotmanpath);
	}

	config = fopen (CONFIG_FILE, "r");
	if (config == NULL)
		error (FAIL, 0,
		       _("can't open the manpath configuration file %s"),
		       CONFIG_FILE);

	if (debug)
		fprintf (stderr, "From the config file %s:\n\n", CONFIG_FILE);

	add_to_dirlist (config, 0);
	fclose (config);

	if (debug)
		print_list ();
}


/*
 * Construct the default manpath.  This picks up mandatory manpaths
 * only.
 */
static char *def_path (int flag)
{
	char *manpath = NULL;
	struct list *list; 

	for (list = namestore; list; list = list->next)
		if (list->flag == flag) {
	 		int status = is_directory (list->key);

			if (status < 0)
				gripe_stat_file (list->key);
			else if (status == 0 && !quiet)
				error (0, 0,
				       _("warning: mandatory directory %s "
					 "doesn't exist"), list->key);
			else if (status == 1)
				manpath = pathappend (manpath, list->key);
	}

	/* If we have complete config file failure... */
	if (!manpath)
		return xstrdup ("/usr/man");

	return manpath;
}

/*
 * For each directory in the user's path, see if it is one of the
 * directories listed in the man_db.config file.  If so, and it is
 * not already in the manpath, add it.  If the directory is not listed
 * in the man_db.config file, see if there is a subdirectory `../man' or
 * `man'.  If so, and it is not already in the manpath, add it.
 * Example:  user has $HOME/bin in his path and the directory
 * $HOME/man exists -- the directory $HOME/man will be added
 * to the manpath.
 */
static __inline__ char *get_manpath (char *path)
{
	int len;
	char *tmppath;
	char *t;
	char *p;
	char **lp;
	char *end;
	char *manpathlist;
	struct list *list;

	tmppath = xstrdup (path);

	for (end = p = tmppath; end; p = end + 1) {
		struct list *mandir_list;

		end = strchr (p, ':');
		if (end)
			*end = '\0';

		/* don't do this for current dir ("." or empty entry in PATH) */
		if (*p == '\0' || strcmp (p, ".") == 0)
			continue;

		if (debug)
			fprintf (stderr, "\npath directory %s ", p);

		mandir_list = iterate_over_list (NULL, p, MANPATH_MAP);

		/*
      		 * The directory we're working on is in the config file.
      		 * If we haven't added it to the list yet, do.
      		 */

		if (mandir_list) {
			if (debug)
				fputs("is in the config file\n", stderr);
			while (mandir_list) {
				add_dir_to_list (tmplist, mandir_list->cont);
				mandir_list = iterate_over_list
					(mandir_list, p, MANPATH_MAP);
			}

      		 /* The directory we're working on isn't in the config file.  
      		    See if it has ../man or man subdirectories.  
      		    If so, and it hasn't been added to the list, do. */

		} else {
			if (debug)
				fputs ("is not in the config file\n", stderr);

		 	t = has_mandir (p);
		 	if (t) {
				if (debug)
					fprintf (stderr, "but does have a ../man or man subdirectory\n");
	
				add_dir_to_list (tmplist, t);
				free (t);
		 	} else
				if (debug)
					fprintf (stderr, "and doesn't have ../man or man subdirectories\n");
		}
	}

	free (tmppath);

	if (debug)
		fprintf (stderr, "\nadding mandatory man directories\n\n");

	for (list = namestore; list; list = list->next)
		if (list->flag == MANDATORY) 
			add_dir_to_list (tmplist, list->key);

	len = 0;
	lp = tmplist;
	while (*lp != NULL) {
		len += strlen (*lp) + 1;
		lp++;
	}

	assert (len);
	manpathlist = (char *) xmalloc (len);
	*manpathlist = '\0';

	lp = tmplist;
	p = manpathlist;
	while (*lp != NULL) {
		len = strlen (*lp);
		memcpy (p, *lp, len);
		p += len;
		*p++ = ':';
		lp++;
	}

	p[-1] = '\0';

	return manpathlist;
}

/*
 * Add a directory to the manpath list if it isn't already there.
 */
static void add_dir_to_list (char **lp, const char *dir)
{
	int status;
	int pos = 0;

	while (*lp != NULL) {
		if (pos > MAXDIRS - 1)
			gripe_overlong_list ();
		if (!strcmp (*lp, dir)) {
			if (debug)
				fprintf (stderr,
					 "%s is already in the manpath\n",
					 dir);
			return;
		}
		lp++;
		pos++;
	}

	/* Not found -- add it. */

	status = is_directory (dir);

	if (status < 0)
		gripe_stat_file (dir);
	else if (status == 0)
		gripe_not_directory (dir);
	else if (status == 1) {
		if (debug)
			fprintf (stderr, "adding %s to manpath\n", dir);

		*lp = xstrdup (dir);
	}
}

/* path does not exist in config file: check to see if path/../man or 
   path/man exist.  If so return it, if not return NULL. */
static __inline__ char *has_mandir (const char *path)
{
	char *newpath = NULL;

	/* don't assume anything about path, especially that it ends in 
	   "bin" or even has a '/' in it! */
	   
	char *subdir = strrchr (path, '/');
	if (subdir) {
		const int prefix_len = subdir + 1 - path;
		newpath = xmalloc (prefix_len + sizeof ("man") + 1);
		strncpy (newpath, path, prefix_len);
		strcpy (newpath + prefix_len, "man");

		if (is_directory (newpath) == 1)
			return newpath;
		else
			*newpath = '\0';
	}

	newpath = strappend (newpath, path, "/man", NULL);

	if (is_directory (newpath) == 1)
		return newpath;

	free (newpath);
	return NULL;
}

static char **add_dir_to_path_list (char **mphead, char **mp, const char *p)
{
	int status;
	char *cwd;

	if (mp - mphead > MAXDIRS - 1)
		gripe_overlong_list ();

	status = is_directory (p);

	if (status < 0)
		gripe_stat_file (p);
	else if (status == 0)
		gripe_not_directory (p);
	else {
		/* deal with relative paths */

		if (*p != '/') {
			cwd = getcwd_allocated ();
			if (!cwd)
				error (FATAL, errno,
				       _("can't determine current directory"));
			*mp = strappend (cwd, "/", p, NULL);
		} else 
			*mp = xstrdup (p);

		if (debug)
			fprintf (stderr, "adding %s to manpathlist\n", *mp);
		mp++;
	}
	return mp;
}

void create_pathlist (const char *manp, char **mp)
{
	const char *p, *end;
	char **mphead = mp;

	/* Expand the manpath into a list for easier handling. */

	for (p = manp;; p = end + 1) {
		end = strchr (p, ':');
		if (end)
			mp = add_dir_to_path_list (mphead, mp,
						   xstrndup (p, end - p));
		else {
			mp = add_dir_to_path_list (mphead, mp, p);
			break;
		}
	}
	*mp = NULL;
}

/* Routine to get list of named system and user manpaths (in reverse order). */
char *get_mandb_manpath (void)
{
	char *manpath = NULL;
	struct list *list;

	for (list = namestore; list; list = list->next)
		if (list->flag == MANDB_MAP || list->flag == MANDB_MAP_USER)
			manpath = pathappend (manpath, list->key);

	return manpath;
}

/* Take manpath or manfile path as the first argument, and the type of
 * catpaths we want as the other (system catpaths, user catpaths, or both).
 * Return catdir mapping or NULL if it isn't a global/user mandir (as
 * appropriate).
 *
 * This routine would seem to work correctly for nls subdirs and would 
 * specify the (correct) consistent catpath even if not defined in the 
 * config file.
 *
 * Do not return user catpaths when cattype == 0! This is used to decide
 * whether to drop privileges. When cattype != 0 it's OK to return global
 * catpaths.
 */
char *get_catpath (const char *name, int cattype)
{
	struct list *list;

	for (list = namestore; list; list = list->next)
		if (((cattype & SYSTEM_CAT) && list->flag == MANDB_MAP) ||
		    ((cattype & USER_CAT)   && list->flag == MANDB_MAP_USER)) {
			size_t manlen = strlen (list->key);
			if (STRNEQ (name, list->key, manlen)) {
				const char *suffix = name + manlen;
				char *catpath = xstrdup (list->cont);

				if (*suffix == '/') {
					++suffix;
					catpath = strappend (catpath, "/",
							     NULL);
				}
				if (STRNEQ (suffix, "man", 3)) {
					suffix += 3;
					catpath = strappend (catpath, "cat",
							     NULL);
				}
				catpath = strappend (catpath, suffix, NULL);
			  	return catpath;
			}
		}

	return NULL;
}

/* Check to see if the supplied man directory is a system-wide mandir.
 * Obviously, user directories must not be included here.
 */
int is_global_mandir (const char *dir)
{
	struct list *list;

	for (list = namestore; list; list = list->next)
		if (list->flag == MANDB_MAP &&
		    STRNEQ (dir, list->key, strlen (list->key)))
		    	return 1;
	return 0;
}

/* Accept a manpath (not a full pathname to a file) and return an FSSTND 
   equivalent catpath */
static __inline__ char *fsstnd (const char *path)
{
	char *manpath;
	char *catpath;
	char *element;
	
	if (strncmp (path, MAN_ROOT, sizeof MAN_ROOT - 1) != 0) {
		if (!quiet)
			error (0, 0, _("warning: %s does not begin with %s"),
			       path, MAN_ROOT);
		return xstrdup (path);
	}
	/* get rid of initial "/usr" */
	path += sizeof MAN_ROOT - 1;
	manpath = xstrdup (path);
	catpath = (char *) xmalloc (strlen (path) + sizeof CAT_ROOT - 3);

	/* start with CAT_ROOT */ 
	(void) strcpy (catpath, CAT_ROOT);

	/* split up path into elements and deal with accordingly */
	for (element = strtok (manpath, "/"); element;
	     element = strtok (NULL, "/")) {
		if (strncmp (element, "man", 3) == 0) {
			if (*(element + 3)) { 
				*element = 'c';
				*(element + 2) = 't';
			} else
				continue;
		} 
		(void) strcat (catpath, "/");
		(void) strcat (catpath, element);
	}
	free (manpath);
	return catpath;
}
