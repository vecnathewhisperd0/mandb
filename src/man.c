/*
 * man.c: The manual pager
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
 * Mostly written/re-written by Wilf, some routines by Markus Armbruster.
 *
 * CJW: Various robustness, security, and internationalization fixes.
 * Improved HTML support (originally written by Fabrizio Polacco).
 * Rewrite of page location routines for improved maintainability and
 * accuracy.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <termios.h>

#if defined(STDC_HEADERS)
#  include <string.h>
#  include <stdlib.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#else /* no string(s) header */
extern char *strchr(), *strcat();
#endif /* STDC_HEADERS */

#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#else
extern pid_t vfork();
#  define R_OK		4
#  define STDOUT_FILENO	1
#  define STDIN_FILENO	0
#endif /* HAVE_UNISTD_H */

#if defined(HAVE_LIMITS_H)
#  include <limits.h>
#elif defined(HAVE_SYS_PARAM_H)
#  include <sys/param.h>
#endif

#ifndef PATH_MAX
#  ifdef _POSIX_VERSION
#    define PATH_MAX _POSIX_PATH_MAX
#  else /* !_POSIX_VERSION */
#    ifdef MAXPATHLEN
#      define PATH_MAX MAXPATHLEN
#    else /* !MAXPATHLEN */
#      define PATH_MAX 1024
#    endif /* MAXPATHLEN */
#  endif /* _POSIX_VERSION */
#endif /* !PATH_MAX */

static char wd[PATH_MAX];
static char *cwd = wd;

#ifndef PIPE_BUF
#  if defined(_POSIX_VERSION) && defined(_POSIX_PIPE_MAX)
#    define PIPE_MAX _POSIX_PIPE_MAX
#  else /* !_POSIX_PIPE_MAX */
#    if defined(PIPE_MAX) && (PIPE_MAX != INT_MAX)
#      define PIPE_BUF PIPE_MAX
#    else /* !PIPE_MAX */
#      define PIPE_BUF 512
#    endif /* PIPE_MAX */
#  endif /* _POSIX_PIPE_MAX */
#endif /* PIPE_BUF */

#ifdef HAVE_SYS_FILE
#  include <sys/file.h>
#endif /* HAVE_SYS_FILE */

#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>

#if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#ifndef STDC_HEADERS
extern char *getenv();
extern int errno;
#endif

#ifdef HAVE_GETOPT_H
#  include <getopt.h>
#else /* !HAVE_GETOPT_H */
#  include "lib/getopt.h"
#endif /* HAVE_GETOPT_H */

#include <locale.h>
#include <libintl.h>
#define _(String) gettext (String)
#ifdef HAVE_LANGINFO_CODESET
#  include <langinfo.h>
#endif

#include "manconfig.h"
#include "libdb/mydbm.h"
#include "libdb/db_storage.h"
#include "lib/error.h"
#include "lib/cleanup.h"
#include "lib/hashtable.h"
#include "check_mandirs.h"
#include "globbing.h"
#include "ult_src.h"
#include "manp.h"
#include "security.h"
#include "man.h"

#ifdef SECURE_MAN_UID
extern uid_t ruid;
extern uid_t euid;
#endif /* SECURE_MAN_UID */

/* the magic cookie to request preprocessing */
#define PP_COOKIE "'\\\" "

/* the default preprocessor sequence */
#ifndef DEFAULT_MANROFFSEQ
#  define DEFAULT_MANROFFSEQ ""
#endif

/* placeholder for the manual page name in the less prompt string */
#define MAN_PN "$MAN_PN"

/* Some systems lack these */
#ifndef STDIN_FILENO
#  define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#  define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#  define STDERR_FILENO 2
#endif

char *lang;
struct lt {
	const char *lang;
	const char *device;
	const char *charset;
} lang_table[] = {

	/* LESSCHARSET=latin1 means '0x80-0xff is displayable'. */
	/* It does not mean 'ISO-8859-1 charset'. */

	/* roff_device=latin1 means 'groff uses ISO-8859-1 characters'. */
	/* Thus 'ascii' should be used for ISO-8859-{2,3,4,...} languages. */

	/* LANG means language of manpage.  However, for English manpages, */
	/* roff_device and LESSCHARSET are determined by user environment */
	/* (latin1+latin1 for ISO-8859-1 languages and ascii+ascii for */
	/* non-ISO-8859-1 languages). */

	/* LANG		roff_device	LESSCHARSET */
	{ "C"		, "latin1"	, "latin1"	}, /* English */
	{ "POSIX"	, "latin1"	, "latin1"	}, /* English */
	{ "da"		, "latin1"	, "latin1"	}, /* Danish */
	{ "de"		, "latin1"	, "latin1"	}, /* German */
	{ "en"		, "latin1"	, "latin1"	}, /* English */
	{ "es"		, "latin1"	, "latin1"	}, /* Spanish */
	{ "fi"		, "latin1"	, "latin1"	}, /* Finnish */
	{ "fr"		, "latin1"	, "latin1"	}, /* French */
	{ "ga"		, "latin1"	, "latin1"	}, /* Irish */
	{ "is"		, "latin1"	, "latin1"	}, /* Icelandic */
	{ "it"		, "latin1"	, "latin1"	}, /* Italian */
	{ "ja"		, "nippon"	, "ja"		}, /* Japanese */
	{ "ko"		, "ascii8"	, "latin1"	}, /* Korean */
	{ "nl"		, "latin1"	, "latin1"	}, /* Dutch */
	{ "no"		, "latin1"	, "latin1"	}, /* Norwegian */
	{ "pt"		, "latin1"	, "latin1"	}, /* Portuguese */
	{ "sv"		, "latin1"	, "latin1"	}, /* Swedish */
	{ "*"		, "ascii8"	, "latin1"	}, /* universal */
	{ 0		, 0		, 0		} };

/* external formatter programs, one for use without -t, and one with -t */
#define NFMT_PROG "./mandb_nfmt"
#define TFMT_PROG "./mandb_tfmt"
#undef ALT_EXT_FORMAT	/* allow external formatters located in cat hierarchy */

static int global_manpath = -1;	/* global or user manual page hierarchy? */
static int skip;		/* page exists but has been skipped */

#if defined _AIX || defined __sgi
char **global_argv;
#endif

struct candidate {
	const char *req_name;
	char from_db;
	char cat;
	const char *path;
	struct mandata *source;
	struct candidate *next;
};

#define CANDIDATE_FILESYSTEM 0
#define CANDIDATE_DATABASE   1

#ifdef MAN_CATS
static FILE *checked_popen (const char *command, const char *type)
{
	FILE *stream;

#ifdef SECURE_MAN_UID
	if (global_manpath)
		drop_effective_privs ();
#endif /* SECURE_MAN_UID */

	if (debug) {
		fprintf (stderr, "popen (\"%s\", \"%s\")\n",
			 command, type);
		if (strchr (type, 'r'))
			stream = popen (CAT " </dev/null", type);
		else
			stream = popen (CAT " >/dev/null", type);
	} else
		stream = popen (command, type);

#ifdef SECURE_MAN_UID
	if (global_manpath)
		regain_effective_privs ();
#endif /* SECURE_MAN_UID */

	if (stream == NULL)
		error (0, 0, _("can't popen")); 
	return stream;
}
#endif /* MAN_CATS */

char *lang_dir (const char *filename)
{
	char *ld;	/* the lang dir: point to static data */
	const char *fm;	/* the first "/man/" dir */
	const char *sm;	/* the second "/man?/" dir */

	ld = "";
	if (!filename) 
		return ld;

	fm = strstr (filename, "/man/");
	if (!fm)
		return ld;
	sm = strstr (fm + 3, "/man");
	if (!sm)
		return ld;
	if (sm == fm + 4)
		return ld;
	if (sm[5] != '/')
		return ld;
	if (!strchr ("123456789lno", sm[4]))
		return ld;
	/* found a lang dir */
	fm += 5;
	sm = strchr (fm, '/');
	if (!sm)
		return ld;
	ld = xstrdup (fm);
	ld[sm - fm] = '\0';
	if (debug)
		fprintf (stderr, "found lang dir element %s\n", ld);
	return ld;
}

static __inline__ void gripe_system (const char *command, int status)
{
	error (CHILD_FAIL, 0, _("command exited with status %d: %s"),
	       status, command);
}


static int checked_system (const char *command)
{
	int status;

	status = do_system_drop_privs (command);
	if (status != 0)
		gripe_system (command, status);

	return status;
}


extern char *manpathlist[];	/* defined in manp.c     */

/* globals */
int debug = 0;
int quiet = 1;
char *program_name;
char *database;
MYDBM_FILE dbf; 
extern char *extension; /* for globbing.c */

/* locals */
static char *alt_system_name;
static char **section_list;		
static const char *section;
static char *colon_sep_section_list;
static char *preprocessors;
static char *pager;
static char *locale;
static char *internal_locale;
static char *prompt_string;
static char *less;
static char *std_sections[] = STD_SECTIONS;
static char *manp;
static char *external;
static struct hashtable *db_hash = NULL;

static int troff;
static const char *roff_device = NULL;
static int print_where, print_where_cat;
static int catman;
static int local_man_file;
static int findall;
static int update;
static int match_case;

#ifdef MAN_DB_UPDATES
static int update_required = 1;	/* we haven't performed update */
#endif /* MAN_DB_UPDATES */

static int ascii;		/* insert tr in the output pipe */
static int save_cat; 		/* security breach? Can we save the cat? */
static int different_encoding;	/* was an explicit encoding specified? */

static int found_a_stray;		/* found a straycat */

#ifdef MAN_CATS
static char *tmp_cat_file;	/* for open_cat_stream(), close_cat_stream() */
static int cat_comp_pid;			/* dto. */
static int created_tmp_cat;			/* dto. */
static int man_modtime;		/* modtime of man page, for commit_tmp_cat() */
#endif

static const struct option long_options[] =
{
    {"local-file", no_argument, 	0, 'l'},
    {"manpath", required_argument, 	0, 'M'},
    {"pager", required_argument, 	0, 'P'},
    {"sections", required_argument, 	0, 'S'},
    {"all", no_argument, 		0, 'a'},
    {"debug", no_argument, 		0, 'd'},
    {"whatis", no_argument, 		0, 'f'},
    {"help", no_argument, 		0, 'h'},
    {"apropos", no_argument, 		0, 'k'},
    {"version", no_argument, 		0, 'V'},
    {"systems", required_argument, 	0, 'm'},
    {"preprocessor", required_argument,	0, 'p'},
    {"location", no_argument, 		0, 'w'},
    {"where", no_argument,		0, 'w'},
    {"location-cat", no_argument,	0, 'W'},
    {"where-cat", no_argument,		0, 'W'},
    {"locale", required_argument,	0, 'L'},
    {"extension", required_argument,	0, 'e'},
    {"update", no_argument, 		0, 'u'},
    {"prompt", required_argument,	0, 'r'},
    {"default", no_argument,		0, 'D'},
    {"ascii", no_argument,		0, '7'},
    {"catman", no_argument, 		0, 'c'},
    {"encoding", required_argument,	0, 'E'},
    {"ignore-case", no_argument,	0, 'i'},
    {"match-case", no_argument,		0, 'I'},

#ifdef HAS_TROFF
    {"troff", no_argument, 		0, 't'},
    {"troff-device", optional_argument,	0, 'T'},
# ifdef TROFF_IS_GROFF
    {"ditroff", no_argument, 		0, 'Z'},
    {"gxditview", optional_argument,	0, 'X'},
    {"html", optional_argument,		0, 'H'},
# endif
    {0, 0, 0, 0}
};

static const char args[] = "7DlM:P:S:adfhH::kVum:p:tT::wWe:L:Zcr:X::E:iI";

