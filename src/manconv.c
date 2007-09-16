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

#if defined(STDC_HEADERS)
#  include <stdlib.h>
#  include <string.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#endif

#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_ICONV
#  include <iconv.h>
#endif /* HAVE_ICONV */

#ifdef HAVE_LIBGEN_H
#  include <libgen.h>
#endif /* HAVE_LIBGEN_H */

#ifdef HAVE_GETOPT_H
#  include <getopt.h>
#else /* !HAVE_GETOPT_H */
#  include "lib/getopt.h"
#endif /* HAVE_GETOPT_H */

#include "lib/gettext.h"
#include <locale.h>
#define _(String) gettext (String)

#include "manconfig.h"
#include "lib/error.h"
#include "lib/pipeline.h"
#include "lib/decompress.h"
#include "encodings.h"

char *program_name;
static const char *from_codes;
static char *to_code;
static char **from_code;

extern int optind;

static const struct option long_options[] =
{
	{"from-code",	required_argument,	0, 'f'},
	{"to-code",	required_argument,	0, 't'},
	{"debug",	no_argument,		0, 'd'},
	{"help",	no_argument,		0, 'h'},
	{"version",	no_argument,		0, 'V'},
	{0, 0, 0, 0}
};

static const char args[] = "f:t:dhV";

static void usage (int status)
{
	printf (_("usage: %s -f FROM-CODE:... -t TO-CODE [-d] [filename]\n"),
		program_name);
	printf (_(
		"-f, --from-code     possible encodings of original text.\n"
		"-t, --to-code       encoding for output.\n"
		"-d, --debug         emit debugging messages.\n"
		"-V, --version       show version.\n"
		"-h, --help          show this usage message.\n")
	);

	exit (status);
}

static char **split_codes (const char *codestr)
{
	char *codestrtok = xstrdup (codestr);
	char *codestrtok_ptr = codestrtok;
	char *tok;
	int codearray_cur = 0, codearray_alloc = 4;
	char **codearray = xmalloc (codearray_alloc * sizeof *codearray);

	for (tok = strsep (&codestrtok_ptr, ":"); tok;
	     tok = strsep (&codestrtok_ptr, ":")) {
		if (!*tok)
			continue;	/* ignore empty fields */
		if (codearray_cur >= codearray_alloc) {
			codearray_alloc <<= 1;
			codearray = xrealloc
				(codearray,
				 codearray_alloc * sizeof *codearray);
		}
		codearray[codearray_cur++] = xstrdup (tok);
	}

	if (codearray_cur >= codearray_alloc)
		codearray = xrealloc (codearray,
				      (++codearray_alloc) * sizeof *codearray);
	codearray[codearray_cur] = NULL;

	free (codestrtok);

	return codearray;
}

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
			const char *pp_encoding_end;
			pp_search += 7;
			while (*pp_search == ' ')
				++pp_search;
			pp_encoding_end = strchr (pp_search, ' ');
			if (pp_encoding_end)
				pp_encoding = xstrndup
					(pp_search,
					 pp_encoding_end - pp_search);
			else
				pp_encoding = xstrdup (pp_search);
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

	debug ("trying encoding %s\n", try_from_code);

	cd = iconv_open (to_code, try_from_code);
	if (cd == (iconv_t) -1) {
		error (0, errno, "iconv_open");
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
			if (errno == EILSEQ && !strstr (to_code, "//IGNORE")) {
				if (last)
					error (FATAL, errno, "iconv");
				else {
					ret = -1;
					break;
				}
			} else if (errno == EINVAL && input_size < buf_size) {
				if (last)
					error (FATAL, 0,
					       _("iconv: incomplete character "
						 "at end of buffer"));
				else {
					ret = -1;
					break;
				}
			}
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
	int c;
	pipeline *p;
	char *pp_encoding;
	char **try_from_code;

	program_name = xstrdup (basename (argv[0]));

	if (!setlocale (LC_ALL, ""))
		/* Obviously can't translate this. */
		error (0, 0, "can't set the locale; make sure $LC_* and $LANG "
			     "are correct");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);

	while ((c = getopt_long (argc, argv, args,
				 long_options, NULL)) != EOF) {
		switch (c) {
			case 'f':
				from_codes = optarg;
				break;
			case 't':
				to_code = xstrdup (optarg);
				break;
			case 'd':
				debug_level = 1;
				break;
			case 'V':
				ver ();
				break;
			case 'h':
				usage (OK);
				break;
			default:
				usage (FAIL);
				break;
		}
	}

	if (!from_codes) {
		error (0, 0, _("must specify an input encoding"));
		usage (FAIL);
	}
	if (!to_code) {
		error (0, 0, _("must specify an output encoding"));
		usage (FAIL);
	}

	from_code = split_codes (from_codes);
	if (!from_code || !*from_code) {
		error (0, 0, _("must specify an input encoding"));
		usage (FAIL);
	}
	if (!strstr (to_code, "//"))
		to_code = strappend (to_code, "//TRANSLIT", NULL);

	pipeline_install_sigchld ();

	if (optind == argc)
		p = decompress_fdopen (dup (fileno (stdin)));
	else {
		p = decompress_open (argv[optind]);
		if (!p)
			error (FAIL, 0, _("can't open %s"), argv[optind]);
	}
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
