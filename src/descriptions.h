/*
 * descriptions.h: Interface to manipulating man page descriptions
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

#include "libdb/db_storage.h"

struct page_description {
	char *name;
	char *whatis;
	struct page_description *next;
};

extern struct page_description *parse_descriptions (const char *base_name,
						    const char *whatis);
extern void store_descriptions (const struct page_description *head,
				struct mandata *info, const char *base_name);
extern void free_descriptions (struct page_description *head);
