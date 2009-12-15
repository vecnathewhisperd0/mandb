/*
 * catman.c: create and/or update cat files
 *  
 * Copyright (C) 1994, 1995 Graeme W. Wilford. (Wilf.)
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
 * Thu Dec  8 00:03:12 GMT 1994  Wilf. (G.Wilford@ee.surrey.ac.uk) 
 */

/* MAX_ARGS must be >= 6, 4 for options, 1 for page and 1 for NULL */
#define MAX_ARGS	1024	/* *argv[MAX_ARG] */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif /* HAVE_SYS_WAIT_H */

#include <unistd.h>
#include <limits.h>  

#ifndef NAME_MAX
#  if defined(_POSIX_VERSION) && defined(_POSIX_NAME_MAX)
#    define NAME_MAX _POSIX_NAME_MAX
#  else /* !_POSIX_VERSION */
#    ifdef MAXNAMLEN
#      define NAME_MAX MAXNAMLEN
#    else /* !MAXNAMLEN */
#      define NAME_MAX 255 		/* default to max */
#    endif /* MAXNAMLEN */
#  endif /* _POSIX_VERSION */
#endif /* !NAME_MAX */

#ifndef ARG_MAX
#  if defined(_POSIX_VERSION) && defined(_POSIX_ARG_MAX)
#    define ARG_MAX _POSIX_ARG_MAX
#  else /* !_POSIX_VERSION */
#    define ARG_MAX 4096 		/* default to min */
#  endif /* _POSIX_VERSION */
#endif /* !ARG_MAX */

#include "argp.h"
#include "dirname.h"

#include "gettext.h"
#include <locale.h>
#define _(String) gettext (String)
#define N_(String) gettext_noop (String)

#include "manconfig.h"

#include "error.h"

#include "mydbm.h"
#include "db_storage.h"

#include "manp.h"

/* globals */
char *program_name;
int quiet = 1;
MYDBM_FILE dbf;
char *manp;
extern char *user_config_file;
char *database;
static const char **sections;

const char *argp_program_version = "catman " PACKAGE_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;
error_t argp_err_exit_status = FAIL;

static const char args_doc[] = N_("[SECTION...]");

static struct argp_option options[] = {
	{ "debug",		'd',	0,		0,	N_("emit debugging messages") },
	{ "manpath",		'M',	N_("PATH"),	0,	N_("set search path for manual pages to PATH") },
	{ "config-file",	'C',	N_("FILE"),	0,	N_("use this user configuration file") },
	{ 0, 'h', 0, OPTION_HIDDEN, 0 }, /* compatibility for --help */
	{ 0 }
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	char *mansect;

	switch (key) {
		case 'd':
			debug_level = 1;
			return 0;
		case 'M':
			manp = arg;
			return 0;
		case 'C':
			user_config_file = arg;
			return 0;
		case 'h':
			argp_state_help (state, state->out_stream,
					 ARGP_HELP_STD_HELP);
			break;
		case ARGP_KEY_ARGS:
			sections = xmalloc ((state->argc - state->next + 1) *
					    sizeof *sections);
			memcpy (sections, state->argv + state->next,
				(state->argc - state->next) *
				sizeof *sections);
			sections[state->argc - state->next] = NULL;
			return 0;
		case ARGP_KEY_NO_ARGS:
			mansect = getenv ("MANSECT");
			if (mansect && *mansect) {
				/* MANSECT contains sections */
				const char *sec;
				int i = 0;

				mansect = xstrdup (mansect);
				sections = NULL;
				for (sec = strtok (mansect, ":"); sec;
				     sec = strtok (NULL, ":")) {
					sections = xrealloc (sections,
							     (i + 2) *
							     sizeof *sections);
					sections[i++] = sec;
				}
				sections[i] = NULL;
				free (mansect);
			} else {
				/* use default sections */
				static const char *std_sections[] =
					STD_SECTIONS;
				sections = std_sections;
			}
			return 0;
	}
	return ARGP_ERR_UNKNOWN;
}

static struct argp argp = { options, parse_opt, args_doc };

static char *locale;

static char *manpathlist[MAXDIRS];

/* open db for reading, return 0 for success, errcode for failure */
static int rdopen_db (void)
{ 
	dbf = MYDBM_RDOPEN (database);
	if (dbf == NULL) {
		error (0, errno, _("cannot read database %s"), database);
		return 1;
	}
	return 0;
}

/* fork() and execve() man with the appropriate catman args. 
   If we inline this function, gcc v2.6.2 gives us `clobber' warnings ?? */
