/*
 * hashtable.h: contains struct nlist
 *  
 * Copyright (C), 1994, 1995, Graeme W. Wilford. (Wilf.)
 * Copyright (c) 2002 Colin Watson.
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with this
 * distribution.
 *
 * Sat Aug 20 15:01:02 BST 1994  Wilf. (G.Wilford@ee.surrey.ac.uk) 
 */

#ifndef _HASHTABLE_H
#define _HASHTABLE_H

#include "libdb/mydbm.h"
#include "libdb/db_storage.h" 

typedef void (*hash_free_ptr) (int type, void *defn);

struct hashtable {
	struct nlist **hashtab;		/* the storage array */
	int unique;			/* unique hash values */
	int identical;			/* identical hash values */
	hash_free_ptr free_defn;	/* function to free a hash entry */
};

struct nlist {
        struct nlist *next;	/* next in the chain */
        char *name;		/* the _name_ */
        int type;		/* what did we store here ? */
        void *defn;		/* the _definition_ */
};

extern void plain_hash_free (int type, void *defn);

extern struct hashtable *hash_create (hash_free_ptr free_defn);
extern struct nlist *hash_lookup (const struct hashtable *ht,
				  const char *s, size_t len);
extern struct nlist *hash_install (struct hashtable *ht,
				   const char *name, size_t len,
				   int type, void *defn);
extern void hash_free (struct hashtable *ht);

#endif /* _HASHTABLE_H */