# ifdef TROFF_IS_GROFF
static int ditroff;
static char *gxditview;
static int htmlout;
static char *html_pager;
# endif /* TROFF_IS_GROFF */

#else /* !HAS_TROFF */

    {0, 0, 0, 0}
};

static const char args[] = "7DlM:P:S:adfhkVum:p:wWe:L:cr:E:iI";

#endif /* HAS_TROFF */

/* issue a usage message, then exit with the given status */
static void usage (int status)
{
#ifdef HAS_TROFF
#  ifdef TROFF_IS_GROFF
	const char formatter[] = "groff";
#  else
	const char formatter[] = "troff";
#  endif /* TROFF_IS_GROFF */
#endif /* HAS_TROFF */
	
#ifdef HAS_TROFF
	printf (_(
		"usage: %s [-c|-f|-k|-w|-tZT device] [-adlhu7V] [-Mpath] [-Ppager] [-Slist]\n"
		"           [-msystem] [-pstring] [-Llocale] [-eextension] [section] page ...\n"),
		program_name);
#else
	printf (_(
		"usage: %s [-c|-f|-k|-w] [-adlhu7V] [-Mpath] [-Ppager] [-Slist] [-msystem]\n"
		"           [-pstring] [-Llocale] [-eextension] [section] page ...\n"),
		program_name);
#endif

	puts (_(
		"-a, --all                   find all matching manual pages.\n"
		"-d, --debug                 emit debugging messages.\n"
		"-e, --extension             limit search to extension type `extension'.\n"
		"-f, --whatis                equivalent to whatis.\n"
		"-k, --apropos               equivalent to apropos.\n"
		"-w, --where, --location     print physical location of man page(s).\n"
		"-W, --where-cat,\n"
		"    --location-cat          print physical location of cat file(s).\n"
		"-l, --local-file            interpret `page' argument(s) as local filename(s).\n"
		"-u, --update                force a cache consistency check.\n"
		"-i, --ignore-case           look for pages case-insensitively (default).\n"
		"-I, --match-case            look for pages case-sensitively.\n"
		"-r, --prompt string         provide the `less' pager with a prompt\n"
		"-c, --catman                used by catman to reformat out of date cat pages.\n"
		"-7, --ascii                 display ASCII translation of certain latin1 chars.\n"
		"-E, --encoding encoding     use the selected nroff device and display in pager."));
#ifdef HAS_TROFF
	printf (_(
		"-t, --troff                 use %s to format pages.\n"
		"-T, --troff-device device   use %s with selected device.\n"),
		formatter, formatter);
# ifdef TROFF_IS_GROFF
	puts (_("-H, --html                  use lynx or argument to display html output.\n"
		"-Z, --ditroff               use groff and force it to produce ditroff.\n"
		"-X, --gxditview             use groff and display through gditview (X11):\n"
		"                            -X = -TX75, -X100 = -TX100, -X100-12 = -TX100-12."));
# endif /* TROFF_IS_GROFF */
#endif /* HAS_TROFF */

	puts (_(
		"-D, --default               reset all options to their default values.\n"
		"-M, --manpath path          set search path for manual pages to `path'.\n"
		"-P, --pager pager           use program `pager' to display output.\n"
		"-S, --sections list         use colon separated section list.\n"
		"-m, --systems system        search for man pages from other unix system(s).\n"
		"-L, --locale locale         define the locale for this particular man search.\n"
		"-p, --preprocessor string   string indicates which preprocessors to run.\n"
		"                             e - [n]eqn   p - pic    t - tbl\n"
		"                             g - grap     r - refer  v - vgrind\n"
		"-V, --version               show version.\n"
		"-h, --help                  show this usage message."));

	exit (status);
}

/*
 * changed these messages from stdout to stderr,
 * (Fabrizio Polacco) Fri, 14 Feb 1997 01:30:07 +0200
 */
static void gripe_no_name (const char *section)
{
	if (section)
		fprintf (stderr,
			 _("What manual page do you want from section %s?\n"),
			 section);
	else
		fputs (_("What manual page do you want?\n"), stderr);

	exit (FAIL);
}

static struct termios tms;
static int tms_set = 0;

void set_term ()
{
	if (tms_set)
		tcsetattr (0, TCSANOW, &tms);
}

void get_term()
{
	if (isatty (1)) {
		if (debug)
			fprintf(stderr, "is a tty\n");
		tcgetattr (0, &tms);
		if (!tms_set++)
			atexit (set_term);
	}
}

/* Line length detection code adapted from Andries Brouwer's man. */

/* Try to determine the line length to use.
 * Preferences: 1. MANWIDTH, 2. ioctl, 3. COLUMNS, 4. 80
 *
 * joey, 950902
 */

#include <sys/ioctl.h>

static int line_length = 80;

static void store_line_length (void)
{
	const char *columns;
	int width;

	line_length = 80;

	columns = getenv ("MANWIDTH");
	if (columns != NULL) {
		width = atoi (columns);
		if (width > 0) {
			line_length = width;
			return;
		}
	}

#ifdef TIOCGWINSZ
	if (isatty(0) && isatty(1)) { /* Jon Tombs */
		struct winsize wsz;

		if (ioctl (0, TIOCGWINSZ, &wsz))
			perror ("TIOCGWINSZ failed\n");
		else if (wsz.ws_col) {
			line_length = wsz.ws_col;
			return;
		}
	}
#endif

	columns = getenv ("COLUMNS");
	if (columns != NULL) {
		width = atoi (columns);
		if (width > 0)
			line_length = width;
	}
}

static int get_roff_line_length (void)
{
	/* groff >= 1.18 defaults to 78. */
	if (!troff && line_length != 80) {
		int length = line_length * 39 / 40;
		if (length > line_length - 2)
			return line_length - 2;
		else
			return length;
	} else
		return 0;
}

static char *add_roff_line_length (char *filter, int *save_cat)
{
	int length = get_roff_line_length ();
	if (length) {
		char options[64];
		if (debug)
			fprintf (stderr, "Using %d-character lines\n", length);
		*save_cat = 0;
		sprintf (options, " -rLL=%dn -rLT=%dn", length, length);
		return strappend (NULL, filter, options, NULL);
	}
	return filter;
}

/*
 * changed these messages from stdout to stderr,
 * (Fabrizio Polacco) Fri, 14 Feb 1997 01:30:07 +0200
 */
static __inline__ void gripe_no_man (const char *name, const char *sec)
{
	if (troff) {
		fprintf (stderr, _("No source manual entry for %s"), name);
	} else {

/* On AIX and IRIX, fall back to the vendor supplied browser */
#if defined _AIX || defined __sgi

		putenv ("MANPATH=");  /* reset the MANPATH variable */
		execv ("/usr/bin/man", global_argv);
#else
		fprintf (stderr, _("No manual entry for %s"), name);
#endif
	}

	if (sec)
		fprintf (stderr, _(" in section %s\n"), sec);
	else
		putc ('\n', stderr);

#ifdef UNDOC_COMMAND
	fprintf (stderr,
		 _("See '%s' for help when manual pages are not available.\n"),
		 UNDOC_COMMAND);
#endif
}

/* fire up the appropriate external program */
static void do_extern (char *argv[])
{
	/* privs are already dropped */
	argv[0] = basename (external);
	execv (external, argv);
	exit (FATAL);
}

/* lookup $MANOPT and if available, put in *argv[] format for getopt() */
static __inline__ char **manopt_to_env (int *argc)
{
	char *manopt, *opt_start, **argv;

	manopt = getenv ("MANOPT");
	if (manopt == NULL || *manopt == '\0')
		return NULL;

	opt_start = manopt = xstrdup (manopt);

	/* allocate space for the program name */
	*argc = 0;
	argv = (char **) xmalloc ((*argc + 3) * sizeof (char *));
	argv[(*argc)++] = program_name;
	
	/* for each [ \t]+ delimited string, allocate an array space and fill
	   it in. An escaped space is treated specially */	
	while (*manopt) {
		switch (*manopt) {
			case ' ':
			case '\t':
				if (manopt != opt_start) {
					*manopt = '\0';
					argv = (char **) 
					       xrealloc (argv, (*argc + 3) * 
							 sizeof (char *));
					argv[(*argc)++] = opt_start;
				}
				while (isspace (*(manopt + 1)))
					*++manopt = '\0';
				opt_start = manopt + 1;
				break;
			case '\\':
				if (*(manopt + 1) == ' ')
					manopt++;
				break;
			default:
				break;
		}
		manopt++;
	}

	if (*opt_start)
		argv[(*argc)++] = opt_start;
	argv[*argc] = NULL;			

	return argv;
}

/* Return char array with 'less' special chars escaped. Uses static storage. */
static __inline__ const char *escape_less (const char *string)
{
	static char *escaped_string; 
	char *ptr;

	/* 2*strlen will always be long enough to hold the escaped string */
	ptr = escaped_string = (char *) xrealloc (escaped_string, 
						  2 * strlen (string) + 1);
	
	while (*string) {
		if (*string == '?' ||
		    *string == ':' ||
		    *string == '.' ||
		    *string == '%' ||
		    *string == '\\')
			*ptr++ = '\\';

		*ptr++ = *string++;
	}

	*ptr = *string;
	return escaped_string;
}

#ifdef MAN_DB_UPDATES
/* test for new files. If any found return 1, else 0 */
static int need_to_rerun (void)
{
	int rerun = 0;
	char **mp;

	for (mp = manpathlist; *mp; mp++) {
		char *catpath;

		global_manpath = is_global_mandir (*mp);
		if (!global_manpath)
			drop_effective_privs ();

		catpath = get_catpath
			(*mp, global_manpath ? SYSTEM_CAT : USER_CAT);
		if (catpath) {
			database = mkdbname (catpath);
			free (catpath);
		} else {
			database = mkdbname (*mp);
		}

		if (update)
			reset_db_time ();

		/* if no change or couldn't update (EOF), forget it */
		if (update_db (*mp) > 0)
			rerun = 1;

		if (!global_manpath)
			regain_effective_privs ();
	}

	/* only ever need to do this once */
	update_required = 0;
	
	return rerun;
}
#endif /* MAN_DB_UPDATES */


/* man issued with `-l' option */
static int local_man_loop (const char *argv)
{
	int exit_status = OK;
	int local_mf = local_man_file;

	drop_effective_privs ();
	local_man_file = 1;
	if (strcmp (argv, "-") == 0)
		display (NULL, "", NULL, "(stdin)", NULL);
	else {
		struct stat st;
#ifdef COMP_SRC
		struct compression *comp;
#endif /* COMP_SRC */

		/* See if we need to decompress the file(s) first */
		if (cwd[0]) {
			if (debug)
				fprintf (stderr, "chdir %s\n", cwd);
			if (chdir (cwd)) {
				error (0, errno, _("can't chdir to %s"), cwd);
				regain_effective_privs ();
				return 0;
			}
		}

		/* Check that the file exists and isn't e.g. a directory */
		if (stat (argv, &st)) {
			error (0, errno, "%s", argv);
			return NOT_FOUND;
		}

		if (S_ISDIR (st.st_mode)) {
			error (0, EISDIR, "%s", argv);
			return NOT_FOUND;
		}

#ifdef COMP_SRC
		comp = comp_info (argv);
		if (comp)
			if (!decompress(argv, comp))
				exit_status = CHILD_FAIL;
#endif /* COMP_SRC */
		if (exit_status == OK) {
			lang = lang_dir (argv);
			if (!display (NULL, argv, NULL, basename (argv),
				      NULL)) {
				if (local_mf)
					error (0, errno, "%s", argv);
				exit_status = NOT_FOUND;
			}
		}

#ifdef COMP_SRC
		remove_ztemp ();
#endif /* COMP_SRC */
	}
	local_man_file = local_mf;
	regain_effective_privs ();
	return exit_status;
}

