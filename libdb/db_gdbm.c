/*
 * db_gdbm.c: low level gdbm interface routines for man.
 *
 * Copyright (C) 1994, 1995 Graeme W. Wilford. (Wilf.)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Mon Aug  8 20:35:30 BST 1994  Wilf. (G.Wilford@ee.surrey.ac.uk)
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef GDBM

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
#include "mydbm.h"

static struct hashtable *parent_sortkey_hash;

struct sortkey {
	datum key;
	struct sortkey *next;
};

man_gdbm_wrapper man_gdbm_open_wrapper (const char *name, GDBM_FILE file)
{
	man_gdbm_wrapper wrap;

	if (!file)
		return NULL;

	wrap = xmalloc (sizeof *wrap);
	wrap->name = xstrdup (name);
	wrap->file = file;

	return wrap;
}

static void parent_sortkey_hash_free (void *defn)
{
	/* Automatically free child hashtables on removal. */
	hash_free ((struct hashtable *) defn);
}

static void sortkey_hash_free (void *defn)
{
	struct sortkey *key = (struct sortkey *) defn;
	free (key->key.dptr);
	free (key);
}

static int sortkey_compare (const void *a, const void *b)
{
	const struct sortkey **left = (const struct sortkey **) a;
	const struct sortkey **right = (const struct sortkey **) b;
	int cmp;

	/* Sentinel NULL elements sort to the end. */
	if (!(*left)->key.dptr)
		return 1;
	else if (!(*right)->key.dptr)
		return -1;

	cmp = strncmp ((*left)->key.dptr, (*right)->key.dptr,
		       ((*left)->key.dsize < (*right)->key.dsize)
				? (*left)->key.dsize : (*right)->key.dsize);
	if (cmp)
		return cmp;
	else if ((*left)->key.dsize < (*right)->key.dsize)
		return 1;
	else if ((*left)->key.dsize > (*right)->key.dsize)
		return -1;
	else
		return 0;
}

static datum empty_datum = { NULL, 0 };

/* We keep a hashtable of filenames to sorted lists of keys. Each list is
 * stored both with links from each element to the next and in a hashtable,
 * so that both sequential access and random access are quick. This is
 * necessary for a reasonable ordered implementation of nextkey.
 */
datum man_gdbm_firstkey (man_gdbm_wrapper wrap)
{
	struct hashtable *sortkey_hash;
	struct sortkey **keys, *firstkey;
	int numkeys = 0, maxkeys = 256;
	int i;

	/* Build the raw list of keys and sort it. */
	keys = xmalloc (maxkeys * sizeof *keys);
	keys[0] = xmalloc (sizeof **keys);
	keys[0]->key = gdbm_firstkey (wrap->file);
	while (keys[numkeys]->key.dptr) {
		if (++numkeys >= maxkeys) {
			maxkeys *= 2;
			keys = xrealloc (keys, maxkeys * sizeof *keys);
		}
		keys[numkeys] = xmalloc (sizeof **keys);
		keys[numkeys]->key =
			gdbm_nextkey (wrap->file, keys[numkeys - 1]->key);
	}
	free (keys[numkeys]);
	keys[numkeys] = NULL;	/* simplifies the empty case */
	qsort (keys, numkeys, sizeof *keys, &sortkey_compare);

	/* Link the elements together and insert them into a hash. */
	sortkey_hash = hash_create (&sortkey_hash_free);
	for (i = 0; i < numkeys; ++i) {
		if (i < numkeys - 1)
			keys[i]->next = keys[i + 1];
		else
			keys[i]->next = NULL;
		hash_install (sortkey_hash,
			      keys[i]->key.dptr, keys[i]->key.dsize, keys[i]);
	}
	firstkey = keys[0];
	free (keys);	/* element memory now owned by hashtable */

	if (!parent_sortkey_hash)
		parent_sortkey_hash = hash_create (&parent_sortkey_hash_free);

	/* Remember this structure for use by nextkey. */
	hash_install (parent_sortkey_hash,
		      wrap->name, strlen (wrap->name), sortkey_hash);

	if (firstkey)
		return copy_datum (firstkey->key);
	else
		return empty_datum; /* dptr is NULL, so no copy needed */
}

datum man_gdbm_nextkey (man_gdbm_wrapper wrap, datum key)
{
	struct hashtable *sortkey_hash;
	struct sortkey *sortkey;

	if (!parent_sortkey_hash)
		return empty_datum;
	sortkey_hash = hash_lookup (parent_sortkey_hash,
				    wrap->name, strlen (wrap->name));
	if (!sortkey_hash)
		return empty_datum;

	sortkey = hash_lookup (sortkey_hash, key.dptr, key.dsize);
	if (!sortkey || !sortkey->next)
		return empty_datum;

	return copy_datum (sortkey->next->key);
}

void man_gdbm_close (man_gdbm_wrapper wrap)
{
	if (!wrap)
		return;

	if (parent_sortkey_hash) {
		struct hashtable *sortkey_hash =
			hash_lookup (parent_sortkey_hash,
				     wrap->name, strlen (wrap->name));
		if (sortkey_hash)
			hash_remove (parent_sortkey_hash,
				     wrap->name, strlen (wrap->name));
	}

	free (wrap->name);
	gdbm_close (wrap->file);
	free (wrap);
}

#ifndef HAVE_GDBM_EXISTS

int gdbm_exists (GDBM_FILE dbf, datum key)
{
	char *memory;

	memory = gdbm_fetch (dbf, key).dptr;
	if (memory) {
		free (memory);
		return 1;
	}

	return 0;
}

#endif /* !HAVE_GDBM_EXISTS */

#endif /* GDBM */
