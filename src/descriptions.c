/*
 * descriptions.c: manipulate man page descriptions
 *
 * Copyright (C) 2002, 2003, 2006 Colin Watson.
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

#include <stdio.h>

#if defined(STDC_HEADERS)
#  include <string.h>
#  include <stdlib.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#else /* no string(s) header */
extern char *strchr();
#endif /* no string(s) header */

#include "manconfig.h"
#include "descriptions.h"

/* Parse the description in a whatis line returned by find_name() into a
 * sequence of names and whatis descriptions.
 */
struct page_description *parse_descriptions (const char *base_name,
					     const char *whatis)
{
	const char *sep, *nextsep;
	struct page_description *desc = NULL, *head = NULL;
	int seen_base_name = 0;

	if (!whatis)
		return NULL;

	sep = whatis;

	while (sep) {
		char *record;
		size_t length;
		const char *dash;
		char *names;
		const char *token;

		/* Use a while loop so that we skip over things like the
		 * result of double line breaks.
		 */
		while (*sep == 0x11 || *sep == ' ')
			++sep;
		nextsep = strchr (sep, 0x11);

		/* Get this record as a null-terminated string. */
		if (nextsep)
			length = (size_t) (nextsep - sep);
		else
			length = strlen (sep);
		if (length == 0)
			break;

		record = xstrndup (sep, length);
		debug ("record = '%s'\n", record);

		/* Split the record into name and whatis description. */
		dash = strstr (record, " - ");
		if (dash)
			names = xstrndup (record, dash - record);
		else
			names = xstrdup (record);

		for (token = strtok (names, ","); token;
		     token = strtok (NULL, ",")) {
			/* Skip name tokens containing whitespace. They are
			 * almost never useful as manual page names.
			 */
			if (strpbrk (token, " \t") != NULL)
				continue;

			/* Allocate new description node. */
			if (head) {
				desc->next = malloc (sizeof *desc);
				desc = desc->next;
			} else {
				desc = malloc (sizeof *desc);
				head = desc;
			}
			desc->name   = trim_spaces (token);
			desc->whatis = dash ? trim_spaces (dash + 3) : NULL;
			desc->next   = NULL;

			if (base_name && STREQ (base_name, desc->name))
				seen_base_name = 1;
		}

		free (names);

		sep = nextsep;
	}

	/* If it isn't there already, add the base_name onto the returned
	 * list.
	 */
	if (base_name && !seen_base_name) {
		if (head) {
			desc->next = malloc (sizeof *desc);
			desc = desc->next;
			desc->whatis = xstrdup (head->whatis);
		} else {
			desc = malloc (sizeof *desc);
			head = desc;
			desc->whatis = NULL;
		}
		desc->name = xstrdup (base_name);
		desc->next = NULL;
	}

	return head;
}

/* Free a description list and all its contents. */
void free_descriptions (struct page_description *head)
{
	struct page_description *desc = head, *prev;

	while (desc) {
		free (desc->name);
		if (desc->whatis)
			free (desc->whatis);
		prev = desc;
		desc = desc->next;
		free (prev);
	}
}