int main (int argc, char *argv[])
{
	int argc_env, status = 0, exit_status = OK;
	char **argv_env;
	const char *tmp;
	char *multiple_locale = NULL;
	extern int optind;
	void (int_handler) (int);

	program_name = xstrdup (basename (argv[0]));

	umask (022);
	/* initialise the locale */
	if (!setlocale (LC_ALL, ""))
		/* Obviously can't translate this. */
		error (0, 0, "can't set the locale; make sure $LC_* and $LANG "
			     "are correct");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);

	internal_locale = setlocale (LC_MESSAGES, NULL);
	/* Use LANGUAGE only when LC_MESSAGES locale category is
	 * neither "C" nor "POSIX". */
	if (internal_locale && strcmp (internal_locale, "C") &&
	    strcmp (internal_locale, "POSIX")) {
		multiple_locale = getenv ("LANGUAGE");
		if (multiple_locale)
			internal_locale = multiple_locale;
	}
	if (internal_locale != NULL)
		internal_locale = xstrdup (internal_locale);
	else
		internal_locale = "C";

/* export argv, it might be needed when invoking the vendor supplied browser */
#if defined _AIX || defined __sgi
	global_argv = argv;
#endif

	{ /* opens base streams in case someone like "info" closed them */
		struct stat buf;
		if (fileno (stdin) < 0 ||
		    ((fstat (fileno (stdin), &buf) < 0) && (errno == EBADF))) 
			freopen ("/dev/null", "r", stdin);
		if (fileno (stdout) < 0 ||
		    ((fstat (fileno (stdout), &buf) < 0) && (errno == EBADF)))
			freopen ("/dev/null", "w", stdout);
		if (fileno (stderr) < 0 ||
		    ((fstat (fileno (stderr), &buf) < 0) && (errno == EBADF)))
			freopen ("/dev/null", "w", stderr);
	}

	/* This will enable us to do some profiling and know
	where gmon.out will end up. Must chdir(cwd) before we return */
	if (!getcwd (cwd, PATH_MAX - 1))
		cwd[0] = '\0';

	/* First of all, find out if $MANOPT is set. If so, put it in 
	   *argv[] format for getopt to play with. */
	argv_env = manopt_to_env (&argc_env);
	if (argv_env) {
		man_getopt (argc_env, argv_env);
		optind = 0;
	}

	/* give the actual program args to getopt */
	man_getopt (argc, argv);

#ifdef SECURE_MAN_UID
	/* record who we are and drop effective privs for later use */
	init_security ();
#endif

	if (!catman)
		store_line_length();

	read_config_file ();

	/* if the user wants whatis or apropos, give it to them... */
	if (external)
		do_extern (argv);

	get_term(); /* stores terminal settings */
	if (debug)
		fprintf (stderr, "real user = %d; effective user = %d\n",
			 ruid, euid);

#ifdef HAVE_SETLOCALE
	/* close this locale and reinitialise if a new locale was 
	   issued as an argument or in $MANOPT */
	if (locale) {
		internal_locale = setlocale (LC_ALL, locale);
		if (internal_locale == NULL)
			internal_locale = locale;

		internal_locale = xstrdup (internal_locale);
		if (debug)
			fprintf(stderr,
				"main(): locale = %s, internal_locale = %s\n",
				locale, internal_locale);
		if (internal_locale) {
			extern int _nl_msg_cat_cntr;
			setenv ("LANGUAGE", internal_locale, 1);
			++_nl_msg_cat_cntr;
		}
	}

#endif /* HAVE_SETLOCALE */

#ifdef TROFF_IS_GROFF
	if (htmlout) {
		if (!html_pager) {
			html_pager = getenv ("BROWSER");
			if (!html_pager)
				html_pager = WEB_BROWSER;
		}
		pager = html_pager;
	}
#endif
	if (pager == NULL) {
		pager = getenv ("PAGER");
		if (pager == NULL)
			pager = get_def("pager", PAGER);
	}
	if (*pager == '\0')
		pager = get_def("cat", CAT);

	if (prompt_string == NULL)
#ifdef LESS_PROMPT
		prompt_string = LESS_PROMPT;
#else
		prompt_string = _(
				" Manual page " MAN_PN
				" ?ltline %lt?L/%L.:byte %bB?s/%s..?e (END):"
				"?pB %pB\\%..");
#endif

	/* Restore and save $LESS in $MAN_ORIG_LESS so that recursive uses
	 * of man work as expected.
	 */
	less = getenv ("MAN_ORIG_LESS");
	if (less == NULL)
		less = getenv ("LESS");
	setenv ("MAN_ORIG_LESS", less ? less : "", 1);

	if (debug)
		fprintf (stderr, "\nusing %s as pager\n", pager);

	if (optind == argc)
		gripe_no_name (NULL);

	signal (SIGINT, int_handler);

	/* man issued with `-l' option */
	if (local_man_file) {
		while (optind < argc) {
			exit_status = local_man_loop (argv[optind]);
			optind++;
		}
		exit (exit_status);
	}

	if (manp == NULL) {
		char tmp_locale[3];
		int idx;

		manp = add_nls_manpath (manpath (alt_system_name), 
					internal_locale);
		/* Handle multiple :-separated locales in LANGUAGE */
		idx = multiple_locale ? strlen (multiple_locale) : 0;
		while (idx) {
			while (idx && multiple_locale[idx] != ':')
				idx--;
			if (multiple_locale[idx] == ':')
				idx++;
			tmp_locale[0] = multiple_locale[idx];
			tmp_locale[1] = multiple_locale[idx + 1];
			tmp_locale[2] = 0;
			/* step back over preceding ':' */
			if (idx) idx--;
			if (idx) idx--;
			if (debug)
				fprintf (stderr, "checking for locale %s\n",
					 tmp_locale);
			manp = add_nls_manpath (manp, tmp_locale);
		}
	} else
		free (manpath (NULL));

	create_pathlist (manp, manpathlist);

	if (debug)
		fprintf (stderr, "*manpath search path* = %s\n", manp);

	/* finished manpath processing, regain privs */
	regain_effective_privs ();

	section_list = get_section_list ();

#ifdef MAN_DB_UPDATES
	/* If `-u', do it now. */
	if (update)
		(void) need_to_rerun ();
#endif /* MAN_DB_UPDATES */

	while (optind < argc) {
		static int maybe_section = 0;
		const char *nextarg = argv[optind++];

		/*
     		 * See if this argument is a valid section name.  If not,
      		 * is_section returns NULL.
      		 */
		tmp = is_section (nextarg);
		if (tmp) {
			section = tmp;
			if (debug)
				fprintf (stderr, "\nsection: %s\n", section);
			maybe_section = 1;
		}

		if (maybe_section) {
			if (optind < argc)
				/* e.g. 'man 3perl Shell' */
				nextarg = argv[optind++];
			else
				/* e.g. 'man 9wm' */
				section = NULL;
				/* ... but leave maybe_section set so we can
				 * tell later that this happened.
				 */
		}

		/* this is where we actually start looking for the man page */
		skip = 0;
		status = man (nextarg);

		/* clean out the cache of database lookups for each man page */
		hash_free (db_hash);
		db_hash = NULL;

		if (section && maybe_section) {
			if (!status && !catman) {
				/* Maybe the section wasn't a section after
				 * all? e.g. 'man 9wm fvwm'.
				 */
				if (debug)
					fprintf (stderr,
						 "\nRetrying section %s as "
						 "name\n", section);
				tmp = section;
				section = NULL;
				status = man (tmp);
				hash_free (db_hash);
				db_hash = NULL;
				/* ... but don't gripe about it if it doesn't
				 * work!
				 */
				if (status) {
					/* It was a name after all, so arrange
					 * to try the next page again with a
					 * null section.
					 */
					nextarg = tmp;
					--optind;
				} else
					/* No go, it really was a section. */
					section = tmp;
			}
		}

		if (!status && !catman) {
			if (!skip) {
				if (strchr (nextarg, '/'))
					exit_status = local_man_loop (nextarg);
				else
					exit_status = NOT_FOUND;
				if (exit_status == NOT_FOUND) {
					if (!section && maybe_section &&
					    isdigit (nextarg[0]))
						gripe_no_name (nextarg);
					else
						gripe_no_man (nextarg, section);
				}
			}
		} else {
			if (debug)
				fprintf(stderr,
					"\nFound %d man pages\n", status);
			if (catman) {
				printf ("%s", nextarg);
				if (section)
					printf ("(%s)", section);
				if (optind != argc)
					fputs (", ", stdout);
				else
					fputs (".\n", stdout);
			}
		}

		maybe_section = 0;

		chkr_garbage_detector ();
	}

	drop_effective_privs ();

	/* For profiling */
	if (cwd[0])
		chdir (cwd);

	exit (exit_status);
}

/* parse the arguments contained in *argv[] and set appropriate vars */
static void man_getopt (int argc, char *argv[])
{
	int c, option_index; /* not used, but required by getopt_long() */
	static int apropos, whatis; /* retain values between calls */

	while ((c = getopt_long (argc, argv, args,
				 long_options, &option_index)) != EOF) {

		switch (c) {

			case 'l':
				local_man_file = 1;
				break;
			case 'e':
				extension = optarg;
				break;
			case 'M':
				manp = optarg;
				break;
		    	case 'P':
				pager = optarg;
				break;
		    	case 'S':
				if (*optarg)
					colon_sep_section_list = optarg;
				break;
			case 'V':
				ver();
				break;
		    	case 'a':
				findall = 1; 
				break;
			case 'u':
				update = 1;
				break;
			case 'i':
				match_case = 0;
				break;
			case 'I':
				match_case = 1;
				break;
			case 'c':
				catman = 1;
				break;
		    	case 'd':
				debug = 1;
				break;
		    	case 'f':
				external = WHATIS;
				apropos = 1;
				break;
		    	case 'k':
				external = APROPOS;
				whatis = 1;
				break;
		    	case 'm':
				alt_system_name = optarg;
				break;
			case 'L':
				locale = optarg;
				break;
		    	case 'p':
				preprocessors = optarg;
				break;
			case '7':
				ascii = 1;
				break;
			case 'E':
				roff_device = optarg;
				different_encoding = 1;
				break;
#ifdef HAS_TROFF
		    	case 't':
				troff = 1;
				break;

			case 'T':
				/* @@@ traditional nroff knows -T,
				   troff does not (gets ignored).
				   All incarnations of groff know it.
				   Why does -T imply -t? */
				/* as this is an optional argument */
				roff_device = (optarg ? optarg : "ps");
				troff = 1;
				break;
			case 'X':
#ifdef TROFF_IS_GROFF
				troff = 1;
				gxditview = (optarg ? optarg : "75");
#endif /* TROFF_IS_GROFF */
				break;
			case 'H':
#ifdef TROFF_IS_GROFF
				html_pager = (optarg ? optarg : 0);
				htmlout = 1;
				troff = 1;
				roff_device = "html";
#endif /* TROFF_IS_GROFF */
				break;
			case 'Z':
#ifdef TROFF_IS_GROFF
				ditroff = 1;
				troff = 1;
#endif /* TROFF_IS_GROFF */
				break;
#endif /* HAS_TROFF */
		    	case 'w':
				print_where = 1;
				break;
			case 'W':
				print_where_cat = 1;
				break;
			case 'r':
				prompt_string = optarg;
				break;
			case 'D':
		    		/* discard all preset options */
		    		local_man_file = findall = update = catman =
					debug = troff =
					print_where = print_where_cat =
					ascii = different_encoding =
					match_case = 0;
#ifdef TROFF_IS_GROFF
				ditroff = 0;
				gxditview = NULL;
				htmlout = 0;
				html_pager = NULL;
#endif
		    		roff_device = extension = pager = locale
		    			     = colon_sep_section_list
		    			     = alt_system_name = external
		    			     = preprocessors = manp = NULL;
		    		break;
		    	case 'h':
		    		usage(OK);
		    		break;
		    	default:
				usage(FAIL);
				break;
		}
	}

	/* check for incompatible options */
	if (troff + whatis + apropos + catman +
	    (print_where || print_where_cat) > 1) {
		error (0, 0,
		       strappend (NULL,
				  troff ? "-[tTZH] " : "",
				  whatis ? "-f " : "",
				  apropos ? "-k " : "",
				  catman ? "-c " : "",
				  print_where ? "-w " : "",
				  print_where_cat ? "-W " : "",
				  _(": incompatible options"), NULL));
		usage (FAIL);
	}
}

