/*
 * mydbm.h: database interface definitions and prototypes.
 *
 * Copyright (C) 1994, 1995, Graeme W. Wilford. (Wilf.)
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
 *
 * Header file to make programming independent of db type used
 *
 * Currently satisfies:
 *
 *	*hash based* 
 *		GNU dbm: 	(gdbm & ndbm)
 *		Berkeley db: 	(ndbm)
 *		`native': 	(ndbm)
 *
 *	*binary tree based*
 *		Berkeley db: 	(BTREE)
 *
 * Tue Apr 26 12:56:44 BST 1994  Wilf. (G.Wilford@ee.surrey.ac.uk) 
 */

#ifndef MYDBM_H
# define MYDBM_H

# if defined(GDBM) && !defined(NDBM) && !defined(BTREE)

#  include <gdbm.h>

#  ifndef HAVE_GDBM_EXISTS
extern inline int gdbm_exists(GDBM_FILE dbf, datum key);
#  endif /* !HAVE_GDBM_EXISTS */

/* gdbm_nextkey() is not lexicographically sorted, so we need to keep the
 * filename around to use as a hash key.
 */
typedef struct {
	char *name;
	GDBM_FILE file;
} *man_gdbm_wrapper;

man_gdbm_wrapper man_gdbm_open_wrapper (const char *name, GDBM_FILE file);
datum man_gdbm_firstkey (man_gdbm_wrapper wrap);
datum man_gdbm_nextkey (man_gdbm_wrapper wrap, datum key);
void man_gdbm_close (man_gdbm_wrapper wrap);

#  define BLK_SIZE			0  /* to invoke normal fs block size */
#  define DB_EXT				".db"
#  define MYDBM_FILE 			man_gdbm_wrapper
#  define MYDBM_DPTR(d)			((d).dptr)
#  define MYDBM_SET_DPTR(d, value)	((d).dptr = (value))
#  define MYDBM_DSIZE(d)		((d).dsize)
#  define MYDBM_CTRWOPEN(file)		man_gdbm_open_wrapper(file,\
					  gdbm_open(file, BLK_SIZE,\
					  GDBM_NEWDB|GDBM_FAST, DBMODE, 0))
#  define MYDBM_CRWOPEN(file)		man_gdbm_open_wrapper(file,\
					  gdbm_open(file, BLK_SIZE,\
					  GDBM_WRCREAT|GDBM_FAST, DBMODE, 0))
#  define MYDBM_RWOPEN(file)		man_gdbm_open_wrapper(file,\
					  gdbm_open(file, BLK_SIZE,\
					  GDBM_WRITER|GDBM_FAST, DBMODE, 0))
#  define MYDBM_RDOPEN(file)		man_gdbm_open_wrapper(file,\
					  gdbm_open(file, BLK_SIZE,\
					  GDBM_READER, DBMODE, 0))
#  define MYDBM_INSERT(dbf, key, cont)	gdbm_store((dbf)->file, key, cont, GDBM_INSERT)
#  define MYDBM_REPLACE(dbf, key, cont) 	gdbm_store((dbf)->file, key, cont, GDBM_REPLACE)
#  define MYDBM_EXISTS(dbf, key)		gdbm_exists((dbf)->file, key)
#  define MYDBM_DELETE(dbf, key)		gdbm_delete((dbf)->file, key)
#  define MYDBM_FETCH(dbf, key)		gdbm_fetch((dbf)->file, key)
#  define MYDBM_CLOSE(dbf)		man_gdbm_close(dbf)
#  define MYDBM_FIRSTKEY(dbf)		man_gdbm_firstkey(dbf)
#  define MYDBM_NEXTKEY(dbf, key)		man_gdbm_nextkey(dbf, key)
#  define MYDBM_REORG(dbf)		gdbm_reorganize((dbf)->file)
#  define MYDBM_FREE(x)			free(x)

# elif defined(NDBM) && !defined(GDBM) && !defined(BTREE)

#  include <ndbm.h>

#  if HAVE_FCNTL_H
#   include <fcntl.h>
#  endif

/* Berkeley db routines emulate ndbm but don't add .dir & .pag, just .db! */
#  ifdef _DB_H_ /* has Berkeley db.h been included? */
#   define BERKELEY_DB
#  endif /* _DB_H_ */

extern DBM *ndbm_flopen(char *file, int flags, int mode);
extern int ndbm_flclose(DBM *dbf);

