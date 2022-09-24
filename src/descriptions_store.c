/*
 * descriptions_store.c: store man page descriptions in database
 *
 * Copyright (C) 2002, 2003, 2006, 2007, 2008, 2011 Colin Watson.
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "gettext.h"
#define _(String) gettext (String)

#include "error.h"
#include "gl_array_list.h"
#include "gl_xlist.h"
#include "stat-time.h"
#include "xalloc.h"

#include "manconfig.h"

#include "debug.h"
#include "filenames.h"
#include "glcontainers.h"

#include "db_storage.h"

#include "ult_src.h"
#include "descriptions.h"

static void gripe_bad_store (const char *name, const char *ext)
{
	if (quiet < 2)
		error (0, 0, _("warning: failed to store entry for %s(%s)"),
		       name, ext);
}

/* Is PARENT a prefix of CHILD, such that CHILD is in the manual hierarchy
 * PARENT?  This requires that the part of CHILD following PARENT start with
 * "/man".
 */
static int is_prefix (const char *parent, const char *child)
{
	return (STRNEQ (child, parent, strlen (parent)) &&
		STRNEQ (child + strlen (parent), "/man", 4));
}

/* Take a list of descriptions returned by parse_descriptions() and store
 * it into the database.
 */
void store_descriptions (MYDBM_FILE dbf, gl_list_t descs, struct mandata *info,
			 const char *path, const char *base, gl_list_t trace)
{
	const struct page_description *desc;
	const char *trace_name;
	gl_list_t whatis_infos;
	struct mandata *whatis_info;
	char best_id = WHATIS_CAT + 1;
	const char *best_name = base;

	if (gl_list_size (descs) && trace) {
		GL_LIST_FOREACH (trace, trace_name)
			debug ("trace: '%s'\n", trace_name);
	}

	whatis_infos = gl_list_create_empty (GL_ARRAY_LIST, NULL, NULL,
					     (gl_listelement_dispose_fn)
					     free_mandata_struct,
					     true);

	GL_LIST_FOREACH (descs, desc) {
		/* Either it's the real thing or merely a reference. Get the
		 * id and pointer right in either case.
		 */
		bool found_real_page = false;
		bool found_external = false;

		whatis_info = XZALLOC (struct mandata);
		whatis_info->ext = xstrdup (info->ext);
		whatis_info->sec = xstrdup (info->sec);
		whatis_info->id = info->id;
		if (info->comp)
			whatis_info->comp = xstrdup (info->comp);
		if (info->filter)
			whatis_info->filter = xstrdup (info->filter);
		if (desc->whatis)
			whatis_info->whatis = xstrdup (desc->whatis);
		whatis_info->mtime = info->mtime;

		if (STREQ (base, desc->name))
			found_real_page = true;
		else if (trace) {
			GL_LIST_FOREACH (trace, trace_name) {
				struct mandata *trace_info;
				struct stat st;

				trace_info = filename_info (trace_name,
							    quiet < 2);
				if (!trace_info ||
				    !STREQ (trace_info->name, desc->name))
					goto next_trace;

				if (path && !is_prefix (path, trace_name)) {
					/* Link outside this manual
					 * hierarchy; skip this description.
					 */
					found_external = true;
					free_mandata_struct (trace_info);
					break;
				}
				free (whatis_info->ext);
				whatis_info->ext = xstrdup (trace_info->ext);
				free (whatis_info->sec);
				whatis_info->sec = xstrdup (trace_info->sec);
				if (!gl_list_next_node (trace, trace_node)) {
					if (info->id == SO_MAN)
						whatis_info->id = ULT_MAN;
				} else {
					if (info->id == ULT_MAN)
						whatis_info->id = SO_MAN;
				}
				free (whatis_info->comp);
				if (trace_info->comp)
					whatis_info->comp = xstrdup
						(trace_info->comp);
				else
					whatis_info->comp = NULL;
				if (lstat (trace_name, &st) == 0)
					whatis_info->mtime = get_stat_mtime
						(&st);
				else
					whatis_info->mtime = info->mtime;
				found_real_page = true;

next_trace:
				free_mandata_struct (trace_info);
			}
		}

		if (found_external) {
			debug ("skipping '%s'; link outside manual "
			       "hierarchy\n", desc->name);
			free_mandata_struct (whatis_info);
			continue;
		}

		if (!found_real_page) {
			whatis_info->name = xstrdup (desc->name);
			if (info->id < STRAY_CAT)
				whatis_info->id = WHATIS_MAN;
			else
				whatis_info->id = WHATIS_CAT;
			/* Don't waste space storing the whatis in the db
			 * more than once.
			 */
			free (whatis_info->whatis);
			whatis_info->whatis = NULL;
			gl_list_add_last (whatis_infos, whatis_info);
			continue;
		}

		if (whatis_info->id < best_id) {
			best_id = whatis_info->id;
			best_name = desc->name;
		}

		debug ("name = '%s', id = %c\n", desc->name, whatis_info->id);
		if (dbstore (dbf, whatis_info, desc->name) > 0) {
			gripe_bad_store (base, whatis_info->ext);
			free_mandata_struct (whatis_info);
			goto out;
		}

		free_mandata_struct (whatis_info);
	}

	GL_LIST_FOREACH (whatis_infos, whatis_info) {
		char *name;

		name = whatis_info->name;
		whatis_info->name = NULL;

		whatis_info->pointer = xstrdup (best_name);

		debug ("name = '%s', id = %c, pointer = '%s'\n",
		       name, whatis_info->id, whatis_info->pointer);
		if (dbstore (dbf, whatis_info, name) > 0) {
			gripe_bad_store (base, whatis_info->ext);
			free (name);
			goto out;
		}

		free (name);
	}

out:
	gl_list_free (whatis_infos);
}
