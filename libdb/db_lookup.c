/*
 * db_lookup.c: low level database interface routines for man.
 *
 * Copyright (C) 1994, 1995 Graeme W. Wilford. (Wilf.)
 * Copyright (C) 2001, 2002 Colin Watson.
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

#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>

#if defined(STDC_HEADERS)
#include <string.h>
#include <stdlib.h>
#elif defined(HAVE_STRING_H)
#include <string.h>
#elif defined(HAVE_STRINGS_H)
#include <strings.h>
#else /* no string(s) header file */
extern char *strsep();
#endif /* STDC_HEADERS */

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifndef STDC_HEADERS
extern long atol();
extern char *strsep();
extern int errno;
#endif /* not STDC_HEADERS */

#include <libintl.h>
#define _(String) gettext (String)

#include "manconfig.h"
#include "lib/error.h"
#include "mydbm.h"
#include "db_storage.h"

/* If using ndbm or BTREE, copy the static storage before doing anything
 * interesting with it. If using gdbm, firstkey and nextkey need to copy the
 * storage because our ordered wrappers keep an effectively static copy.
 */
datum copy_datum (datum dat)
{
	if (dat.dptr) {
		dat.dptr = memcpy (xmalloc (dat.dsize + 1),
				   dat.dptr, dat.dsize);
		dat.dptr[dat.dsize] = '\0';
	}
	return dat;
}

/* gdbm does locking itself. */
#if defined(NDBM) || defined(BTREE)
void gripe_lock (char *filename)
{
	error (0, errno, _("can't lock index cache %s"), filename);
}
#endif /* NDBM || BTREE */

/* issue fatal message, then exit */
void gripe_corrupt_data (void)
{
	error (FATAL, 0, _("index cache %s corrupt"), database);
}

/* deal with situation where we cannot replace a key */
void gripe_replace_key (const char *data)
{
	error (0, 0, _("cannot replace key %s"), data);
	gripe_corrupt_data ();
}

char *copy_if_set (const char *str)
{
	if (STREQ (str, "-"))
		return NULL;
	else
		return xstrdup (str);
}

const char *dash_if_unset (const char *str)
{
	if (str)
		return str;
	else
		return "-";
}

/* Just print out what would be stored in the db */
void dbprintf (const struct mandata *info)
{
	fprintf (stderr,
		 "name:      %s\n"
		 "sec. ext:  %s\n"
		 "section:   %s\n"
		 "comp. ext: %s\n"
		 "id:        %c\n"
		 "st_mtime   %ld\n"
		 "pointer:   %s\n"
		 "filter:    %s\n"
		 "whatis:    %s\n\n",
		 dash_if_unset (info->name),
		 info->ext, info->sec, info->comp,
		 info->id, (long) info->_st_mtime,
		 info->pointer, info->filter, info->whatis);
}

/* Form a multi-style key from page and extension info. The page should
 * *not* be name_to_key()'d - that should only happen to the parent.
 */
datum make_multi_key (const char *page, const char *ext)
{
	datum key;

	key.dsize = strlen (page) + strlen (ext) + 2;
	key.dptr = (char *) xmalloc (key.dsize);
	sprintf (key.dptr, "%s\t%s", page, ext);
	return key;
}

/* allocate a mandata structure */
struct mandata *infoalloc (void)
{
	struct mandata *info = xmalloc (sizeof (struct mandata));
	memset (info, 0, sizeof *info);
	return info;
}

/* Free allocated elements of a mandata structure, but not the structure
 * itself.
 */
void free_mandata_elements (struct mandata *pinfo)
{
	if (pinfo->addr)
		free (pinfo->addr);		/* free the 'content' */
	if (pinfo->name)
		free (pinfo->name);		/* free the real name */
}

/* Go through the linked list of structures, free()ing the 'content' and the
 * structs themselves.
 */
void free_mandata_struct (struct mandata *pinfo)
{
	while (pinfo) {
		struct mandata *next;

		next = pinfo->next;
		free_mandata_elements (pinfo);
		free (pinfo);			/* free the structure */
		pinfo = next;
	}
}