static void catman (char *args[])
{
	pid_t child;
	int status;
	int res;

	if (debug_level) {
		/* just show the command, but don't execute it */
		char **p;

		fputs ("man command =", stderr);
		for (p = args; *p; p++)
			fprintf (stderr, " %s", *p);
		putc ('\n', stderr);
		return;
	}
		
	child = vfork ();
	if (child < 0)
		error (FATAL, errno, _("fork failed"));
	else if (child == 0) {
		char *const envp[] = { NULL };

		execve (MAN, args, envp);
		_exit (127);
	} 

	do {			/* cope with non-restarting system calls */
		res = waitpid (child, &status, 0);
	} while ((res == -1) && (errno == EINTR));
	if (res == -1)
		error (FATAL, 0, _("can't get man command's exit status"));
	else if (status)
		error (CHILD_FAIL, 0,
		       _("man command failed with exit status %d"), status);
}

/* accept key and a pointer to the array address that needs to be filled in,
   fill in address and return 1 if MYDBM_DPTR (key) can be freed otherwise 0 */
static inline int add_arg (datum key, char **argument)
{
    	char *tab;

	tab = strrchr (MYDBM_DPTR (key), '\t');

	if (tab && tab != MYDBM_DPTR (key)) {
		*tab = '\0';
		*argument = xstrdup (MYDBM_DPTR (key));
		*tab = '\t';
		return 1;
	} 

	*argument = MYDBM_DPTR (key);
	return 0;
}

/* simply close db, tidy up, call catman() and then free() the array */
static void do_catman (char *args[], int arg_no, int first_arg)
{
	MYDBM_CLOSE (dbf);

	/* The last argument must be NULL */
	args[arg_no] = NULL;

	catman (args);

	/* don't free the last entry, it's NULL */
	/* don't free the last but one entry, it's our nextkey */
	arg_no -= 2;

	while (arg_no >= first_arg)
		/* all db methods now free() */	
		MYDBM_FREE (args[arg_no--]);
}

#ifdef BTREE
/* we need to reset the cursor position after a reopen */
static inline void reset_cursor (datum key)
{
	int status;
	DBT content; /* dummy */
	
	status = (dbf->seq) (dbf, (DBT *) &key, &content, R_CURSOR);
	if (status == 1)
		status = (dbf->seq) (dbf, (DBT *) &key, &content, R_LAST);
	if (status == -1)
		error (FATAL, errno,
		       _("unable to reset cursor position in %s"), database);
}
#else /* !BTREE */
#  define reset_cursor(key) 		/* nothing */
#endif /* BTREE */

/* find all pages that are in the supplied manpath and section and that are
   ultimate source files. */
