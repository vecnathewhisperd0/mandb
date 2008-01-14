/*
 * manconv.c: convert manual page from one encoding to another
 *
 * Copyright (C) 2007 Colin Watson.
 * Based loosely on parts of glibc's iconv_prog.c, which is:
 * Copyright (C) 1998-2004, 2005, 2006, 2007 Free Software Foundation, Inc.
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

/* This program arose during a discussion with Adam Borowski. See:
 *   http://lists.debian.org/debian-mentors/2007/09/msg00245.html
 * It behaves like iconv, but allows multiple source encodings and
 * attempts to guess the first one that works. An Emacs-style
 * "-*- coding:" declaration overrides this.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_ICONV
#  include <iconv.h>
#endif /* HAVE_ICONV */

#include "argp.h"
#include "dirname.h"

#include "gettext.h"
#include <locale.h>
#define _(String) gettext (String)
#define N_(String) gettext_noop (String)

#include "manconfig.h"

#include "error.h"
#include "pipeline.h"
#include "decompress.h"

#include "encodings.h"

char *program_name;
static const char *from_codes;
static char *to_code;
static char **from_code;
static const char *filename;

static char **split_codes (const char *codestr)
{
	char *codestrtok = xstrdup (codestr);
	char *codestrtok_ptr = codestrtok;
	char *tok;
	size_t codearray_cur = 0, codearray_alloc = 0;
	char **codearray = NULL;

	for (tok = strsep (&codestrtok_ptr, ":"); tok;
	     tok = strsep (&codestrtok_ptr, ":")) {
		if (!*tok)
			continue;	/* ignore empty fields */
		if (codearray_cur >= codearray_alloc)
			codearray = x2nrealloc
				(codearray,
				 &codearray_alloc, sizeof *codearray);
		codearray[codearray_cur++] = xstrdup (tok);
	}

	if (codearray_cur >= codearray_alloc)
		codearray = x2nrealloc (codearray,
					&codearray_alloc, sizeof *codearray);
	codearray[codearray_cur] = NULL;

	free (codestrtok);

	return codearray;
}

const char *argp_program_version = "manconv " PACKAGE_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;
error_t argp_err_exit_status = FAIL;

static const char args_doc[] = N_("-f CODE:... -t CODE [FILENAME]");

static struct argp_option options[] = {
	{ "from-code",	'f',	N_("CODE:..."),	0,	N_("possible encodings of original text") },
	{ "to-code",	't',	N_("CODE"),	0,	N_("encoding for output") },
	{ "debug",	'd',	0,		0,	N_("emit debugging messages") },
	{ 0, 'h', 0, OPTION_HIDDEN, 0 }, /* compatibility for --help */
	{ 0 }
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	switch (key) {
		case 'f':
			from_codes = arg;
			return 0;
		case 't':
			to_code = xstrdup (arg);
			if (!strstr (to_code, "//"))
				to_code = appendstr (to_code, "//TRANSLIT",
						     NULL);
			return 0;
		case 'd':
			debug_level = 1;
			return 0;
		case 'h':
			argp_state_help (state, state->out_stream,
					 ARGP_HELP_STD_HELP);
			break;
		case ARGP_KEY_ARG:
			if (filename)
				argp_usage (state);
			filename = arg;
			return 0;
		case ARGP_KEY_SUCCESS:
			if (!from_codes)
				argp_error (state,
					    _("must specify an input "
					      "encoding"));
			if (!to_code)
				argp_error (state,
					    _("must specify an output "
					      "encoding"));
			from_code = split_codes (from_codes);
			if (!from_code || !*from_code)
				argp_error (state,
					    _("must specify an input "
					      "encoding"));
			return 0;
	}
	return ARGP_ERR_UNKNOWN;
}

static struct argp argp = { options, parse_opt, args_doc };

static char *check_preprocessor_encoding (pipeline *p)
{
	char *pp_encoding = NULL;

#ifdef PP_COOKIE
	const char *line = pipeline_peekline (p);
	char *directive = NULL;

	/* Some people use .\" incorrectly. We allow it for encoding
	 * declarations but not for preprocessor declarations.
	 */
	if (line &&
	    (STRNEQ (line, PP_COOKIE, 4) || STRNEQ (line, ".\\\" ", 4))) {
		const char *newline = strchr (line, '\n');
		if (newline)
			directive = xstrndup (line + 4,
					      newline - (line + 4));
		else
			directive = xstrdup (line + 4);
	}

	if (directive && strstr (directive, "-*-")) {
		const char *pp_search = strstr (directive, "-*-") + 3;
		while (*pp_search == ' ')
			++pp_search;
		if (STRNEQ (pp_search, "coding:", 7)) {
			const char *pp_encoding_allow;
			size_t pp_encoding_len;
			pp_search += 7;
			while (*pp_search == ' ')
				++pp_search;
			pp_encoding_allow = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					    "abcdefghijklmnopqrstuvwxyz"
					    "0123456789-_/:.()";
			pp_encoding_len = strspn (pp_search,
						  pp_encoding_allow);
			pp_encoding = xstrndup (pp_search, pp_encoding_len);
			debug ("preprocessor encoding: %s\n", pp_encoding);
		}
	}
#endif /* PP_COOKIE */

	return pp_encoding;
}