/* Get the key that should be used for a given name. The caller is
 * responsible for freeing the return value.
 */
char *name_to_key (const char *name)
{
	char *key = xstrdup (name);
	char *p;
	for (p = key; *p; ++p)
		*p = CTYPE (tolower, *p);
	return key;
}

/* return char ptr array to the data's fields */
char **split_data (char *content, char *start[])
{
        int count;

	/* initialise pointers to first N-1 fields */
	for (count = 0; count < FIELDS - 1 ; count++) {
		start[count] = strsep (&content, "\t");
		if (!start[count]) {
			error (0, 0,
			       _("only %d fields in content"),
			       count);
			gripe_corrupt_data ();
		}
	}

	/* initialise pointer to Nth field (whatis) */
	start[FIELDS - 1] = content;
	if (!start[FIELDS - 1]) {
		error (0, 0,
		       _("only %d fields in content"),
		       FIELDS - 1);
		gripe_corrupt_data ();
	}

	return start;
}

/* Parse the db-returned data and put it into a mandata format */
void split_content (char *cont_ptr, struct mandata *pinfo)
{
	char *start[FIELDS];
	char **data;

	data = split_data (cont_ptr, start);

	pinfo->name = copy_if_set (*(data++));
	pinfo->ext = *(data++);
	pinfo->sec = *(data++);
	pinfo->_st_mtime = (time_t) atol (*(data++));	/* time_t format */
	pinfo->id = **(data++);				/* single char id */
	pinfo->pointer = *(data++);
	pinfo->filter = *(data++);
	pinfo->comp = *(data++);
	pinfo->whatis = *(data);

	pinfo->addr = cont_ptr;
	pinfo->next = (struct mandata *) NULL;
}

/* The complement of split_content */
datum make_content (struct mandata *in)
{
	datum cont;
	static const char dash[] = "-";

	if (!in->pointer)
		in->pointer = dash;
	if (!in->filter)
		in->filter = dash;
	if (!in->comp)
		in->comp = dash;
	if (!in->whatis)
		in->whatis = dash + 1;

	cont.dsize = strlen (dash_if_unset (in->name)) +
		     strlen (in->ext) +
		     strlen (in->sec) +
		  /* strlen (in->_st_mtime) */ + 11 +
		  /* strlen (in->id) */ + 1 +
		     strlen (in->pointer) +
		     strlen (in->filter) +
		     strlen (in->comp) +
		     strlen (in->whatis) + 8;
	cont.dptr = (char *) xmalloc (cont.dsize);
#ifdef ANSI_SPRINTF
	cont.dsize = 1 + sprintf (cont.dptr,
		"%s\t%s\t%s\t%ld\t%c\t%s\t%s\t%s\t%s",
		dash_if_unset (in->name),
		in->ext,
		in->sec,
		in->_st_mtime,
		in->id,
		in->pointer,
		in->filter,
		in->comp,
		in->whatis);

	assert (strlen (cont.dptr) + 1 == (size_t) cont.dsize);
#else /* !ANSI_SPRINTF */
	sprintf (cont.dptr, "%s\t%s\t%s\t%ld\t%c\t%s\t%s\t%s\t%s",
		 dash_if_unset (in->name),
		 in->ext,
		 in->sec,
		 (long)in->_st_mtime,
		 in->id,
		 in->pointer,
		 in->filter,
		 in->comp,
		 in->whatis);

	cont.dsize = strlen (cont.dptr) + 1;	/* to be sure (st_mtime) */
#endif /* ANSI_SPRINTF */

#ifdef NDBM
	/* limit of 4096 bytes of data using ndbm */
	if (cont.dsize > 4095) {
		cont.dptr[4095] = '\0';
		cont.dsize = 4096;
	}
#endif
	return cont;
}

/* Extract all of the names/extensions associated with this key. Each case
 * variant of a name will be returned separately.
 *
 * names and ext should be pointers to valid memory which will be filled in
 * with the address of the allocated arrays of names and extensions. The
 * caller is expected to free these arrays.
 */