/*
 * Check to see if the argument is a valid section number. 
 * If the name matches one of
 * the sections listed in section_list, we'll assume that it's a section.
 * The list of sections in config.h simply allows us to specify oddly
 * named directories like .../man3f.  Yuk.
 */
static __inline__ const char *is_section (const char *name)
{
	char **vs;

	for (vs = section_list; *vs; vs++) {
		if (STREQ (*vs, name))
			return name;
		if (strlen (*vs) == 1 && STRNEQ (*vs, name, 1))
			return name;
	}
	return NULL;
}

/* Snarf pre-processors from file, return (static) string or NULL on failure */
static const char *get_preprocessors_from_file (const char *file)
{
	char *directive = NULL;
#ifdef PP_COOKIE
	FILE *fp;
	static char line[128];

	if (*file == '\0')
		return NULL;

	drop_effective_privs ();
	fp = fopen (file, "r");
	if (fp) {
		if (fgets (line, sizeof (line), fp)) {
			if (!memcmp (line, PP_COOKIE, 4)) {
				int len;
				directive = line + 4;
				/* strip trailing newline */
				len = strlen (directive);
				if (len && (directive[len - 1] == '\n'))
					directive[len - 1] = 0;
			}
		}
		fclose (fp);
	}
	regain_effective_privs ();

	/* if we couldn't read the first line from file, or we didn't
	   find PP_COOKIE, then directive == NULL */
#endif
	return directive;
}


/* Determine pre-processors, set save_cat and return
   (static) string */
static const char *get_preprocessors (const char *file, const char *dbfilters)
{
	const char *pp_string;
	const char *pp_source;

	/* try in order: database, command line, file, environment, default */
	/* command line overrides the database, but database empty overrides default */
	if (dbfilters && (dbfilters[0] != '-') && !preprocessors) {
		pp_string = dbfilters;
		pp_source = "database";
		save_cat = 1;
	} else if ((pp_string = preprocessors)) {
		pp_source = "command line";
		save_cat = 0;
	} else if ((pp_string = get_preprocessors_from_file (file))) {
		pp_source = "file";
		save_cat = 1;
	} else if ((pp_string = getenv ("MANROFFSEQ"))) {
		pp_source = "environment";
		save_cat = 0;
	} else if (!dbfilters) {
		pp_string = DEFAULT_MANROFFSEQ;
		pp_source = "default";
		save_cat = 1;
	} else {
		pp_string = "";
		pp_source = "no filters";
		save_cat = 1;
	}

	if (debug)
		fprintf (stderr, "pre-processors `%s' from %s\n",
			 pp_string, pp_source);
	return pp_string;
}

/* This is so that we can store the temp file name used when input is
 * stdin and remove it on exit.
 */
static char *stdin_tmpfile;
static int stdin_tmpfile_fd;

static void remove_stdintmp (void)
{
	if (stdin_tmpfile) {
		if (stdin_tmpfile_fd >= 0)
			close (stdin_tmpfile_fd);
		(void) remove_with_dropped_privs (stdin_tmpfile);
		free (stdin_tmpfile);
		stdin_tmpfile = NULL;
	}
}

static __inline__ void create_stdintmp (void)
{
	/* This is basically copied from create_ztemp() in compression.c;
	 * it therefore shouldn't introduce any new security holes -- PMM
	 */
	int oldmask = umask (022);
	drop_effective_privs ();
	stdin_tmpfile_fd = create_tempfile ("sman", &stdin_tmpfile);

	if (stdin_tmpfile_fd < 0)
		error (FATAL, errno, _("can't create a temporary filename"));
	regain_effective_privs ();
	umask (oldmask);
	atexit (remove_stdintmp);
}

/* Determine roff_device and LESSCHARSET */
static void determine_lang_table (const char *lang)
{
	int j;
	int chosen_locale;
	char *ctype;
	int is_utf8;
	if (lang && *lang)
		chosen_locale = 1;
	else {
		/* English manpages */
		chosen_locale = 0;
		lang = internal_locale;
	}
	for (j = 0; lang_table[j].lang; j++) {
		if (STRNEQ (lang_table[j].lang, lang,
			    strlen (lang_table[j].lang))
		    || lang_table[j].lang[0] == '*') {
			if (chosen_locale) {
				roff_device = lang_table[j].device;
				putenv (strappend (NULL, "LESSCHARSET=",
						   lang_table[j].charset,
						   NULL));
			} else if (!strcmp (lang_table[j].device, "latin1")) {
				roff_device = "latin1";
				putenv ("LESSCHARSET=latin1");
			} else {
				roff_device = "ascii";
				putenv ("LESSCHARSET=ascii");
			}
			break;
		}
	}

	/* This is done at the end for now so that we can special-case the
	 * nippon device.
	 */
#ifdef HAVE_LANGINFO_CODESET
	ctype = setlocale (LC_CTYPE, "");
	is_utf8 = !strcmp (nl_langinfo (CODESET), "UTF-8");
#else
	ctype = setlocale (LC_CTYPE, NULL);
	is_utf8 = (strstr (ctype, "UTF-8") != NULL);
#endif
	if (is_utf8) {
		if (debug)
			fprintf (stderr, "Using UTF-8 locale\n");
		if (!STREQ (roff_device, "nippon"))
			roff_device = "utf8";
		putenv ("LESSCHARSET=utf-8");
		/* Can't cat these for now. */
		save_cat = 0;
	}
}

/* Return command (malloced string) to format file to stdout */
static __inline__ char *make_roff_command (const char *dir, const char *file,
					   const char *dbfilters)
{
	const char *pp_string;
	char *fmt_prog;
	char *command;

	if (!*file) {
		/* file == "": this means we are reading input from stdin.
		 * Unfortunately we need to read the first line or so to
		 * determine the preprocessor filename. So we read a bit
		 * into a temporary file which we give to get_preprocessors
		 * and then prepend to the rest of stdin by putting cat
		 * in the beginning of the pipeline we pass to system().
		 * I can't think of a better way to do this :-<
		 * [this code seems remarkably hairy for what it does...]
		 */
		/* get_preprocessors uses 128 byte buffer, no point using
		 * more here
		 */
#define STDIN_SNARFLEN 128
		char bf[STDIN_SNARFLEN];
		int ct = 0;		       /* number of bytes in bf[] */
		int ctot, r;
		/* Ignore SIGPIPE; we want to be notified by write returning
		 * EPIPE.
		 */
		RETSIGTYPE (*old_handler)() = signal (SIGPIPE, SIG_IGN);

		/* read data into bf; we don't want to use the stream stuff
		 * because we don't want data to be lost in the buffers, not
		 * read here or by the system()-run pipeline.
		 */
		do {
			errno = 0;
			r = read (STDIN_FILENO, bf + ct, (128 - ct));
			if (r != -1)
				ct += r;
			/* stop on EOF (r==0), bf full (ct==128) or error
			 * (errno not 0 or EINTR)
			 */
		} while (r != 0 && ct < STDIN_SNARFLEN &&
			 (errno == 0 || errno == EINTR));

		if (errno != 0)
			error (FATAL, errno,
			       _("error trying to read from stdin"));

		drop_effective_privs ();
		create_stdintmp ();
		/* write bf to stdin_tmpfile_fd */
		ctot = ct;
		do {
			errno = 0;
			r = write (stdin_tmpfile_fd, bf + (ctot - ct), ct);
			if (r != -1)
				ct -= r;
		} while (ct > 0 && (errno == 0 || errno == EINTR));
		if (errno != 0)
			error (FATAL, errno,
			       _("error writing to temporary file %s"),
			       stdin_tmpfile);

		close (stdin_tmpfile_fd);
		/* ensure we don't try to close it again */
		stdin_tmpfile_fd = -1;
		signal (SIGPIPE, old_handler);

		pp_string = get_preprocessors (stdin_tmpfile, dbfilters);
		regain_effective_privs ();
	} else
		pp_string = get_preprocessors (file, dbfilters);

#ifdef ALT_EXT_FORMAT
	/* Check both external formatter locations */
	if (dir) {
		char *catpath = get_catpath
			(dir, global_manpath ? SYSTEM_CAT : USER_CAT);

		/* If we have an alternate catpath */
		if (catpath) {
			fmt_prog = strappend (catpath, "/",
					      troff ? TFMT_PROG : NFMT_PROG, 
					      NULL);
			if (access (fmt_prog, X_OK)) {
				free (fmt_prog);
				fmt_prog = xstrdup (troff ? TFMT_PROG :
							    NFMT_PROG);
				if (access (fmt_prog, X_OK)) {
					free (fmt_prog);
					fmt_prog = NULL;
				}
			}
		/* If we don't */
		} else {
#endif /* ALT_EXT_FORMAT */

			fmt_prog = xstrdup (troff ? TFMT_PROG : NFMT_PROG);
			if (access (fmt_prog, X_OK)) {
				free (fmt_prog);
				fmt_prog = NULL;
			}

#ifdef ALT_EXT_FORMAT
		}
	} else
		fmt_prog = NULL;
#endif /* ALT_EXT_FORMAT */
	
	if (debug && fmt_prog)
		fprintf (stderr, "External formatter %s\n", fmt_prog);
				
	if (!fmt_prog) {
		/* we don't have an external formatter script */
		char *dev;	/* either " -T<mumble>" or "" */
		int using_tbl = 0;

		/* Load the roff_device value dependent on the language dir
		 * in the path.
		 */
		if (!troff && !different_encoding)
			determine_lang_table (lang);

		/* tell grops to guess the page size */
		if (roff_device && strcmp (roff_device, "ps") == 0)
			roff_device = strappend (NULL, "ps -P-g ", NULL);
		if (gxditview && roff_device)
			dev = strappend (NULL, " -X -T", roff_device, NULL);
		else if (gxditview)
			dev = strappend (NULL, " -TX", gxditview, NULL);
		else if (roff_device)
			dev = strappend (NULL, " -T", roff_device, NULL);
		else
			dev = "";

		if (*file) {
			char *esc_file = escape_shell (file);
			command = strappend (NULL, get_def ("soelim", SOELIM),
					     " ", esc_file, NULL); 
			free (esc_file);
		} else {
			char *esc_tmpfile = escape_shell (stdin_tmpfile);
			/* Reading from stdin: use cat to pick up the part we
			 * read in to figure out the format pipeline.
			 * ? is '-' as a cat argument standard?
			 * If not we could try "(cat tempfile; cat) | SOELIM..."
			 */
			command = strappend (NULL, get_def ("cat", CAT), " ",
					     esc_tmpfile, " - | ",
					     get_def ("soelim", SOELIM), NULL);
			free (esc_tmpfile);
		}

		/* Preformatted pages get standard 80-character lines. */
		if (!catman) {
			/* This code is for groff < 1.18. groff >= 1.18 uses
			 * a different mechanism; see add_roff_line_length().
			 * However, this does no harm, so it's here for
			 * backward compatibility.
			 */
			int roff_line_length = get_roff_line_length();
			if (roff_line_length) {
				char ll_macro[32], *new_command;
				if (debug)
					fprintf (stderr,
						 "Using %d-character lines\n",
						 roff_line_length);
				sprintf (ll_macro, ".ll %dn",
					 roff_line_length);
				new_command = strappend (NULL, "(echo '",
							 ll_macro, "'; ",
							 command, ")", NULL);
				free (command);
				command = new_command;
				save_cat = 0;
			}
		}

		do {
			char *filter;
			int wants_dev = 0; /* filter wants a dev argument */

			/* set filter according to *pp_string, on
                           errors set filter to "" */
			switch (*pp_string) {
			case 'e':
				filter = troff ? get_def ("eqn", EQN) 
					       : get_def ("neqn", NEQN);
				wants_dev = 1;
				break;
			case 'g':
				filter = get_def ("grap", GRAP);
				break;
			case 'p':
				filter = get_def ("pic", PIC);
				break;
			case 't':
				filter = get_def ("tbl", TBL);
				using_tbl = 1;
				break;
			case 'v':
				filter = get_def ("vgrind", VGRIND);
				break;
			case 'r':
				filter = get_def ("refer", REFER);
				break;
			case 0:
				/* done with preprocessors, now add roff */
#ifdef TROFF_IS_GROFF
				if (troff && ditroff) 
					filter = strappend (NULL, 
							    get_def ("troff",
								     TROFF), 
							    " -Z", NULL);
				else
#endif
                                if (troff)
					filter = get_def ("troff", TROFF);
                                else {
					filter = get_def ("nroff", NROFF);
#ifdef TROFF_IS_GROFF
					filter = add_roff_line_length
						(filter, &save_cat);
#endif
				}

				wants_dev = 1;
				break;
			default:
				filter = "";
			}

			if (*filter) {
				command = strappend (command,
						     " | ",
						     filter,
						     wants_dev ? dev : "",
						     NULL);
			} else {
				assert (*pp_string); /* didn't fail on roff */
				error (0, 0,
				       _("ignoring unknown preprocessor `%c'"),
				       *pp_string);
			}
		} while (*pp_string++);

#ifndef GNU_NROFF
		/* tbl needs col */
		if (using_tbl && !troff && *COL)
			command = strappend (command, " | ", COL, NULL);
#endif /* GNU_NROFF */

		if (roff_device || gxditview)
			free (dev);
	} else {
		/* use external formatter script, it takes arguments
		   input file, preprocessor string, and (optional)
		   output device */
		char *esc_file = escape_shell (file);
		char *esc_pp   = escape_shell (pp_string);
		command = strappend (fmt_prog, " ", esc_file, " ", esc_pp, " ",
				     roff_device ? roff_device : "", NULL);
		free (esc_pp);
		free (esc_file);
	}

	return command;
}

