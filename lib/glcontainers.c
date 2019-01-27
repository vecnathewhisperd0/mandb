/*
 * glcontainers.c: common Gnulib container helpers
 *
 * Copyright (C) 2019 Colin Watson.
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "hash-pjw-bare.h"

#include "manconfig.h"

bool string_equals (const void *s1, const void *s2)
{
	return STREQ ((const char *) s1, (const char *) s2);
}

size_t string_hash (const void *s)
{
	return hash_pjw_bare (s, strlen ((const char *) s));
}

void plain_free (const void *s)
{
	/* gl_list declares the argument as const, but there doesn't seem to
	 * be a good reason for this.
	 */
	free ((void *) s);
}