static int try_iconv (pipeline *p, const char *try_from_code, int last)
{
	static const size_t buf_size = 65536;
	size_t input_size = buf_size;
	const char *input;
	static char *output = NULL;
	iconv_t cd;
	int ret = 0;

	debug ("trying encoding %s -> %s\n", try_from_code, to_code);

	cd = iconv_open (to_code, try_from_code);
	if (cd == (iconv_t) -1) {
		error (0, errno, "iconv_open (\"%s\", \"%s\")",
		       to_code, try_from_code);
		return -1;
	}

	input = pipeline_peek (p, &input_size);
	if (input_size < buf_size) {
		/* End of file, error, or just a short read? Repeat until we
		 * have either a full buffer or EOF/error.
		 */
		while (input_size < buf_size) {
			size_t old_input_size = input_size;
			input_size = buf_size;
			input = pipeline_peek (p, &input_size);
			if (input_size == old_input_size)
				break;
		}
	}

	if (!output)
		output = xmalloc (buf_size);

	while (input_size) {
		char *inptr = (char *) input, *outptr = output;
		size_t inleft = input_size, outleft = buf_size;
		size_t n;

		n = iconv (cd, (ICONV_CONST char **) &inptr, &inleft,
			   &outptr, &outleft);

		if (n == (size_t) -1 &&
		    errno == EILSEQ && strstr (to_code, "//IGNORE"))
			errno = 0;

		/* If we need to try the next encoding, do that before
		 * writing anything.
		 */
		if (!last && n == (size_t) -1 &&
		    ((errno == EILSEQ && !strstr (to_code, "//IGNORE")) ||
		     (errno == EINVAL && input_size < buf_size))) {
			ret = -1;
			break;
		}

		if (outptr != output) {
			/* We have something to write out. */
			int errno_save = errno;
			size_t w;
			w = fwrite (output, 1, buf_size - outleft, stdout);
			if (w < (size_t) (buf_size - outleft) ||
			    ferror (stdout))
				error (FATAL, 0, _("can't write to "
						   "standard output"));
			errno = errno_save;
		}

		if (inptr != input)
			pipeline_peek_skip (p, input_size - inleft);

		if (n != (size_t) -1) {
			/* All the input text is processed. For
			 * state-dependent character sets we have to flush
			 * the state now.
			 */
			outptr = output;
			outleft = buf_size;
			n = iconv (cd, NULL, NULL, &outptr, &outleft);

			if (outptr != output) {
				/* We have something to write out. */
				int errno_save = errno;
				size_t w;
				w = fwrite (output, 1, buf_size - outleft,
					    stdout);
				if (w < (size_t) (buf_size - outleft) ||
				    ferror (stdout))
					error (FATAL, 0, _("can't write to "
							   "standard output"));
				errno = errno_save;
			}
		} else {
			/* !last case handled above */
			if (errno == EILSEQ && !strstr (to_code, "//IGNORE"))
				error (FATAL, errno, "iconv");
			else if (errno == EINVAL && input_size < buf_size)
				error (FATAL, 0,
				       _("iconv: incomplete character "
					 "at end of buffer"));
		}

		if (input_size < buf_size)
			break;

		input_size = buf_size;
		input = pipeline_peek (p, &input_size);
		while (input_size < buf_size) {
			size_t old_input_size = input_size;
			input_size = buf_size;
			input = pipeline_peek (p, &input_size);
			if (input_size == old_input_size)
				break;
		}
	}

	iconv_close (cd);

	return ret;
}

int main (int argc, char *argv[])
{
	pipeline *p;
	char *pp_encoding;
	char **try_from_code;

	program_name = base_name (argv[0]);

	if (!setlocale (LC_ALL, ""))
		/* Obviously can't translate this. */
		error (0, 0, "can't set the locale; make sure $LC_* and $LANG "
			     "are correct");
	bindtextdomain (PACKAGE, LOCALEDIR);
	bindtextdomain (PACKAGE "-gnulib", LOCALEDIR);
	textdomain (PACKAGE);

	if (argp_parse (&argp, argc, argv, 0, 0, 0))
		exit (FAIL);

	pipeline_install_sigchld ();

	if (filename) {
		p = decompress_open (filename);
		if (!p)
			error (FAIL, 0, _("can't open %s"), argv[optind]);
	} else
		p = decompress_fdopen (dup (STDIN_FILENO));
	pipeline_start (p);

	pp_encoding = check_preprocessor_encoding (p);
	if (pp_encoding) {
		try_iconv (p, pp_encoding, 1);
		free (pp_encoding);
	} else {
		for (try_from_code = from_code; *try_from_code;
		     ++try_from_code)
			if (try_iconv (p, *try_from_code,
				       !*(try_from_code + 1)) == 0)
				break;
	}

	free (to_code);
	for (try_from_code = from_code; *try_from_code; ++try_from_code)
		free (*try_from_code);
	free (from_code);

	pipeline_wait (p);

	return 0;
}