/* Return command (statically allocated string) to run a browser on a
 * given file, observing http://www.tuxedo.org/~esr/BROWSER/.
 *
 * (Actually, I really implement
 * http://www.dwheeler.com/browse/secure_browser.html, but it's
 * backward-compatible.)
 */
static char *make_browser (const char *command, const char *file)
{
	static char *browser;
	static int browser_len = 0;
	int command_len = strlen (command) * 2 + strlen (file) + 1;
	int found_percent_s = 0;
	char *percent;
	char *esc_file;

	if (command_len > browser_len) {
		browser_len = command_len;
		browser = xrealloc (browser, browser_len + 1);
	}
	*browser = '\0';

	percent = strchr (command, '%');
	while (percent) {
		strncat (browser, command, percent - command);
		switch (*(percent + 1)) {
			case '\0':
			case '%':
				strcat (browser, "%");
				break;
			case 'c':
				strcat (browser, ":");
				break;
			case 's':
				esc_file = escape_shell (file);
				strcat (browser, esc_file);
				free (esc_file);
				found_percent_s = 1;
				break;
			default:
				strncat (browser, percent, 2);
				break;
		}
		if (*(percent + 1))
			command = percent + 2;
		else
			command = percent + 1;
		percent = strchr (command, '%');
	}
	strcat (browser, command);
	if (!found_percent_s) {
		strcat (browser, " ");
		esc_file = escape_shell (file);
		strcat (browser, esc_file);
		free (esc_file);
	}

	return browser;
}

static void setenv_less (const char *title)
{
	const char *esc_title;
	char *less_opts, *man_pn;
	const char *force = getenv ("MANLESS");

	if (force) {
		setenv ("LESS", force, 1);
		return;
	}

	esc_title = escape_less (title);
	less_opts = xmalloc (strlen (LESS_OPTS) +
			     strlen (prompt_string) * 2 + 1);
	sprintf (less_opts, LESS_OPTS, prompt_string, prompt_string);
	less_opts = strappend (less_opts, less, NULL);
	man_pn = strstr (less_opts, MAN_PN);
	while (man_pn) {
		char *subst_opts =
			xmalloc (strlen (less_opts) - strlen (MAN_PN) +
				 strlen (esc_title) + 1);
		strncpy (subst_opts, less_opts, man_pn - less_opts);
		subst_opts[man_pn - less_opts] = '\0';
		strcat (subst_opts, esc_title);
		strcat (subst_opts, man_pn + strlen (MAN_PN));
		free (less_opts);
		less_opts = subst_opts;
		man_pn = strstr (less_opts, MAN_PN);
	}

	if (debug)
		fprintf (stderr, "Setting LESS to %s\n", less_opts);
	/* If there isn't enough space in the environment, ignore it. */
	setenv ("LESS", less_opts, 1);

	if (debug)
		fprintf (stderr, "Setting MAN_PN to %s\n", esc_title);
	setenv ("MAN_PN", esc_title, 1);

	free (less_opts);
}

/* Return command (malloced string) to display file, NULL means stdin */
static char *make_display_command (const char *file, const char *title)
{
	char *command;
	char *esc_file = escape_shell (file);

	setenv_less (title);

	if (file) {
		if (ascii)
			command = strappend (NULL, get_def ("cat", CAT), 
					     " ", esc_file, " |",
					     get_def ("tr", TR TR_SET1 TR_SET2),
					     " |", pager, NULL);
#ifdef TROFF_IS_GROFF
		else if (htmlout)
			/* The filename needs to be substituted in later. */
			command = xstrdup (html_pager);
#endif
		else
			command = strappend (NULL, pager, " ", esc_file, NULL);
	} else {
		if (ascii)
			command = strappend (NULL,
					     get_def ("tr", TR TR_SET1 TR_SET2),
					     " |", pager, NULL);
#ifdef TROFF_IS_GROFF
		else if (htmlout)
			command = xstrdup (html_pager);
#endif
		else
			command = xstrdup (pager);
	}

	free (esc_file);

	return command;
}


/* return a (malloced) temporary name in cat_file's directory */
static char *tmp_cat_filename (const char *cat_file)
{
	char *name = xmalloc (strlen (cat_file) + 12);
	char *base;
	strcpy (name, cat_file);
	base = basename (name);
	sprintf (base, "%d", getpid ());
	return name;
}


/* If delete unlink tmp_cat, else commit tmp_cat to cat_file.
   Return non-zero on error.
 */
static int commit_tmp_cat (const char *cat_file, const char *tmp_cat,
			   int delete)
{
	int status = 0;

	if (!delete && global_manpath && euid == 0) {
		if (debug) {
			fprintf (stderr, "fixing temporary cat's ownership\n");
			status = 0;
		} else {
			struct passwd *man_owner = get_man_owner ();
			status = chown (tmp_cat, man_owner->pw_uid, -1);
			if (status)
				error (0, errno, _("can't chown %s"), tmp_cat);
		}
	}

	if (!delete && !status) {
		if (debug) {
			fprintf (stderr, "fixing temporary cat's mode\n");
			status = 0;
		} else {
			status = chmod (tmp_cat, CATMODE);
			if (status)
				error (0, errno, _("can't chmod %s"), tmp_cat);
		}
	}

	if (!delete && !status) {
		if (debug) {
			fprintf (stderr, "renaming temporary cat to %s\n",
				 cat_file);
			status = 0;
		} else {
			status = rename (tmp_cat, cat_file);
			if (status)
				error (0, errno, _("can't rename %s to %s"),
				       tmp_cat, cat_file);
		}
	}

	if (!delete && !status) {
		if (debug) {
			fprintf (stderr, "setting modtime on cat file %s\n",
				 cat_file);
			status = 0;
		} else {
			time_t now = time (NULL);
			struct utimbuf utb;
			utb.actime = now;
			if (man_modtime)
				utb.modtime = man_modtime;
			else
				utb.modtime = 0;
			status = utime (cat_file, &utb);
			if (status)
				error (0, errno, _("can't set times on %s"),
				       cat_file);
		}
	}

	if (delete || status) {
		if (debug)
			fprintf (stderr, "unlinking temporary cat\n");
		else if (unlink (tmp_cat))
			error (0, errno, _("can't unlink %s"), tmp_cat);
	}

	return status;
}

#ifdef MAN_CATS

/* Return stream to write formatted manual page to for saving as cat file */
static __inline__ FILE *open_cat_stream (const char *cat_file)
{
	FILE *save;
#  ifdef COMP_CAT
	pid_t child;
	int pipe_fd[2];
	int cat_fd;
#  endif

	tmp_cat_file = tmp_cat_filename (cat_file);
	if (!debug)
		push_cleanup ((void (*)()) unlink, tmp_cat_file);
	created_tmp_cat = 0;

#  ifdef COMP_CAT
	/* write to a pipe that compresses into tmp_cat_file */

	/* create pipe */
	if (pipe (pipe_fd)) {
		error (0, errno, _("can't create pipe"));
		return NULL;
	}

	/* Create the cat file out here so that we can deal with errors
	 * properly.
	 */
	if (debug) {
		fprintf (stderr, "compressing to temporary cat %s\n",
			 tmp_cat_file);
		strcpy (tmp_cat_file, "/dev/null");
	}

	cat_fd = open (tmp_cat_file, O_WRONLY | O_CREAT | O_TRUNC, CATMODE);
	if (cat_fd != -1)
		created_tmp_cat = 1;
	else {
		if (!debug && (errno == EACCES || errno == EROFS)) {
			/* No permission to write to the cat file. Oh well,
			 * return NULL and let the caller sort it out.
			 */
			if (debug)
				fprintf (stderr, "can't write to %s\n",
					 tmp_cat_file);
			return NULL;
		} else
			error (FATAL, errno, _("can't create %s"),
			       tmp_cat_file);
	}

	/* fork the compressor */
	fflush (NULL);
	child = fork ();
	if (child < 0) {
		error (0, errno, _("can't fork"));
		return NULL;
	} else if (child == 0) {
		/* compress reading end of pipe to tmp_cat_file */
		char *const envp[] = { NULL };

		pop_all_cleanups ();

		/* connect standard input to reading end of pipe */
		if (dup2 (pipe_fd[0], 0) == -1)
			error (0, errno, _("can't dup2"));
		close (pipe_fd[0]);
		close (pipe_fd[1]);

		/* set up standard output to write to tmp_cat_file */
		if (dup2 (cat_fd, 1) == -1)
			error (0, errno, _("can't dup2"));
		close (cat_fd);

		nice (10);

		/* compress standard input to standard output */
		/* @@@ disgusting: have to split COMPRESSOR into argv */
		{
			char *cmd;
			char *path;
			char *argv[8];
			int n;
			cmd = xstrdup (get_def ("compressor", COMPRESSOR));
			path = strtok (cmd, " \t\n");
			argv[0] = basename (path);
			for (n = 1; argv[n-1]; ++n) {
				argv[n] = strtok (NULL, " \t\n");
			}
			execve (path, argv, envp);
			error (FATAL, errno, _("can't exec %s"), path);
		}
	}

	/* connect save to the writing end of pipe */
	close (pipe_fd[0]);
	save = fdopen (pipe_fd[1], "w");
	close (cat_fd);

	cat_comp_pid = child;
#  else
	/* write directly to tmp_cat_file */
	if (debug) {
		fprintf (stderr, "saving to temporary cat %s\n", tmp_cat_file);
		tmp_cat_file = "/dev/null";
	} else
		push_cleanup ((void (*)()) unlink, tmp_cat_file);
	save = fopen (tmp_cat_file, "w");
#  endif

	return save;
}

