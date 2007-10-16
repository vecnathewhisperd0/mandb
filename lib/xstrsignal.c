/*
 * xstrsignal.c: strsignal() compatibility shim
 *
 * Copyright (C) 2003 Colin Watson. Based on part of groff's
 * pipeline.c, copyright (C) 1989, 1990, 1991, 1992, 2000, 2001, 2002,
 * 2003 Free Software Foundation, Inc., written by James Clark.
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

#ifndef MAN_XSTRSIGNAL_H
#define MAN_XSTRSIGNAL_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef HAVE_STRSIGNAL

#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#  include <string.h>

const char *xstrsignal (int signum)
{
	return strsignal (signum);
}

#else /* !HAVE_STRSIGNAL */

#  include <signal.h>
#  include <stdio.h>

/* NetBSD declares sys_siglist in unistd.h. */
#  include <unistd.h>

const char *xstrsignal (int signum)
{
	/* log(256)/log(10) ~= 2.4 */
	static char buffer[sizeof ("Signal ") + sizeof (int) * 3 + 1];

#if HAVE_DECL_SYS_SIGLIST && defined(NSIG)
	if (signum >= 0 && signum < NSIG && sys_siglist[signum] != 0)
		return sys_siglist[signum];
#endif /* HAVE_DECL_SYS_SIGLIST && NSIG */

	sprintf (buffer, "Signal %d", signum);
	return buffer;
}

#endif /* HAVE_STRSIGNAL */

#endif /* MAN_XSTRSIGNAL_H */
