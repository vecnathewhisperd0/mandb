/*
 * manp.h: Interface to manpath calculations
 *
 * Copyright (C) 1990, 1991 John W. Eaton.
 * Copyright (C) 1994, 1995 Graeme W. Wilford. (Wilf.)
 * Copyright (C) 2001, 2002 Colin Watson.
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

/* manp.c */
extern char *cat_manpath (char *manp);
extern char *add_nls_manpath (char *manpathlist, const char *locale);
extern char *manpath (const char *systems);
extern void create_pathlist (const char *manp, char **mp);
extern char *get_mandb_manpath (void);
extern char *get_catpath (const char *name, int cattype);
extern int is_global_mandir (const char *dir);
extern void read_config_file (void);
extern const char *get_def (const char *thing, const char *def);
extern const char *get_def_user (const char *thing, const char *def);
extern const char **get_sections (void);