int list_extensions (char *data, char ***names, char ***ext)
{
	int count = 0;
	int bound = 4;	/* most multi keys will have fewer than this */

	*names = xmalloc (bound * sizeof **names);
	*ext   = xmalloc (bound * sizeof **ext);
	while (((*names)[count] = strsep (&data, "\t")) != NULL) {
		(*ext)[count] = strsep (&data, "\t");
		if ((*ext)[count])
			++count;
		else
			break;

		if (count >= bound) {
			bound *= 2;
			*names = xrealloc (*names, bound * sizeof **names);
			*ext   = xrealloc (*ext,   bound * sizeof **ext);
		}
	}

	if (debug)
		fprintf (stderr, "found %d names/extensions\n", count);
	return count;
}

/* These should be bitwise-ored together. */
#define ALL	    0
#define	EXACT	    1
#define MATCH_CASE  2

/*
 There are three possibilities on lookup:

 1) No data exists, lookup will fail, returned structure will be NULL.
 2) One data item exists. Item is returned as first in set of structures.
 3) Many items exist. They are all returned, in a multiple structure set.
 */
#ifndef FAST_BTREE
static struct mandata *dblookup (const char *page, const char *section,
				 int flags)
{
	struct mandata *info = NULL;
	datum key, cont;

	key.dptr = name_to_key (page);
	key.dsize = strlen (key.dptr) + 1;
	cont = MYDBM_FETCH (dbf, key);
	free (key.dptr);

	if (cont.dptr == NULL) {		/* No entries at all */
		return info;			/* indicate no entries */
	} else if (*cont.dptr != '\t') {	/* Just one entry */
		info = infoalloc ();
		split_content (cont.dptr, info);
		if (!info->name)
			info->name = xstrdup (page);
		if (!(flags & MATCH_CASE) || STREQ (info->name, page)) {
			if (section == NULL ||
			    STRNEQ (section, info->ext,
				    flags & EXACT ? strlen (info->ext) :
						    strlen (section))) {
				return info;
			}
		}
		free_mandata_struct (info);
		return NULL;
	} else {				/* multiple entries */
		char **names, **ext;
		struct mandata *ret = NULL;
		int refs, i;

		/* Extract all of the case-variant-names/extensions
		 * associated with this key.
		 */

		refs = list_extensions (cont.dptr + 1, &names, &ext);

		/* Make the multi keys and look them up */

		for (i = 0; i < refs; ++i) {
			datum multi_cont;

			/* Decide whether this part of a multi key is
			 * suitable.
			 */

			if ((flags & MATCH_CASE) && !STREQ (names[i], page))
				continue;

			if (section != NULL &&
			    !STRNEQ (section, ext[i],
				     flags & EXACT ? strlen (ext[i])
						   : strlen (section)))
				continue;

			/* So the key is suitable ... */
			key = make_multi_key (names[i], ext[i]);
			if (debug)
				fprintf (stderr, "multi key lookup (%s)\n",
					 key.dptr);
			multi_cont = MYDBM_FETCH (dbf, key);
			if (multi_cont.dptr == NULL) {
				error (0, 0, _("bad fetch on multi key %s"),
				       key.dptr);
				gripe_corrupt_data ();
			}
			free (key.dptr);

			/* allocate info struct, fill it in and
			   point info to the next in the list */
			if (!ret)
				ret = info = infoalloc ();
			else
				info = info->next = infoalloc ();
			split_content (multi_cont.dptr, info);
			if (!info->name)
				info->name = xstrdup (names[i]);
		}

		free (names);
		free (ext);
		MYDBM_FREE (cont.dptr);
		return ret;
	}
}
#endif /* !FAST_BTREE */

struct mandata *dblookup_all (const char *page, const char *section,
			      int match_case)
{
	return dblookup (page, section, ALL | (match_case ? MATCH_CASE : 0));
}

struct mandata *dblookup_exact (const char *page, const char *section,
				int match_case)
{
	return dblookup (page, section, EXACT | (match_case ? MATCH_CASE : 0));
}