/* Close the cat page stream, return non-zero on error.
   If delete don't update the cat file.
 */
static __inline__ int close_cat_stream (FILE *cat_stream, const char *cat_file,
					int delete)
{
	int status = fclose (cat_stream);

#  ifdef COMP_CAT
	int comp_status;	/* get compressor's exit status */

	while (waitpid (cat_comp_pid, &comp_status, 0) == -1) {
		if (errno != EINTR) {
			error (0, errno, _("waiting for pid %u"),
			       cat_comp_pid);
			comp_status = -1;
			break;
		}
	}

	if (debug)
		fprintf (stderr, "compressor exited with status %d\n",
			 comp_status);
	status |= comp_status;
#  endif

	if (created_tmp_cat) {
		status |= commit_tmp_cat (cat_file, tmp_cat_file,
					  delete || status);
		pop_cleanup ();
	}
	free (tmp_cat_file);
	return status;
}

/*
 * format a manual page with format_cmd, display it with disp_cmd, and
 * save it to cat_file
 */
static int format_display_and_save (const char *format_cmd,
				    const char *disp_cmd,
				    const char *cat_file)
{
	FILE *in  = checked_popen (format_cmd, "r");
	FILE *out = checked_popen (disp_cmd, "w");
	FILE *sav = open_cat_stream (cat_file);
	int instat = 1, outstat;
	RETSIGTYPE (*old_handler)() = signal (SIGPIPE, SIG_IGN);

	if (in && out) {
		/* copy in to both out and sav */
		char buf[PIPE_BUF];
		int inned;	/* #bytes in buf */
		int outing = 1;
		int saving = sav != NULL;

		while ((outing || saving) &&
		       (inned = fread (buf, 1, PIPE_BUF, in))) {
			int outed = 0; /* #bytes already written to out */
			int saved = 0; /* dto. to sav  */

			/* write buf to out and sav, cope with short writes */
			do {
				int n;

				if (outing && (outed < inned)) {
					n = fwrite (buf+outed, 1, inned-outed, 
						    out);
					outed += n;
					if (!n && saving)
						fprintf (stderr,
							 _("Still saving the page, please wait...\n"));
					outing = n; /* stop outing on error */
				}

				if (saving && (saved < inned)) {
					n = fwrite (buf+saved, 1, inned-saved, 
						    sav);
					saved += n;
					saving = n; /* stop saving on error */
			        }

			} while ((outing && outed < inned)
			      || (saving && saved < inned));
		}
	}

	if (in)  instat  = pclose (in);
	if (sav) close_cat_stream (sav, cat_file, instat);
	if (out) outstat = pclose (out);
	signal (SIGPIPE, old_handler);
	return instat;
}
#endif /* MAN_CATS */

/* Format a manual page with format_cmd and display it with disp_cmd.
 * Handle temporary file creation if necessary.
 */
static void format_display (const char *format_cmd, const char *disp_cmd,
			    const char *man_file)
{
	char *command;
	int status;

	drop_effective_privs ();

#ifdef TROFF_IS_GROFF
	if (format_cmd && htmlout) {
		char old_cwd[PATH_MAX];
		char *htmldir;
		char *man_file_copy, *man_base, *man_ext;
		char *htmlfile, *esc_htmlfile;
		char *browser_list, *candidate;

		if (!getcwd (old_cwd, PATH_MAX - 1))
			old_cwd[0] = '\0';
		htmldir = create_tempdir ("hman");
		if (chdir (htmldir) == -1)
			error (FATAL, errno, _("can't change to directory %s"),
			       htmldir);
		man_file_copy = xstrdup (man_file);
		man_base = basename (man_file_copy);
		man_ext = strchr (man_base, '.');
		if (man_ext)
			*man_ext = '\0';
		htmlfile = xstrdup (htmldir);
		htmlfile = strappend (htmlfile, "/", man_base, ".html", NULL);
		free (man_file_copy);
		esc_htmlfile = escape_shell (htmlfile);
		command = strappend (NULL, format_cmd, " > ", esc_htmlfile,
				     NULL);

		status = do_system_drop_privs (command);
		/* Some shells report broken pipe; ignore it. */
		if (status && status != (SIGPIPE + 0x80) * 256) {
			if (chdir (old_cwd) == -1) {
				error (0, errno,
				       _("can't change to directory %s"),
				       old_cwd);
				chdir ("/");
			}
			if (remove_directory (htmldir) == -1)
				error (0, errno,
				       _("can't remove directory %s"),
				       htmldir);
			free (htmlfile);
			free (htmldir);
			gripe_system (command, status);
		}
		free (command);

		browser_list = xstrdup (disp_cmd);
		for (candidate = strtok (browser_list, ":"); candidate;
		     candidate = strtok (NULL, ":")) {
			if (debug)
				fprintf (stderr, "Trying browser: %s\n",
					 candidate);
			command = make_browser (candidate, htmlfile);
			status = do_system_drop_privs (command);
			if (!status || status == (SIGPIPE + 0x80) * 256)
				break;
		}
		if (!candidate)
			error (CHILD_FAIL, 0,
			       "couldn't execute any browser from %s",
			       disp_cmd);
		free (browser_list);
		if (chdir (old_cwd) == -1) {
			error (0, errno, _("can't change to directory %s"),
			       old_cwd);
			chdir ("/");
		}
		if (remove_directory (htmldir) == -1)
			error (0, errno, _("can't remove directory %s"),
			       htmldir);
		free (htmlfile);
		free (htmldir);
	} else
#endif /* TROFF_IS_GROFF */
	    if (format_cmd) {
		command = strappend (NULL, format_cmd, " | ", disp_cmd, NULL);
		status = do_system_drop_privs (command);
		if (status && status != (SIGPIPE + 0x80) * 256)
			gripe_system (command, status);
		free (command);
	} else {
		const char *given_command = disp_cmd;
		status = do_system_drop_privs (given_command);
		if (status && status != (SIGPIPE + 0x80) * 256)
			gripe_system (given_command, status);
	}

	regain_effective_privs ();
}

/* "Display" a page in catman mode, which amounts to saving it. */
static void display_catman (const char *cat_file, const char *format_cmd)
{
	char *tmpcat = tmp_cat_filename (cat_file);
	char *esc_tmpcat = escape_shell (tmpcat);
	char *cmd;
	int status;

#ifdef COMP_CAT
	cmd = strappend (NULL, format_cmd, " | ",
			 get_def ("compressor", COMPRESSOR),
			 " > ", esc_tmpcat, NULL);
#else /* !COMP_CAT */
	cmd = strappend (NULL, format_cmd, " > ", esc_tmpcat, NULL);
#endif /* COMP_CAT */
	/* save the cat as real user
	 * (1) required for user man hierarchy
	 * (2) else depending on ruid's privs is ok, effectively disables
	 *     catman for non-root.
	 */
	push_cleanup ((void (*)()) unlink, tmpcat);
	status = do_system_drop_privs (cmd);
	if (status)
		gripe_system (cmd, status);
	free (cmd);

	commit_tmp_cat (cat_file, tmpcat, status);
	pop_cleanup();
	free (esc_tmpcat);
	free (tmpcat);
}

/*
 * optionally chdir to dir, if necessary update cat_file from man_file
 * and display it.  if man_file is NULL cat_file is a stray cat.  If
 * !save_cat or cat_file is NULL we must not save the formatted cat.
 * If man_file is "" this is a special case -- we expect the man page
 * on standard input.
 */
static int display (const char *dir, const char *man_file,
		    const char *cat_file, const char *title,
		    const char *dbfilters)
{
	int found;
	static int pause;
	char *format_cmd;	/* command to format man_file to stdout */
	int display_to_stdout;

	/* if dir is set chdir to it */
	if (dir) {
		if (debug)
			fprintf (stderr, "chdir %s\n", dir);

		if (chdir (dir)) {
			error (0, errno, _("can't chdir to %s"), dir);
			return 0;
		}
	}

	/* define format_cmd */
	{
		const char *source_file = NULL;
#ifdef COMP_SRC
		if (man_file) {
			source_file = get_ztemp ();
			if (!source_file)
				source_file = man_file;
		}
#else
		source_file = man_file;
#endif /* COMP_SRC */

		if (source_file)
			format_cmd = make_roff_command (dir, source_file,
							dbfilters);
		else
			format_cmd = NULL;
	}

	/* Get modification time, for commit_tmp_cat(). */
	if (man_file && *man_file) {
		struct stat stb;
		if (stat (man_file, &stb))
			man_modtime = 0;
		else
			man_modtime = stb.st_mtime;
	}

	display_to_stdout = troff;
#ifdef TROFF_IS_GROFF
	if (htmlout)
		display_to_stdout = 0;
#endif

	if (display_to_stdout) {
		/* If we're reading stdin via '-l -', man_file is "". See
		 * below.
		 */
		if (*man_file == '\0')
			found = 1;
		else
			found = !access (man_file, R_OK);
		if (found) {
			if (pause && do_prompt (title))
				return 0;
			checked_system (format_cmd);
		}
	} else {
		int format;
		int status;
		char *catpath;

		/* cat_file is the alternate cat_file.. */
		/* If !man_file, we have a straycat
		   If !cat_file, we can't produce a cat_file, but we
		   may still have one tucked away under /usr... 
		   Check there first ala the FSSTND, and display if newer
		   than man_file, if older, ignore it altogether */

		if (different_encoding
#ifdef TROFF_IS_GROFF
		    || htmlout
#endif
			    ) {
			format = 1;
			save_cat = 0;
		} else if (!local_man_file) {
			catpath = get_catpath
				(dir, global_manpath ? SYSTEM_CAT : USER_CAT);

			assert (dir);

			if (man_file && catpath) {
				/* we may have a FSSTND cat != cat_file */
				char *std_cat_file =
					convert_name (man_file, NULL);
				status = is_changed (man_file, std_cat_file);
	
				if (status != -2 && !(status & 1) == 1) {
					cat_file = std_cat_file;
					save_cat = format = 0;
				} else
					format = 1;
				/* @@@ memory leak of std_cat_file */
			} else
				format = 1;

			if (catpath)
				free (catpath);
		} else
			format = 1;

		if (!man_file) {
			assert (cat_file);
			format = 0;
		} else if (!cat_file) {
			assert (man_file);
			save_cat = 0;
			format = 1;
		} else if (format && save_cat) {
			char *cat_dir;
			char *tmp;

			status = is_changed (man_file, cat_file);
			format = (status == -2) || ((status & 1) == 1);

			/* don't save if we haven't a cat directory */
			cat_dir = xstrdup (cat_file);
			tmp = strrchr (cat_dir, '/');
			if (tmp)
				*tmp = 0;
			save_cat = is_directory (cat_dir) == 1;
			if (debug && !save_cat)
				fprintf (stderr, "cat dir %s does not exist\n",
					 cat_dir);
			free (cat_dir);
		}

		/* if we're trying to read stdin via '-l -' then man_file
		 * will be "" which access() obviously barfs on, but all is
		 * well because the format_cmd will have been created to
		 * expect input via stdin. So we special-case this to avoid
		 * the bogus access() check.
		*/
		if (format == 1 && *man_file == '\0')
			found = 1;
		else
			found = !access (format ? man_file : cat_file, R_OK);

		if (debug)
			fprintf (stderr,
				 "format: %d, save_cat: %d, found: %d\n",
				 format, save_cat, found);

		if (!found) {
			if (format_cmd)
				free (format_cmd);
			return found;
		}

		if (print_where || print_where_cat) {
			int printed = 0;
			if (print_where && man_file) {
				printf ("%s", man_file);
				printed = 1;
			}
			if (print_where_cat && cat_file && !format) {
				if (printed)
					putchar (' ');
				printf ("%s", cat_file);
			}
			putchar ('\n');
		} else if (catman) {
			if (format) {
				if (!save_cat)
					error (0, 0,
					       _("\ncannot write to "
						 "%s in catman mode"),
					       cat_file);
				else
					display_catman (cat_file, format_cmd);
			}
		} else if (format) {
			/* no cat or out of date */
			char *disp_cmd;

			if (pause && do_prompt (title)) {
				if (format_cmd)
					free (format_cmd);
				if (local_man_file)
					return 1;
				else
					return 0;
			}

			fprintf (stderr,
				 _("Reformatting %s, please wait...\n"),
				 title);
			disp_cmd = make_display_command (NULL, title);

#ifdef MAN_CATS
			if (save_cat) {
				/* save cat */
				format_display_and_save (format_cmd,
							 disp_cmd,
							 cat_file);
			} else 
#endif /* MAN_CATS */
				/* don't save cat */
				format_display (format_cmd, disp_cmd,
						man_file);

			free (disp_cmd);

		} else {
			/* display preformatted cat */
			char *disp_cmd;
			char *esc_cat_file;
#ifdef COMP_SRC
			struct compression *comp;
#endif /* COMP_SRC */

			if (format_cmd) {
				free (format_cmd);
				format_cmd = NULL;
			}

#if defined(COMP_SRC)
			if (pause && do_prompt (title))
				return 0;

			comp = comp_info (cat_file);
			if (comp) {
				esc_cat_file = escape_shell (cat_file);
				format_cmd = strappend (NULL, comp->prog, " ",
							esc_cat_file, NULL);
				free (esc_cat_file);
				disp_cmd = make_display_command (NULL, title);
			} else
				disp_cmd = make_display_command (cat_file,
								 title);
#elif defined(COMP_CAT)
			if (pause && do_prompt (title))
				return 0;

			esc_cat_file = escape_shell (cat_file);
			format_cmd = strappend (NULL,
						get_def ("decompressor",
							 DECOMPRESSOR),
						" ", esc_cat_file, NULL);
			free (esc_cat_file);
			disp_cmd = make_display_command (NULL, title);
#else /* !(COMP_SRC || COMP_CAT) */
			if (pause && do_prompt (title))
				return 0;

			disp_cmd = make_display_command (cat_file, title);
#endif /* COMP_SRC */
			format_display (format_cmd, disp_cmd, man_file);
			if (format_cmd) {
				free (format_cmd);
				format_cmd = NULL;
			}
			if (disp_cmd)
				free (disp_cmd);
		}
	}

	if (format_cmd)
		free (format_cmd);
		
	if (!pause)
		pause = found;

	return found;
}


