/*
 * fake_security.c: fake routines for non setuid programs
 *  
 * Copyright (C) 1994, 1995 Graeme W. Wilford. (Wilf.)
 * Copyright (C) 2001 Colin Watson.
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
