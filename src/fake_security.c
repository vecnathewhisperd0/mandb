/*
 * fake_security.c: fake routines for non setuid programs
 *  
 * Copyright (C), 1994, 1995, Graeme W. Wilford. (Wilf.)
 * Copyright (c) 2001 Colin Watson.
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

#include <sys/types.h>

#include <libintl.h>
#define _(String) gettext (String)

#include "manconfig.h"
#include "lib/error.h"
#include "security.h"

uid_t ruid, euid, uid;

#ifdef SECURE_MAN_UID
static struct passwd *man_owner;

struct passwd *get_man_owner (void)
{
	if (man_owner)
		return man_owner;

	man_owner = getpwuid (0);
	if (!man_owner)
		error (FAIL, errno, _("can't get passwd structure for uid 0"));
	return man_owner;
}
#endif /* SECURE_MAN_UID */

void drop_effective_privs (void)
{
	/* do nothing */
}

void regain_effective_privs (void)
{
	/* do nothing */
}

int remove_with_dropped_privs (const char *filename)
{
	return remove (filename);
}

int do_system_drop_privs (const char *command)
{
	return do_system (command);
}