static char *find_cat_file (const char *path, const char *man_file)
{
	char *cat_file, *cat_path;

	/* could do this with `global' */

	global_manpath = is_global_mandir (path);
	cat_path = get_catpath
		(man_file, global_manpath ? SYSTEM_CAT : USER_CAT);
	cat_file = convert_name (man_file, cat_path);

	if (cat_path)
		free (cat_path);

	return cat_file;
}

static int compare_candidates (const struct mandata *left,
			       const struct mandata *right,
			       const char *req_name)
{
	char **sp;
	int sec_left = 0, sec_right = 0, cmp;

	/* If one candidate matches the requested name exactly, sort it
	 * first. This makes --ignore-case behave more sensibly.
	 */
	/* name is never NULL here, see add_candidate() */
	if (STREQ (left->name, req_name)) {
		if (!STREQ (right->name, req_name))
			return -1;
	} else {
		if (STREQ (right->name, req_name))
			return 1;
	}

	cmp = strcmp (left->ext, right->ext);
	if (cmp) {
		/* Find out whether left->ext is ahead of right->ext in
		 * section_list.
		 */
		for (sp = section_list; *sp; sp++) {
			if (!*(*sp + 1)) {
				/* No extension */
				if (!sec_left  && **sp == *(left->ext))
					sec_left  = sp - section_list + 1;
				if (!sec_right && **sp == *(right->ext))
					sec_right = sp - section_list + 1;
			} else if (!sec_left  && STREQ (*sp, left->ext)) {
				sec_left  = sp - section_list + 1;
			} else if (!sec_right && STREQ (*sp, right->ext)) {
				sec_right = sp - section_list + 1;
			}
		}
		if (sec_left != sec_right)
			return sec_left - sec_right;
		else
			return cmp;
	}

	/* ULT_MAN comes first, etc. */
	cmp = compare_ids (left->id, right->id);
	if (cmp)
		return cmp;

	/* add_candidate() will keep equal candidates in order of insertion
	 * so that manpath ordering (e.g. language-specific hierarchies)
	 * works.
	 */
	return 0;
}

/* Add an entry to the list of candidates. */
static int add_candidate (struct candidate **head, char from_db, char cat,
			  const char *req_name, const char *path,
			  struct mandata *source)
{
	struct candidate *search, *insert, *candp;
	int insert_found = 0;

	if (debug)
		fprintf (stderr, "candidate: %d %d %s %s %c %s %s %s\n",
				 from_db, cat, req_name, path,
				 source->id, source->name ? source->name : "-",
				 source->sec, source->ext);

	if (!source->name)
		source->name = xstrdup (req_name);

	/* insert will be NULL (insert at start) or a pointer to the element
	 * after which this element should be inserted.
	 */
	insert = NULL;
	if (*head && compare_candidates (source, (*head)->source,
					 req_name) < 0)
		insert_found = 1;
	search = *head;
	while (search) {
		/* Check for duplicates. */
		if (STREQ (path, search->path) &&
		    STREQ (source->name, search->source->name) &&
		    STREQ (source->sec, search->source->sec) &&
		    STREQ (source->ext, search->source->ext)) {
			if (debug)
				fprintf (stderr, "duplicate candidate\n");
			return 0;
		}
		if (!insert_found &&
		    (!search->next ||
		     compare_candidates (source, search->next->source,
					 req_name) < 0)) {
			insert = search;
			insert_found = 1;
		}

		if (search->next)
			search = search->next;
		else
			break;
	}

	candp = (struct candidate *) malloc (sizeof (struct candidate));
	candp->req_name = req_name;
	candp->from_db = from_db;
	candp->cat = cat;
	candp->path = path;
	candp->source = source;
	candp->next = insert ? insert->next : *head;
	if (insert)
		insert->next = candp;
	else
		*head = candp;

	return 1;
}

/*
 * See if the preformatted man page or the source exists in the given
 * section.
 */
static int try_section (const char *path, const char *sec, const char *name,
			struct candidate **cand_head)
{
	int found = 0;
	char **names = NULL, **np;
	char cat = 0;

	if (debug)
		fprintf (stderr, "trying section %s with globbing\n", sec);

#ifndef NROFF_MISSING /* #ifdef NROFF */
	/*
  	 * Look for man page source files.
  	 */

	names = look_for_file (path, sec, name, 0, match_case);
	if (!names)
		/*
    		 * No files match.  
    		 * See if there's a preformatted page around that
    		 * we can display.
    		 */
#endif /* NROFF_MISSING */
	{
		if (catman)
			return 1;

		if (!troff && !different_encoding) {
			names = look_for_file (path, sec, name, 1, match_case);
			cat = 1;
		}
	}

	for (np = names; np && *np; np++) {
		struct mandata *info =
			(struct mandata *) malloc (sizeof (struct mandata));
		char *info_buffer = filename_info (*np, info, name);
		char *ult;
		if (!info_buffer)
			continue;
		info->addr = info_buffer;

		/* What kind of page is this? Since it's a real file, it
		 * must be either ULT_MAN or SO_MAN. ult_src() can tell us
		 * which.
		 */
		ult = ult_src (*np, path, NULL,
			       SO_LINK | SOFT_LINK | HARD_LINK);
		if (!ult) {
			/* already warned */
			if (debug)
				fprintf (stderr,
					 "try_section(): bad link %s\n", *np);
			continue;
		}
		if (STREQ (ult, *np))
			info->id = ULT_MAN;
		else
			info->id = SO_MAN;

		found += add_candidate (cand_head, CANDIDATE_FILESYSTEM,
					cat, name, path, info);
		/* Don't free info and info_buffer here. */
	}

	return found;
}

static int display_filesystem (struct candidate *candp)
{
	char *filename = make_filename (candp->path, NULL, candp->source,
					candp->cat ? "cat" : "man");
	/* source->name is never NULL thanks to add_candidate() */
	char *title = strappend (NULL, candp->source->name,
				 "(", candp->source->ext, ")", NULL);
	if (candp->cat) {
		if (troff || different_encoding)
			return 0;
		return display (candp->path, NULL, filename, title, NULL);
	} else {
		char *man_file, *cat_file;
		int found;

		man_file = ult_src (filename, candp->path, NULL,
				    SO_LINK | SOFT_LINK | HARD_LINK);
		if (man_file == NULL) {
			free (title);
			return 0;
		}

		if (debug)
			fprintf (stderr, "found ultimate source file %s\n",
				 man_file);
		lang = lang_dir (man_file);

		cat_file = find_cat_file (candp->path, man_file);
		if (debug)
			fprintf (stderr, "will try cat file %s\n", cat_file);
		found = display (candp->path, man_file, cat_file, title, NULL);
		free (cat_file);
		/* Be careful not to free man_file, as it's static. */
		free (title);

#ifdef COMP_SRC
		/* If ult_src() produced a ztemp file, we need to remove it
		 * before proceeding.
		 */
		remove_ztemp ();
#endif /* COMP_SRC */

		return found;
	}
}

#ifdef MAN_DB_UPDATES
/* wrapper to dbdelete which deals with opening/closing the db */
static void dbdelete_wrapper (const char *page, struct mandata *info)
{
	if (!catman) {
		dbf = MYDBM_RWOPEN (database);
		if (dbf) {
			if (dbdelete (page, info) == 1)
				if (debug)
					fprintf (stderr, "%s(%s) not in db!\n",
						 page, info->ext);
			MYDBM_CLOSE(dbf);
		}
	}
}
#endif /* MAN_DB_UPDATES */

/* This started out life as try_section, but a lot of that routine is 
   redundant wrt the db cache. */
