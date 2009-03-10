/*
 * manconv.c: convert manual page from one encoding to another
 *
 * Copyright (C) 2007, 2008 Colin Watson.
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

#ifdef HAVE_ICONV
#  include <iconv.h>
#endif /* HAVE_ICONV */

#include "argp.h"

#include "gettext.h"
#include <locale.h>
#define _(String) gettext (String)

#include "manconfig.h"

#include "error.h"
#include "pipeline.h"

#include "encodings.h"
#include "manconv.h"

#ifdef HAVE_ICONV

static int try_iconv (pipeline *p, const char *try_from_code, const char *to,
		      int last)
{
	char *try_to_code = xstrdup (to);
	static const size_t buf_size = 65536;
	size_t input_size = buf_size;
	const char *input;
	static char *output = NULL;
	iconv_t cd;
	int ret = 0;

	/* Only handle //IGNORE for the last encoding. */
	if (!last) {
		char *ignore = strstr (try_to_code, "//IGNORE");
		if (ignore)
			*ignore = '\0';
	}
	debug ("trying encoding %s -> %s\n", try_from_code, try_to_code);

	cd = iconv_open (try_to_code, try_from_code);
	if (cd == (iconv_t) -1) {
		error (0, errno, "iconv_open (\"%s\", \"%s\")",
		       try_to_code, try_from_code);
		free (try_to_code);
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
		    errno == EILSEQ && strstr (try_to_code, "//IGNORE"))
			errno = 0;

		/* If we need to try the next encoding, do that before
		 * writing anything.
		 */
		if (!last && n == (size_t) -1 &&
		    ((errno == EILSEQ && !strstr (try_to_code, "//IGNORE")) ||
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
			if (errno == EILSEQ &&
			    !strstr (try_to_code, "//IGNORE")) {
				if (!quiet)
					error (0, errno, "iconv");
				exit (FATAL);
			} else if (errno == EINVAL && input_size < buf_size) {
				if (!quiet)
					error (FATAL, 0,
					       _("iconv: incomplete character "
						 "at end of buffer"));
				exit (FATAL);
			}
		}

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
	free (try_to_code);

	return ret;
}

void manconv (pipeline *p, char * const *from, const char *to)
{
	char *pp_encoding;
	char * const *try_from_code;

	pp_encoding = check_preprocessor_encoding (p);
	if (pp_encoding) {
		try_iconv (p, pp_encoding, to, 1);
		free (pp_encoding);
	} else {
		for (try_from_code = from; *try_from_code; ++try_from_code)
			if (try_iconv (p, *try_from_code, to,
				       !*(try_from_code + 1)) == 0)
				break;
	}
}

#else /* !HAVE_ICONV */

/* If we don't have iconv, there isn't much we can do; just pass everything
 * through unchanged.
 */
void manconv (pipeline *p, char * const *from ATTRIBUTE_UNUSED,
	      const char *to ATTRIBUTE_UNUSED)
{
	for (;;) {
		size_t len = 4096;
		const char *buffer = pipeline_read (p, &len);
		if (len == 0)
			break;
		if (fwrite (buffer, 1, len, stdout) < len || ferror (stdout))
			error (FATAL, 0, _("can't write to standard output"));
	}
}

#endif /* HAVE_ICONV */