static int parse_for_sec (const char *manpath, const char *section)
{
	char *args[MAX_ARGS];
	datum key;
	size_t arg_size, initial_bit;
	int arg_no = 0, message = 1, first_arg;
	
	if (rdopen_db () || dbver_rd (dbf))
		return 1;
		
	args[arg_no++] = xstrdup ("man"); 	/* Name of program */

	/* As we supply a NULL environment to save precious execve() space,
	   we must also supply a locale if necessary */
	if (locale) {
		args[arg_no++] = xstrdup ("-L");	/* locale option */
		args[arg_no++] = xstrdup (locale);	/* The locale */
		initial_bit = sizeof "-L" + strlen (locale) + 1;
	} else
		initial_bit = 0;

	args[arg_no++] = xstrdup ("-caM");	/* options */
	args[arg_no++] = xstrdup (manpath);	/* particular manpath */
	args[arg_no++] = xstrdup (section);	/* particular section */

	first_arg = arg_no;		/* first pagename argument */
	
	initial_bit += sizeof "man" + sizeof "-caM" + 
		       strlen (manpath) + strlen (section) + 2;

	arg_size = initial_bit;
	key = MYDBM_FIRSTKEY (dbf);

	while (MYDBM_DPTR (key) != NULL) {
		int free_key = 1;

		/* ignore db identifier keys */
		if (*MYDBM_DPTR (key) != '$') { 
			datum content;

			content = MYDBM_FETCH (dbf, key);

			if (!MYDBM_DPTR (content))
				error (FATAL, 0,
				       _( "NULL content for key: %s"),
				       MYDBM_DPTR (key));

			/* ignore overflow entries */
			if (*MYDBM_DPTR (content) != '\t') { 
				struct mandata entry;

				split_content (MYDBM_DPTR (content), &entry);

				/* Accept if the entry is an ultimate manual
				   page and the section matches the one we're
				   currently dealing with */
				if (entry.id == ULT_MAN && 
				    strcmp (entry.sec, section) == 0) {
					if (message) {
						printf (_("\nUpdating cat files for section %s of man hierarchy %s\n"),
							section, manpath);
						message = 0;
					}

					free_key = add_arg (key,
							    &(args[arg_no]));
					arg_size += strlen (args[arg_no++]) + 1;

					debug ("arg space free: %d bytes\n",
					       ARG_MAX - arg_size);

					/* Check to see if we have enough room 
					   to add another max sized filename 
					   and that we haven't run out of array 
					   space too */ 
				    	if (arg_size >= ARG_MAX - NAME_MAX ||
				    	    arg_no == MAX_ARGS - 1) {
				    		do_catman (args, arg_no,
							   first_arg);

				    		/* reopen db and tidy up */
				    		if (rdopen_db ())
				    			return 1;
						reset_cursor (key);
						free_key = 1;
				    		arg_no = first_arg;
				    		arg_size = initial_bit;
				    	}
				}

				/* == MYDBM_DPTR (content), freed below */
				entry.addr = NULL;
				free_mandata_elements (&entry);
			}
			
			/* we don't need the content ever again */
			assert (MYDBM_DPTR (content)); /* just to be sure */
			MYDBM_FREE (MYDBM_DPTR (content));
		}

		/* If we are not using the key, free it now */
		if (free_key) {
			datum nextkey;

			nextkey = MYDBM_NEXTKEY (dbf, key);
			MYDBM_FREE (MYDBM_DPTR (key));
			key = nextkey;
		} else 
			key = MYDBM_NEXTKEY (dbf, key);
	}

	if (arg_no > first_arg)
		do_catman (args, arg_no, first_arg);

	arg_no = first_arg - 1;
	while (arg_no >= 0)
		free (args[arg_no--]);

	return 0;
}

static inline int check_access (const char *directory)
{
	if (access (directory, W_OK)) {
		error (0, errno, _("cannot write within %s"), directory);
		return 1;
	}

	return 0;
}
		
int main (int argc, char *argv[])
{
	char *sys_manp;
	char **mp;
	const char **sp;

	program_name = base_name (argv[0]);

	init_debug ();

	/* initialise the locale */
	locale = xstrdup (setlocale (LC_ALL, ""));
	if (!locale) {
		/* Obviously can't translate this. */
		if (!getenv ("MAN_NO_LOCALE_WARNING"))
			error (0, 0, "can't set the locale; make sure $LC_* "
				     "and $LANG are correct");
		locale = xstrdup ("C");
	}
	setenv ("MAN_NO_LOCALE_WARNING", "1", 1);
	bindtextdomain (PACKAGE, LOCALEDIR);
	bindtextdomain (PACKAGE "-gnulib", LOCALEDIR);
	textdomain (PACKAGE);

	if (argp_parse (&argp, argc, argv, 0, 0, 0))
		exit (FAIL);

	for (sp = sections; *sp; sp++)
		debug ("sections: %s\n", *sp);

	/* Deal with the MANPATH */

	/* This is required for get_catpath(), regardless */
	sys_manp = get_manpath (NULL);

	/* pick up the system manpath or use the supplied one */
	if (!manp) {
		manp = get_mandb_manpath ();
		if (!manp)
			manp = sys_manp;
	}

	debug ("manpath=%s\n", manp);

	/* get the manpath as an array of pointers */
	create_pathlist (manp, manpathlist); 
	
	for (mp = manpathlist; *mp; mp++) {
		char *catpath;
		size_t len;

		catpath = get_catpath (*mp, SYSTEM_CAT | USER_CAT);

		if (catpath) { 
			if (is_directory (catpath) != 1) {
				free (catpath);
				continue;
			}
			database = mkdbname (catpath);
		} else {
			if (is_directory (*mp) != 1)
				continue;
			database = mkdbname (*mp);
			catpath = xstrdup (*mp);
		}

		len = strlen (catpath);
		
		for (sp = sections; *sp; sp++) {
			*(catpath + len) = '\0';
			catpath = appendstr (catpath, "/cat", *sp, NULL);
			if (is_directory (catpath) != 1)
				continue;
			if (check_access (catpath))
				continue;
			if (parse_for_sec (*mp, *sp)) {
				error (0, 0, _("unable to update %s"), *mp);
				break;
			}
		}
			
		free (catpath);
	}

	free_pathlist (manpathlist);
	free (locale);
	free (program_name);
	exit (OK);
}