static int display_database (struct candidate *candp)
{
	int found = 0;
	char *file;
	const char *name;
	char *title;
	struct mandata *in = candp->source;
#ifdef MAN_DB_UPDATES
	struct stat buf;
#endif

	if (debug) {
		fprintf (stderr, "trying a db located file.\n");
		dbprintf (in);
	}

	/* if the pointer holds some data, this is a reference to the 
	   real page, use that instead. */
	if (*in->pointer != '-')
		name = in->pointer;
	else if (in->name)
		name = in->name;
	else
		name = candp->req_name;

	/* make sure the file we want is the same as the one in the 
	   filesystem */

#ifdef MAN_DB_UPDATES
	/* The next piece of code examines the db found manual page
	   and checks for consistency */
	file = make_filename (candp->path, name, in, "man");
	if (lstat (file, &buf) == 0 && buf.st_mtime != in->_st_mtime) {
		/* update of this file required */
		if (debug)
			fprintf (stderr, "%s needs to be recached: %ld %ld.\n", 
				 file, (long)in->_st_mtime,
				 (long)buf.st_mtime);
		dbf = MYDBM_RWOPEN (database);
		if (dbf) {
			dbdelete (candp->req_name, in);
			test_manfile (file, candp->path);
			in = dblookup_exact (candp->req_name, in->ext,
					     match_case);
			MYDBM_CLOSE (dbf);
			if (!in)
				return 0;
			if (*in->pointer != '-')
				name = in->pointer;
			else if (in->name)
				name = in->name;
			else
				name = candp->req_name;
		} else {
			if (errno == EACCES || errno == EROFS) {
				if (debug)
					fprintf (stderr,
						 "database %s is read-only\n",
						 database);
			} else
				error (0, errno,
				       _("can't update index cache %s"),
				       database);
		}
	}
#endif /* MAN_DB_UPDATES */

	if (debug && (in->id == WHATIS_MAN || in->id == WHATIS_CAT))
		fprintf (stderr,
			 _("%s: relying on whatis refs is deprecated\n"),
			 name);

	title = strappend (NULL, name, "(", in->ext, ")", NULL);

#ifndef NROFF_MISSING /* #ifdef NROFF */
	/*
  	 * Look for man page source files.
  	 */

	if (in->id < STRAY_CAT) {	/* There should be a src page */
		file = make_filename (candp->path, name, in, "man");
		if (debug)
			fprintf (stderr, "Checking physical location: %s\n",
				 file);

		if (access (file, R_OK) == 0) {
			char *man_file;
			char *cat_file;

			man_file = ult_src (file, candp->path, NULL,
					    SO_LINK | SOFT_LINK | HARD_LINK);
			if (man_file == NULL) {
				free (title);
				return found; /* zero */
			}

			if (debug)
				fprintf (stderr,
					 "found ultimate source file %s\n",
					 man_file);
			lang = lang_dir (man_file);

			cat_file = find_cat_file (candp->path, man_file);
			found += display (candp->path, man_file, cat_file,
					  title, in->filter);
			free (cat_file);
#ifdef COMP_SRC
			/* if ult_src() produced a ztemp file, we need to 
			   remove it (and unexist it) before proceeding */
			remove_ztemp ();
#endif /* COMP_SRC */

		} /* else {drop through to the bottom and return 0 anyway} */
	} else 

#endif /* NROFF_MISSING */

	if (in->id <= WHATIS_CAT) {
		/* The db says we have a stray cat or whatis ref */

		if (catman) {
			free (title);
			return ++found;
		}

		/* show this page but force an update later to make sure
		   we haven't just added the new page */
		found_a_stray = 1;

		/* If explicitly asked for troff or a different encoding,
		 * don't show a stray cat.
		 */
		if (troff || different_encoding) {
			free (title);
			return found;
		}

		file = make_filename (candp->path, name, in, "cat");
		if (debug)
			fprintf (stderr,
				 "Checking physical location: %s\n",
				 file);

		if (access (file, R_OK) != 0) {
			char *catpath;
			catpath = get_catpath (candp->path,
					       global_manpath ? SYSTEM_CAT
							      : USER_CAT);

			if (catpath && strcmp (catpath, candp->path) != 0) {
				file = make_filename (catpath, name,
						      in, "cat");
				free (catpath);
				if (debug)
					fprintf (stderr,
						 "Checking physical "
						 "location: %s\n",
						 file);

				if (access (file, R_OK) != 0) {
					/* don't delete here, 
					   return==0 will do that */
					free (title);
					return found; /* zero */
				}
			} else {
				if (catpath)
					free (catpath);
				free (title);
				return found; /* zero */
			}
		}

		found += display (candp->path, NULL, file, title, in->filter);
	}
	free (title);
	return found;
}

/* test for existence, if fail: call dbdelete_wrapper, else return amount */
static int display_database_check (struct candidate *candp)
{
	int exists = display_database (candp);

#ifdef MAN_DB_UPDATES
	if (!exists && !skip) {
		if (debug)
			fprintf (stderr, "dbdelete_wrapper (%s, %p)\n",
				 candp->req_name, candp->source);
		dbdelete_wrapper (candp->req_name, candp->source);
	}
#endif /* MAN_DB_UPDATES */

	return exists;
}

static void db_hash_free (void *defn)
{
	free_mandata_struct (defn);
}

/* Look for a page in the database. If db not accessible, return -1,
   otherwise return number of pages found. */
static int try_db (const char *manpath, const char *sec, const char *name,
		   struct candidate **cand_head)
{
	struct nlist *in_cache;
	struct mandata *loc, *data;
	char *catpath;
	int found = 0;

	/* find out where our db for this manpath should be */

	catpath = get_catpath (manpath, global_manpath ? SYSTEM_CAT : USER_CAT);
	if (catpath) {
		database = mkdbname (catpath);
		free (catpath);
	} else
		database = mkdbname (manpath);

	if (!db_hash)
		db_hash = hash_create (&db_hash_free);

	/* Have we looked here already? */
	in_cache = hash_lookup (db_hash, manpath, strlen (manpath));
	
	if (!in_cache) {
		dbf = MYDBM_RDOPEN (database);
		if (dbf && dbver_rd (dbf)) {
			MYDBM_CLOSE (dbf);
			dbf = NULL;
		}
		if (dbf) {
			if (debug)
				fprintf (stderr,
					 "Succeeded in opening %s O_RDONLY\n",
					 database);

			/* if section is set, only return those that match,
			   otherwise NULL retrieves all available */
			data = dblookup_all (name, section, match_case);
			hash_install (db_hash, manpath, strlen (manpath),
				      data);
			MYDBM_CLOSE (dbf);
#ifdef MAN_DB_CREATES
		} else if (!global_manpath) {
			/* create one */
			if (debug)
				fprintf (stderr, 
					 "Failed to open %s O_RDONLY\n",
					 database);
			/* create_db should really return EOF on failure. */
			if (create_db (manpath) == 0) {
				data = infoalloc ();
				data->next = NULL;
				data->addr = NULL;
				hash_install (db_hash,
					      manpath, strlen (manpath),
					      data);
				return -1;
			}
			return -2;
#endif /* MAN_DB_CREATES */
		} else {
			if (debug)
				fprintf (stderr, 
					 "Failed to open %s O_RDONLY\n",
					 database);
			data = infoalloc ();
			data->next = (struct mandata *) NULL;
			data->addr = NULL;
			hash_install (db_hash, manpath, strlen (manpath),
				      data);
			return -1; /* indicate failure to open db */
		}
	} else
		data = in_cache->defn;

	/* if we already know that there is nothing here, get on with it */
	if (!data)
		return 0;

	/* We already tried (and failed) to open this db before */
	if (!data->addr)
		return -1;

	/* cycle through the mandata structures (there's usually only 
	   1 or 2) and see what we have w.r.t. the current section */
	for (loc = data; loc; loc = loc->next)
		if (STREQ (sec, loc->sec) &&
		    (!extension || STREQ (extension, loc->ext)
				|| STREQ (extension, loc->ext + strlen (sec))))
			found += add_candidate (cand_head, CANDIDATE_DATABASE,
						0, name, manpath, loc);

	return found;
}

/* Try to locate the page under the specified manpath, in the desired section,
 * with the supplied name. Glob if necessary. Initially search the filesystem;
 * if that fails, try finding it via a db cache access. */
static int locate_page (const char *manpath, const char *sec, const char *name,
			struct candidate **candidates)
{
	int found, db_ok;

	/* sort out whether we want to treat this hierarchy as 
	   global or user. Differences:

	   global: if setuid, use privs; don't create db.
	   user  : if setuid, drop privs; allow db creation. */

	global_manpath = is_global_mandir (manpath);
	if (!global_manpath)
		drop_effective_privs ();

	if (debug)
		fprintf (stderr, "searching in %s, section %s\n", 
			 manpath, sec);

	found = try_section (manpath, sec, name, candidates);

	if (!found || findall) {
		db_ok = try_db (manpath, sec, name, candidates);

#ifdef MAN_DB_CREATES
		if (db_ok == -2) /* we created a db in the last call */
			db_ok = try_db (manpath, sec, name, candidates);
#endif /* MAN_DB_CREATES */

		if (db_ok > 0)  /* we found/opened a db and found something */
			found += db_ok;
	}

	if (!global_manpath)
		regain_effective_privs ();

	return found;
}

static int display_pages (struct candidate *candidates)
{
	struct candidate *candp;
	int found = 0;

	for (candp = candidates; candp; candp = candp->next) {
		if (candp->from_db)
			found += display_database_check (candp);
		else
			found += display_filesystem (candp);
		if (found && !findall)
			return found;
	}

	return found;
}

/*
 * Search for manual pages.
 *
 * If preformatted manual pages are supported, look for the formatted
 * file first, then the man page source file.  If they both exist and
 * the man page source file is newer, or only the source file exists,
 * try to reformat it and write the results in the cat directory.  If
 * it is not possible to write the cat file, simply format and display
 * the man file.
 *
 * If preformatted pages are not supported, or the troff option is
 * being used, only look for the man page source file.
 *
 */
static int man (const char *name)
{
	struct candidate *candidates = NULL;
	int found = 0;

	fflush (stdout);

	if (section) {
		char **mp;

		for (mp = manpathlist; *mp; mp++)
			found += locate_page (*mp, section, name, &candidates);
	} else {
		char **sp;

		for (sp = section_list; *sp; sp++) {
			char **mp;

			for (mp = manpathlist; *mp; mp++)
				found += locate_page (*mp, *sp, name,
						      &candidates);
		}
	}

	if (found)
		found = display_pages (candidates);

#ifdef MAN_DB_UPDATES
	/* check to see if any of the databases need updating */
	if ((!found || found_a_stray) && update && update_required) {
		if (need_to_rerun ())
			return man (name);
	}
#endif /* MAN_DB_UPDATES */

	return found;
}


static __inline__ char **get_section_list (void)
{
	int i = 0;
	char **config_sections;
	char **sections = NULL;
	char *sec;

	/* Section list from configuration file, or STD_SECTIONS if it's
	 * empty.
	 */
	config_sections = get_sections ();
	if (!*config_sections)
		config_sections = std_sections;

	if (colon_sep_section_list == NULL)
		colon_sep_section_list = getenv ("MANSECT");
	if (colon_sep_section_list == NULL || *colon_sep_section_list == '\0')
		return config_sections;

	for (sec = strtok (colon_sep_section_list, ":"); sec; 
	     sec = strtok (NULL, ":")) {
		sections = (char **) xrealloc (sections,
 					       (i + 2) * sizeof (char *));
 		sections[i++] = sec;
 	}

	if (i > 0) {
		sections[i] = NULL;
		return sections;
	} else {
		if (sections)
			free (sections);
		return config_sections;
	}
}

/* allow user to skip a page or quit after viewing desired page 
   return 1 to skip
   return 0 to view
 */
static __inline__ int do_prompt (const char *name)
{
	int ch;

	skip = 0;
	if (!isatty (STDOUT_FILENO) || !isatty (STDIN_FILENO))
		return 0;

	fprintf (stderr, _( 
		 "--Man-- next: %s "
		 "[ view (return) | skip (Ctrl-D) | quit (Ctrl-C) ]\n"), 
		 name);
	fflush (stderr);

	do {
		ch = getchar ();
		switch (ch) {
			case '\n':
				return 0;
			case EOF:
				skip = 1;
				return 1;
			default:
				break;
		}
	} while (1);

	return 0;
}

void int_handler (int signo)
{
	if (debug)
		fprintf (stderr, "\ninterrupt signal %d handler\n", signo);
	exit (INTERRUPTED);
}
