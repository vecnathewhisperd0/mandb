#include "libdb/db_storage.h"

/* check_mandirs.c */
extern char *make_filename (const char *path, const char *name,
			    struct mandata *in, char *type);
extern char *filename_info (char *file, struct mandata *info,
			    const char *req_name);
extern void test_manfile (char *file, const char *path);
extern void update_db_time (void);
extern void reset_db_time (void);
extern short create_db (const char *manpath);
extern short update_db (const char *manpath);
extern short purge_missing (const char *manpath);

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
