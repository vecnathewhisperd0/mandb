/*
 * wrapper.c - wrapper program around man and mandb
 *
 * Copyright (C) 2000 Fabrizio Polacco <fpolacco@debian.org>
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
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#ifdef HAVE_LIBGEN_H
#  include <libgen.h>
#endif /* HAVE_LIBGEN_H */

#include "lib/gettext.h"
#include <locale.h>
#define _(Text) gettext (Text)

#include "manconfig.h"


/* this list is used to authenticate the program running.
 * it is fixed at compile time to avoid a full class of 
 * dangers ...
 */
struct	{
	const char *prog;
	const char *run;
	const char *user;
} *wlp, wrapped_list[] =
{ /* prog	run				user	*/
#ifdef DEBUG
  { "_man",	"src/man",	"man"	},
  { "_mandb",	"src/mandb",	"man"	},
#endif
  { "man",	"/usr/lib/man-db/man",		"man"	},
  { "mandb",	"/usr/lib/man-db/mandb",	"man"	},
  { 0,		0,				0,	}};

char *program_name;

int main (int argc, char **argv)
{
	uid_t ruid;
	char *fakeroot;
	struct passwd *pwd;

	argc = argc; /* not used */

	/* We don't warn about this setlocale() call failing, as the program
	 * we call will do that.
	 */
	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);

	/* this wrapper can be run as "man" or as "mandb" */
	program_name = xstrdup (basename (argv[0]));

	ruid = getuid ();
	fakeroot = getenv ("FAKEROOTKEY");

#ifdef DEBUG
	printf ("%s:\n", program_name);
	printf ("real = %d, = %d, fakeroot = %d\n",
		(int) ruid, (fakeroot != 0));
#endif

	for (wlp = wrapped_list; wlp->prog && strcmp (program_name, wlp->prog);
	     ++wlp)
		;
	if (!wlp->prog) {
		fprintf (stderr, _("Don't know which program should I run being >%s<\n"),
			 program_name);
		return -ENOENT;
	}

	if (!fakeroot && ruid == 0) {
		pwd = getpwnam (wlp->user);
		/*
		if (!pwd
		 || setgid (pwd->pw_gid)
		 || initgroups (wlp->user, pwd->pw_gid)
		 || setuid (pwd->pw_uid)) {
			fprintf (stderr, _("%s: Failed su to user %s\n"),
				 wlp->prog, wlp->user);
			return -EACCES;
		}
		*/
		if (!pwd) {
			fprintf (stderr, _("%s: Failed su to user %s\n"),
				 wlp->prog, wlp->user);
			return -EACCES;
		}
		if (setgid (pwd->pw_gid)) {
			fprintf (stderr, _("%s: Failed su to user %s\n"),
				 wlp->prog, wlp->user);
			return -EACCES;
		}
		if (initgroups (wlp->user, pwd->pw_gid)) {
			fprintf (stderr, _("%s: Failed su to user %s\n"),
				 wlp->prog, wlp->user);
			return -EACCES;
		}
		if (setuid (pwd->pw_uid)) {
			fprintf (stderr, _("%s: Failed su to user %s\n"),
				 wlp->prog, wlp->user);
			return -EACCES;
		}
	}
	execv (wlp->run, argv);
	perror ("execv");
	return -errno;
}
