/*
 * hashtable.c: hashtable wrapper functions for convenience.
 *
 * Copyright (C), 1994, 1995, Graeme W. Wilford. (Wilf.)
 * Copyright (c) 2002 Colin Watson <cjwatson@debian.org>.
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with this
 * distribution.
 *
 * Sat Aug 20 15:01:02 BST 1994  Wilf. (G.Wilford@ee.surrey.ac.uk) 
 *
 * The routines wrapped by these functions were formerly in this file, but
 * were moved to lib/hashtable.c for use elsewhere.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#if defined(STDC_HEADERS)
#  include <string.h>
#  include <stdlib.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#endif /* STDC_HEADERS */

#include "manconfig.h"
#include "lib/hashtable.h"
#include "libdb/mydbm.h"
#include "libdb/db_storage.h"
#include "hashtable.h"

struct hashtable *man_hash;

enum man_hash_type { HASH_TEXT, HASH_DB_PTR };

static void man_hash_free (int type, void *defn)
{
	if (type == HASH_TEXT)
		free (defn);
	else if (type == HASH_DB_PTR)
		free_mandata_struct (defn);
}

/* All the wrappers below create man_hash if necessary. */

/* Wrapper to look up a string. */
struct nlist *lookup (const char *s)
{
	if (!man_hash)
		man_hash = hash_create (&man_hash_free);
	return hash_lookup (man_hash, s, strlen (s));
}

/* Wrapper to insert a string or NULL. */
struct nlist *install_text (const char *name, const char *text)
{
	if (!man_hash)
		man_hash = hash_create (&man_hash_free);
	return hash_install (man_hash, name, strlen (name), HASH_TEXT,
			     text ? xstrdup (text) : NULL);
}

/* Wrapper to insert a pointer to an in-core mandata struct linked list. */
struct nlist *install_db_ptr (const char *name, struct mandata *db_ptr)
{
	if (!man_hash)
		man_hash = hash_create (&man_hash_free);
	return hash_install (man_hash, name, strlen (name), HASH_DB_PTR,
			     db_ptr);
}

/* Free up the hash tree. */
void free_hashtab (void)
{
	hash_free (man_hash);
	man_hash = NULL;
}
