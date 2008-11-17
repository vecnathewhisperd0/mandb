/*
 * descriptions_store.c: store man page descriptions in database
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
#include <string.h>
#include <stdlib.h>

#include "gettext.h"
#define _(String) gettext (String)

#include "manconfig.h"

#include "error.h"

#include "db_storage.h"

#include "descriptions.h"

static void gripe_bad_store (const char *name, const char *ext)
{
	if (quiet < 2)
		error (0, 0, _("warning: failed to store entry for %s(%s)"),
		       name, ext);
}

/* Take a list of descriptions returned by parse_descriptions() and store
 * it into the database.
 */
void store_descriptions (const struct page_description *head,
			 struct mandata *info, const char *base)
{
	const struct page_description *desc;
	char save_id = info->id;

	debug ("base = '%s'\n", base);

	for (desc = head; desc; desc = desc->next) {
		/* Either it's the real thing or merely a reference. Get the
		 * id and pointer right in either case.
		 */
		if (STREQ (base, desc->name)) {
			info->id = save_id;
			info->pointer = NULL;
			info->whatis = desc->whatis;
		} else {
			if (save_id < STRAY_CAT)
				info->id = WHATIS_MAN;
			else
				info->id = WHATIS_CAT;
			info->pointer = base;
			/* Don't waste space storing the whatis in the db
			 * more than once.
			 */
			info->whatis = NULL;
		}

		debug ("name = '%s', id = %c\n", desc->name, info->id);
		if (dbstore (info, desc->name) > 0) {
			gripe_bad_store (base, info->ext);
			break;
		}
	}
}
