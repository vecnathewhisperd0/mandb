/*
 * descriptions.c: manipulate man page descriptions
 *
 * Copyright (C) 2002 Colin Watson.
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

#include <libintl.h>
#define _(String) gettext (String)

#include "manconfig.h"
#include "libdb/db_storage.h"
#include "lib/error.h"
#include "descriptions.h"

static void gripe_bad_store (const char *name, const char *ext)
{
	if (quiet < 2)
		error (0, 0, _("warning: failed to store entry for %s(%s)"),
		       name, ext);
}

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
		if (debug)
			fprintf (stderr, "record = '%s'\n", record);

		/* Split the record into name and whatis description. */
		dash = strstr (record, " - ");
		if (dash)
			names = xstrndup (record, dash - record);
		else
			names = xstrdup (record);

		for (token = strtok (names, ","); token;
		     token = strtok (NULL, ",")) {
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

			if (STREQ (base_name, desc->name))
				seen_base_name = 1;
		}

		free (names);

		sep = nextsep;
	}

	/* If it isn't there already, add the base_name onto the returned
	 * list.
	 */
	if (!seen_base_name) {
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

/* Take a list of descriptions returned by parse_descriptions() and store
 * it into the database.
 */
void store_descriptions (const struct page_description *head,
			 struct mandata *info, const char *base_name)
{
	const struct page_description *desc;
	char save_id = info->id;

	if (debug)
		fprintf (stderr, "base_name = '%s'\n", base_name);

	for (desc = head; desc; desc = desc->next) {
		/* Either it's the real thing or merely a reference. Get the
		 * id and pointer right in either case.
		 */
		if (STREQ (base_name, desc->name)) {
			info->id = save_id;
			info->pointer = NULL;
			info->whatis = desc->whatis;
		} else {
			if (save_id < STRAY_CAT)
				info->id = WHATIS_MAN;
			else
				info->id = WHATIS_CAT;
			info->pointer = base_name;
			/* Don't waste space storing the whatis in the db
			 * more than once.
			 */
			info->whatis = NULL;
		}

		if (debug)
			fprintf (stderr, "name = '%s', id = %c\n",
				 desc->name, info->id);
		if (dbstore (info, desc->name) > 0) {
			gripe_bad_store (base_name, info->ext);
			break;
		}
	}
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
