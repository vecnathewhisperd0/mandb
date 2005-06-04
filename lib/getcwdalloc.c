/*
 * getcwdalloc.c: get current directory name, allocating memory
 *
 * Copyright (C) 2004 Colin Watson.
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "manconfig.h"

/* Algorithm from glibc info documentation (Working Directory). */

char *getcwd_allocated (void)
{
	size_t size = 1024;

	while (1) {
		char *buffer = xmalloc (size);
		errno = 0;
		if (getcwd (buffer, size) == buffer)
			return buffer;
		if (errno != ERANGE) {
			free (buffer);
			return NULL;
		}
		free (buffer);
		size *= 2;
	}
}
