/*
 * hashtable.h: hashtable wrapper functions for convenience.
 *
 * Copyright (C), 1994, 1995, Graeme W. Wilford. (Wilf.)
 * Copyright (c) 2002 Colin Watson <cjwatson@debian.org>.
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with this
 * distribution.
 *
 * Sat Aug 20 15:01:02 BST 1994  Wilf. (G.Wilford@ee.surrey.ac.uk) 
 */

#include "lib/hashtable.h"

struct mandata;

extern struct hashtable *man_hash;

extern struct nlist *lookup (const char *s);
extern struct nlist *install_text (const char *name, const char *text);
extern struct nlist *install_db_ptr (const char *name, struct mandata *db_ptr);
extern void free_hashtab (void);
