/*
 * db_storage.h: define mandata structure, some macros and prototypes
 *  
 * Copyright (C), 1994, 1995, Graeme W. Wilford. (Wilf.)
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with this
 * distribution.
 *
 * Sat Oct 29 13:09:31 GMT 1994  Wilf. (G.Wilford@ee.surrey.ac.uk) 
 */

#ifndef DB_STORAGE_H
#define DB_STORAGE_H

/* These definitions give an inherent precedence to each particular type
   of manual page:
   
   ULT_MAN:	ultimate manual page, the full source nroff file.
   SO_MAN:	source nroff file containing .so request to an ULT_MAN.
   WHATIS_MAN:	virtual `whatis referenced' page pointing to an ULT_MAN.
   STRAY_CAT:	pre-formatted manual page with no source.
   WHATIS_CAT:  virtual `whatis referenced' page pointing to a STRAY_CAT. */

/* WHATIS_MAN and WHATIS_CAT are deprecated. */

#define ULT_MAN		'A'
#define SO_MAN		'B'
#define WHATIS_MAN	'C'
#define STRAY_CAT	'D'
#define WHATIS_CAT	'E'

#define ENTRIES 50      /* Max unique extensions per manual page name */
#define FIELDS  9       /* No of fields in each database page `content' */

#include "sys/time.h"	/* for time_t */

struct mandata {
	struct mandata *next;		/* ptr to next structure, if any */
	char *addr;			/* ptr to memory containing the fields */

	char *name;			/* Name of page, if != key */

	/* The following are all const because they should be pointers to
	 * parts of strings allocated elsewhere (often the addr field above)
	 * and should not be written through or freed themselves.
	 */
	const char *ext;		/* Filename ext w/o comp ext */
	const char *sec;		/* Section name/number */
	char id;			/* id for this entry */
	const char *pointer;		/* id related file pointer */
	const char *comp;		/* Compression extension */
	const char *filter;		/* filters needed for the page */
	const char *whatis;		/* whatis description for page */
	time_t _st_mtime;		/* mod time for file */
}; 

/* used by the world */
extern __inline__ struct mandata *dblookup_all(const char *page,
					       const char *section,
					       int match_case);
extern __inline__ struct mandata *dblookup_exact(const char *page,
						 const char *section,
						 int match_case);
extern int dbstore(struct mandata *in, const char *basename);
extern int dbdelete(const char *name, struct mandata *in);
extern void dbprintf(const struct mandata *info);
extern void free_mandata_elements(struct mandata *pinfo);
extern void free_mandata_struct(struct mandata *pinfo);
extern void split_content(char *cont_ptr, struct mandata *pinfo);
extern int compare_ids(char a, char b);

/* local to db routines */
extern __inline__ void gripe_lock(char *filename);
extern __inline__ void gripe_corrupt_data(void);
extern datum make_multi_key(const char *page, const char *ext);
extern __inline__ struct mandata *infoalloc(void);
extern char *name_to_key(const char *name);
extern char **split_data(char *content, char *start[]);
extern datum make_content(struct mandata *in);
extern int list_extensions(char *data, char *names[], char *ext[]);
extern void gripe_replace_key(const char *data);
extern void gripe_bad_multi_key(const char *data);
extern char *copy_if_set(const char *str);
extern const char *dash_if_unset(const char *str);

#endif
