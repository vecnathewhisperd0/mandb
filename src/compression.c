/*
 * compression.c: code to find decompressor / compression extension
 *  
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
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Sat Aug 20 15:01:02 BST 1994  Wilf. (G.Wilford@ee.surrey.ac.uk) 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <errno.h>
#include <signal.h>

#if defined(STDC_HEADERS)
#  include <string.h>
#  include <stdlib.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#else /* no string(s) header */
#endif /* STDC_HEADERS */

#ifndef STDC_HEADERS
extern int errno;
#endif

#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "lib/gettext.h"
#define _(String) gettext (String)

#include "manconfig.h"
#ifdef COMP_SRC /* must come after manconfig.h */

#include "lib/error.h"
#include "lib/pipeline.h"
#include "security.h"
#include "comp_src.h"

static char *file;	/* pointer to temp file name */
static int file_fd = -1;

/* initialise temp filename */
static __inline__ void create_ztemp (void)
{
	int oldmask = umask (022);
	drop_effective_privs ();
	file_fd = create_tempfile ("zman", &file);

	if (file_fd < 0)
		error (FATAL, errno, _("can't create a temporary filename"));
	regain_effective_privs ();
	umask (oldmask);
	atexit (remove_ztemp);
}

/* Take filename as arg, return structure containing decompressor 
   and extension, or NULL if no comp extension found. 
   If want_stem, set comp->stem to the filename without extension, which
   the caller should free.

   eg.
   	filename = /usr/man/man1/foo.1.gz 

	comp->prog = "/usr/bin/gzip -dc";
   	comp->ext = "gz";
   	comp->stem = "/usr/man/man1/foo.1";
 */
struct compression *comp_info (const char *filename, int want_stem)
{
	const char *ext;
	static struct compression hpux_comp = {GUNZIP " -S \"\"", "", NULL};

	ext = strrchr (filename, '.');

	if (ext) {
		struct compression *comp;
		for (comp = comp_list; comp->ext; comp++) {
			if (strcmp (comp->ext, ext + 1) == 0) {
				if (want_stem)
					comp->stem = xstrndup (filename,
							       ext - filename);
				else
					comp->stem = NULL;
				return comp;
			}
		}
	}

	ext = strstr (filename, ".Z/");
	if (ext) {
		if (want_stem)
			hpux_comp.stem = xstrndup (filename, ext - filename);
		else
			hpux_comp.stem = NULL;
		return &hpux_comp;
	}

	return NULL;
}

/* take filename w/o comp ext. as arg, return comp->stem as a relative
   compressed file or NULL if none found */
struct compression *comp_file (const char *filename)
{
	size_t len;
	char *compfile;
	struct compression *comp;

	compfile = strappend (NULL, filename, ".", NULL);
	len = strlen (compfile);
	
	for (comp = comp_list; comp->ext; comp++) {
		struct stat buf;
		
		compfile = strappend (compfile, comp->ext, NULL);

		if (stat (compfile, &buf) == 0) {
			comp->stem = compfile;
			return comp;
		}

		*(compfile + len) = '\0';
	}
	return NULL;
}

/* Set up a pointer to a unique temp filename on first call.
 * If this returns NULL, an error message will have been printed and the
 * caller should abort the current operation as appropriate.
 */
char *decompress (const char *filename, const struct compression *comp)
{
	pipeline *pl = pipeline_new ();
	command *cmd;
	int status;
	int save_debug = debug;

	if (!comp->prog || !*comp->prog) {
		/* TODO: Temporary workaround for poor decompression program
		 * detection, so deliberately left untranslated for now. See
		 * Debian bug #196097.
		 */
		error (0, 0, "missing decompression program for %s", filename);
		return NULL;
	}

	if (!file)
		create_ztemp();

	cmd = command_new_argstr (comp->prog);
	command_arg (cmd, filename);
	pipeline_command (pl, cmd);
	pl->want_out = file_fd;

	if (debug) {
#ifdef SECURE_MAN_UID
		fputs ("The following command done with dropped privs\n",
		       stderr);
#endif /* SECURE_MAN_UID */
		pipeline_dump (pl, stderr);
	}

	/* temporarily drop the debug flag, so that we can continue */
	debug = 0;
	status = do_system_drop_privs (pl);
	debug = save_debug;
	close (file_fd);
	file_fd = -1;

	if (status) {
		char *pl_str = pipeline_tostring (pl);
		remove_ztemp ();
		error (0, 0, _("command '%s' failed with exit status %d"),
		       pl_str, status);
		free (pl_str);
	}
	pipeline_free (pl);
	return file;
}

/* remove temporary file, drop privs if necessary */
void remove_ztemp (void)
{
	if (file) {
		if (file_fd >= 0)
			close (file_fd);
		(void) remove_with_dropped_privs (file);
		free (file);
		file = NULL;
	}
}

/* return temporary filename */
char *get_ztemp (void)
{
	return file;
}
#endif /* COMP_SRC */
