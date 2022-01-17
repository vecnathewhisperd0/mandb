/*
 * manconv_client.c: use manconv in a pipeline
 *
 * Copyright (C) 2007, 2008, 2010 Colin Watson.
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
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "gl_array_list.h"
#include "gl_xlist.h"
#include "idpriv.h"
#include "xalloc.h"
#include "xvasprintf.h"

#include "manconfig.h"

#include "pipeline.h"

#include "appendstr.h"
#include "glcontainers.h"
#include "sandbox.h"
#include "security.h"

#include "decompress.h"
#include "manconv.h"
#include "manconv_client.h"

extern man_sandbox *sandbox;

struct manconv_codes {
	gl_list_t from;
	char *to;
};

static void manconv_stdin (void *data)
{
	struct manconv_codes *codes = data;
	decompress *decomp;

	decomp = decompress_fdopen (dup (STDIN_FILENO));
	decompress_start (decomp);
	manconv (decomp, codes->from, codes->to);
	decompress_wait (decomp);
	decompress_free (decomp);
}

static void manconv_pre_exec (void *data)
{
	/* We must drop privileges before loading the sandbox, since our
	 * seccomp filter doesn't allow setresuid and friends.
	 */
	drop_privs (NULL);
	sandbox_load (data);
}

static void free_manconv_codes (void *data)
{
	struct manconv_codes *codes = data;

	gl_list_free (codes->from);
	free (codes->to);
	free (codes);
}

void add_manconv (pipeline *p, const char *source, const char *target)
{
	struct manconv_codes *codes;
	char *name;
	pipecmd *cmd;

	if (STREQ (source, "UTF-8") && STREQ (target, "UTF-8"))
		return;

	codes = xmalloc (sizeof *codes);
	/* informational only; no shell quoting concerns */
	name = xasprintf ("%s -f ", MANCONV);
	codes->from = new_string_list (GL_ARRAY_LIST, true);
	if (STREQ (source, "UTF-8")) {
		gl_list_add_last (codes->from, xstrdup (source));
		name = appendstr (name, source, (void *) 0);
	} else {
		gl_list_add_last (codes->from, xstrdup ("UTF-8"));
		gl_list_add_last (codes->from, xstrdup (source));
		name = appendstr (name, "UTF-8:", source, (void *) 0);
	}
	codes->to = xasprintf ("%s//IGNORE", target);
	/* informational only; no shell quoting concerns */
	name = appendstr (name, " -t ", codes->to, (void *) 0);
	if (quiet >= 2)
		name = appendstr (name, " -q", (void *) 0);

	/* iconv_open may not work correctly in setuid processes; in GNU
	 * libc, gconv modules may be linked against other gconv modules and
	 * rely on RPATH $ORIGIN to load those modules from the correct
	 * path, but $ORIGIN is disabled in setuid processes.  It is
	 * impossible to reset libc's idea of setuidness without creating a
	 * whole new process image.  Therefore, if the calling process is
	 * setuid, we must drop privileges and execute manconv.
	 */
	if (running_setuid ()) {
		gl_list_t from = codes->from;
		const char *from_code;
		char *sources = NULL;

		cmd = pipecmd_new_args (MANCONV, "-f", (void *) 0);
		GL_LIST_FOREACH (from, from_code) {
			sources = appendstr (sources, from_code, (void *) 0);
			if (gl_list_next_node (from, from_node))
				sources = appendstr (sources, ":", (void *) 0);
		}
		pipecmd_arg (cmd, sources);
		free (sources);
		pipecmd_args (cmd, "-t", codes->to, (void *) 0);
		if (quiet >= 2)
			pipecmd_arg (cmd, "-q");
		pipecmd_pre_exec (cmd, manconv_pre_exec, sandbox_free,
				  sandbox);
		free_manconv_codes (codes);
	} else {
		cmd = pipecmd_new_function (name, &manconv_stdin,
					    &free_manconv_codes, codes);
		pipecmd_pre_exec (cmd, sandbox_load, sandbox_free, sandbox);
	}
	free (name);
	pipeline_command (p, cmd);
}
