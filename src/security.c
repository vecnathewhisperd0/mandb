/*
 * security.c: routines to aid secure uid operations. 
 *  
 * Copyright (C), 1994, 1995, Graeme W. Wilford. (Wilf.)
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with this
 * distribution.
 *
 * Mon Aug  8 20:35:30 BST 1994  Wilf. (G.Wilford@ee.surrey.ac.uk) 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <errno.h>

#ifndef STDC_HEADERS
extern int errno;
#endif

#if defined(STDC_HEADERS)
#  include <string.h>
#  include <stdlib.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#endif /* STDC_HEADERS */

#include <sys/types.h>

#if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <libintl.h>
#define _(String) gettext (String)

#include "manconfig.h"
#include "lib/error.h"
#include "lib/cleanup.h"
#include "security.h"

#ifdef SECURE_MAN_UID
uid_t ruid;			/* initial real user id */
uid_t euid;			/* initial effective user id */
#endif /* SECURE_MAN_UID */

/* 
 * If you want to try to understand the following routines, go make 
 * a coffee, you'll need it. (a copy of your kernel sources may also 
 * be handy :-)
 */

#ifdef SECURE_MAN_UID

static __inline__ void gripe_set_euid()
{
	error (FATAL, errno, _( "can't set effective uid"));
}

#endif /* SECURE_MAN_UID */

/* 
 * function to gain user privs by either (a) dropping effective privs 
 * completely (saved ids) or (b) reversing euid w/ uid.
 * Ignore if superuser.
 */
void drop_effective_privs (void)
{
#ifdef SECURE_MAN_UID

	/* user is root or we've already dropped privs */
	if (ruid == 0 || geteuid() == ruid)
		return;
		
	if (debug)
		fputs("drop_effective_privs()\n", stderr);
		
#  if defined (POSIX_SAVED_IDS)
	if (seteuid (ruid))
		gripe_set_euid();
	
#  elif defined(BROKEN_LINUX_SAVED_IDS)
	if (setuid (ruid))
		gripe_set_euid();
	
#  elif defined(HAVE_SETREUID) /* fallback (could just be #else ) */
	if (setreuid (euid, ruid))
		gripe_set_euid();
	
#  endif /* all ways to drop privs */

#endif /* SECURE_MAN_UID */

	return;
}

/* 
 * function to (re)gain setuid privs by (a) setting euid from suid or (b)
 * (re)reversing uid w/ euid. Ignore if superuser.
 */
void regain_effective_privs (void)
{
#ifdef SECURE_MAN_UID
	/* user is root or we've already regained (never dropped) privs */
	if (ruid == 0 || geteuid() == euid)
		return;
		
	if (debug)
		fputs("regain_effective_privs()\n", stderr);
		
#  if defined(POSIX_SAVED_IDS)
	if (seteuid (euid))
		gripe_set_euid();
	
#  elif defined(BROKEN_LINUX_SAVED_IDS)
	if (setuid (euid))
		gripe_set_euid();

#  elif defined(HAVE_SETREUID) /* fallback (could just be #else ) */
	if (setreuid (ruid, euid))
		gripe_set_euid();

#  endif

#endif /* SECURE_MAN_UID */

	return;
}

/* remove() a file after dropping privs. If already dropped, just remove and 
   return, don't regain any privs! */
int remove_with_dropped_privs(const char *filename)
{
	int ret;
	
#ifdef SECURE_MAN_UID
	if (geteuid() != ruid) {
		drop_effective_privs();
		ret = remove (filename);
		if (debug)
			fprintf(stderr, "remove(\"%s\")\n", filename);
		regain_effective_privs();
	} else
#endif /* SECURE_MAN_UID */
		ret = remove (filename);

	if (ret != 0)
		error (0, errno, _( "can't remove %s"), filename);

	return ret;
}

/* 
 * If we want to execute a system command with no effective priveledges
 * we have to either
 * 	(a) Use saved id's (if available) to completely drop effective 
 * 	    priveledges and re-engage them after the call.
 *	(b) fork() and then drop effective privs in the child. Do the 
 * 	    system() command from the child and wait for it to die.
 * (b) does not need saved ids as, once dropped, the effective privs are 
 * not required in the child again. (a) does not require a fork() as the
 * system()'d processes will not have suid=MAN_OWNER and will be unable 
 * to gain any man derived priveledges.
 *
 * Obviously (a) is favoured, but there are many implementations...
 * some broken :-(
 */
int do_system_drop_privs (const char *command)
{
#ifdef SECURE_MAN_UID
	
#  if defined(POSIX_SAVED_IDS) || defined(BROKEN_LINUX_SAVED_IDS)

	/* if root or we already dropped privs, just do it */
	if (ruid == 0 || geteuid() == ruid)
		return do_system (command);
	else {
		int status;

		drop_effective_privs();
		status = do_system (command);
		regain_effective_privs();
		return status;
	}
	
#  elif defined(HAVE_SETREUID) /* fallback (could just be #else ) */

	/* if root, just do it */
	if (ruid == 0)
		return do_system (command);
	else {
		pid_t child;
		int status;

		fflush (NULL);
		child = fork ();

		if (child < 0) {
			error (0, errno, _( "can't fork"));
			status = 0;
		} else if (child == 0) {
			pop_all_cleanups ();
			if (setreuid (ruid, ruid))
				gripe_set_euid();
			exit (do_system (command));
		} else {
			pid_t res;
			int save = errno;
			do {	/* cope with non-restarting system calls */
				res = waitpid (child, &status, 0);
			} while ((res == -1) && (errno == EINTR));
			if (res == -1)
				status = -1;
			else
				errno = save;
		}

		return status;
	}
#  endif /* all ways to do a sys command after dropping privs */

#else  /* !SECURE_MAN_UID */
	return do_system (command);
#endif /* SECURE_MAN_UID */
}