#  define DB_EXT				""
#  define MYDBM_FILE 			DBM*
#  define MYDBM_DPTR(d)			((d).dptr)
#  define MYDBM_SET_DPTR(d, value)	((d).dptr = (value))
#  define MYDBM_DSIZE(d)		((d).dsize)
#  define MYDBM_CTRWOPEN(file)		ndbm_flopen(file, O_TRUNC|O_CREAT|O_RDWR, DBMODE)
#  define MYDBM_CRWOPEN(file)             ndbm_flopen(file, O_CREAT|O_RDWR, DBMODE)
#  define MYDBM_RWOPEN(file)		ndbm_flopen(file, O_RDWR, DBMODE)
#  define MYDBM_RDOPEN(file)		ndbm_flopen(file, O_RDONLY, DBMODE)
#  define MYDBM_INSERT(dbf, key, cont)	dbm_store(dbf, key, cont, DBM_INSERT)
#  define MYDBM_REPLACE(dbf, key, cont)   dbm_store(dbf, key, cont, DBM_REPLACE)
#  define MYDBM_EXISTS(dbf, key)		(dbm_fetch(dbf, key).dptr != NULL)
#  define MYDBM_DELETE(dbf, key)		dbm_delete(dbf, key)
#  define MYDBM_FETCH(dbf, key) 		copy_datum(dbm_fetch(dbf, key))
#  define MYDBM_CLOSE(dbf)		ndbm_flclose(dbf)
#  define MYDBM_FIRSTKEY(dbf)		copy_datum(dbm_firstkey(dbf))
#  define MYDBM_NEXTKEY(dbf, key)		copy_datum(dbm_nextkey(dbf))
#  define MYDBM_REORG(dbf)		/* nothing - not implemented */
#  define MYDBM_FREE(x)			free (x)

# elif defined(BTREE) && !defined(NDBM) && !defined(GDBM)

#  include <sys/types.h>
#  include <fcntl.h>
#  include <limits.h>
#  include BDB_H

typedef DBT datum;

extern DB *btree_flopen(char *filename, int flags, int mode);
extern inline int btree_close(DB *db);
extern inline int btree_exists(DB *db, datum key);
extern inline datum btree_fetch(DB *db, datum key);
extern int btree_insert(DB *db, datum key, datum cont);
extern inline datum btree_firstkey(DB *db);
extern inline datum btree_nextkey(DB *db);
extern inline int btree_replace(DB *db, datum key, datum content);
extern inline int btree_nextkeydata(DB *db, datum *key, datum *cont);

#  define DB_EXT			".bt"
#  define MYDBM_FILE			DB*
#  define MYDBM_DPTR(d)			((char *) (d).data)
#  define MYDBM_SET_DPTR(d, value)	((d).data = (char *) (value))
#  define MYDBM_DSIZE(d)		((d).size)
#  define MYDBM_CTRWOPEN(file)		btree_flopen(file, O_TRUNC|O_CREAT|O_RDWR, DBMODE)
#  define MYDBM_CRWOPEN(file)             btree_flopen(file, O_CREAT|O_RDWR, DBMODE)
#  define MYDBM_RWOPEN(file)		btree_flopen(file, O_RDWR, DBMODE)
#  define MYDBM_RDOPEN(file)		btree_flopen(file, O_RDONLY, DBMODE)
#  define MYDBM_INSERT(db, key, cont)	btree_insert(db, key, cont)
#  define MYDBM_REPLACE(db, key, cont)	btree_replace(db, key, cont)
#  define MYDBM_EXISTS(db, key)		btree_exists(db, key)
#  define MYDBM_DELETE(db, key)		((db->del)(db, &key, 0) ? -1 : 0)
#  define MYDBM_FETCH(db, key)		btree_fetch(db, key)
#  define MYDBM_CLOSE(db)		btree_close(db)
#  define MYDBM_FIRSTKEY(db)		btree_firstkey(db)
#  define MYDBM_NEXTKEY(db, key)	btree_nextkey(db)
#  define MYDBM_REORG(db)		/* nothing - not implemented */
#  define MYDBM_FREE(x)			free(x)

# else /* not GDBM or NDBM or BTREE */
#  error Define either GDBM, NDBM or BTREE before including mydbm.h
# endif /* not GDBM or NDBM or BTREE */

#define MYDBM_RESET_DSIZE(d)		(MYDBM_DSIZE(d) = strlen(MYDBM_DPTR(d)) + 1)
#define MYDBM_SET(d, value)		do { MYDBM_SET_DPTR(d, value); MYDBM_RESET_DSIZE(d); } while (0)

extern char *database;
extern MYDBM_FILE dbf;

/* db_lookup.c */
extern datum copy_datum (datum dat);

/* db_ver.c */
extern void dbver_wr(MYDBM_FILE dbfile);
extern int dbver_rd(MYDBM_FILE dbfile);

#endif /* MYDBM_H */
