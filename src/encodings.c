/*
 * encodings.c: locale and encoding handling for man
 *
 * Copyright (C) 2003 Colin Watson.
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#if defined(STDC_HEADERS)
#  include <string.h>
#  include <stdlib.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#endif /* STDC_HEADERS */

#include <locale.h>
#include <libintl.h>
#ifdef HAVE_LANGINFO_CODESET
#  include <langinfo.h>
#endif

#include "manconfig.h"
#include "encodings.h"


/* Due to historical limitations in groff (which may be removed in the
 * future), there is no mechanism for a man page to specify its own
 * encoding. This means that each national language directory needs to carry
 * with it information about its encoding, and each groff device needs to
 * have a default encoding associated with it. Out of the box, groff
 * formally allows only ISO-8859-1 on input; however, patches originating
 * with Debian and imported by many other GNU/Linux distributions change
 * this somewhat.
 *
 * Eventually, groff will support proper Unicode input, and much of this
 * horror can go away.
 *
 * Do *not* confuse source encoding with groff encoding. The encoding
 * specified in this table is the encoding in which the source man pages in
 * each language directory are expected to be written. The groff encoding is
 * determined by the selected groff device and sometimes also by the user's
 * locale.
 *
 * This table is expected to change over time, particularly as man pages
 * begin to move towards UTF-8. Feel free to patch this for your
 * distribution; send me updates for languages I've missed.
 *
 * Explicit encodings in the directory name (e.g. de_DE.UTF-8) override this
 * table. TODO: Implement this.
 */
static struct {
	const char *lang_dir;
	const char *source_encoding;
} directory_table[] = {
	{ "C",		"ISO-8859-1"	}, /* English */
	{ "POSIX",	"ISO-8859-1"	}, /* English */
	{ "da",		"ISO-8859-1"	}, /* Danish */
	{ "de",		"ISO-8859-1"	}, /* German */
	{ "en",		"ISO-8859-1"	}, /* English */
	{ "es",		"ISO-8859-1"	}, /* Spanish */
	{ "fi",		"ISO-8859-1"	}, /* Finnish */
	{ "fr",		"ISO-8859-1"	}, /* French */
	{ "ga",		"ISO-8859-1"	}, /* Irish */
	{ "is",		"ISO-8859-1"	}, /* Icelandic */
	{ "it",		"ISO-8859-1"	}, /* Italian */
	{ "nl",		"ISO-8859-1"	}, /* Dutch */
	{ "no",		"ISO-8859-1"	}, /* Norwegian */
	{ "pt",		"ISO-8859-1"	}, /* Portuguese */
	{ "sv",		"ISO-8859-1"	}, /* Swedish */

#ifdef MULTIBYTE_GROFF
	/* These languages require a patched version of groff with the
	 * ascii8 and nippon devices.
	 */
	{ "cs",		"ISO-8859-2"	}, /* Czech */
	{ "hu",		"ISO-8859-2"	}, /* Hungarian */
	{ "ja",		"EUC-JP"	}, /* Japanese */
	{ "ko",		"EUC-KR"	}, /* Korean */
	{ "pl",		"ISO-8859-2"	}, /* Polish */
	{ "ru",		"KOI8-R"	}, /* Russian */
#endif /* MULTIBYTE_GROFF */

	{ NULL,		NULL		} };

/* The default groff terminal output device to be used is determined based
 * on nl_langinfo(CODESET), which returns the character set used by the
 * current locale.
 */
static struct {
	const char *locale_charset;
	const char *default_device;
} charset_table[] = {
	{ "ANSI_X3.4-1968",	"ascii"		},
	{ "ISO-8859-1",		"latin1"	},
	{ "UTF-8",		"utf8"		},

#ifdef MULTIBYTE_GROFF
	{ "EUC-JP",		"nippon"	},
#endif /* MULTIBYTE_GROFF */

	{ NULL,			NULL		} };

static const char *fallback_locale_charset = "ANSI_X3.4-1968";
static const char *fallback_default_device =
#ifdef MULTIBYTE_GROFF
	"ascii8"
#else /* !MULTIBYTE_GROFF */
	"ascii"
#endif /* MULTIBYTE_GROFF */
	;

/* The encoding used for the text passed to groff is a function of the
 * selected groff device. Traditional devices expect ISO-8859-1 on input
 * (yes, even the utf8 device); devices added in the Debian multibyte patch
 * expect other encodings. The ascii8 device passes top-bit-set characters
 * straight through so is (probably ...) encoding-agnostic. If this encoding
 * does not match the source encoding, an iconv pipe is used (if available)
 * to perform recoding.
 *
 * Setting less_charset to latin1 tells the less pager that characters
 * between 0xA0 and 0xFF are displayable, not that its input is encoded in
 * ISO-8859-1. TODO: Perhaps using LESSCHARDEF would be better.
 */
static struct {
	const char *roff_device;
	const char *roff_encoding;
	const char *less_charset;
} device_table[] = {
	{ "ascii",	"ISO-8859-1",	"ascii"		},
	{ "latin1",	"ISO-8859-1",	"latin1"	},
	{ "utf8",	"ISO-8859-1",	"utf-8"		},

#ifdef MULTIBYTE_GROFF
	{ "ascii8",	NULL,		"latin1"	},
	{ "nippon",	"EUC-JP",	"ja"		},
#endif /* MULTIBYTE_GROFF */

	{ NULL,		NULL,		NULL		} };

static const char *fallback_roff_encoding = "ISO-8859-1";
static const char *fallback_less_charset = "latin1";


/* Return the assumed encoding of the source man page, based on the
 * directory in which it was found. The caller should attempt to recode from
 * this to whatever encoding is expected by groff.
 */
const char *get_source_encoding (const char *lang)
{
	int i;

	if (!lang)
		return NULL;

	for (i = 0; directory_table[i].lang_dir; ++i) {
		if (STRNEQ (directory_table[i].lang_dir, lang,
			    strlen (directory_table[i].lang_dir)))
			return directory_table[i].source_encoding;
	}

	return NULL;
}

/* Return the default groff device for the current locale. This may be
 * overridden by the user.
 */
const char *get_default_device (const char *selected_locale)
{
	const char *ctype;
	const char *charset;
	int i;

	/* selected_locale may be NULL, in which case we look in the
	 * environment.
	 */
	ctype = setlocale (LC_CTYPE, selected_locale);
#ifdef HAVE_LANGINFO_CODESET
	charset = nl_langinfo (CODESET);
#else
	/* If the libc can't tell us, then try to guess from LC_CTYPE. */
	charset = strchr (ctype, '.');
	if (charset)
		++charset;
#endif

	if (!charset || !*charset)
		/* Oh dear. Make the safest choice we can. */
		charset = fallback_locale_charset;

	for (i = 0; charset_table[i].locale_charset; ++i) {
		if (STREQ (charset_table[i].locale_charset, charset))
			return charset_table[i].default_device;
	}

	return fallback_default_device;
}

/* Find the input encoding expected by groff, and set the LESSCHARSET
 * environment variable appropriately. If this returns NULL, then the device
 * claims to be encoding-agnostic and no recoding should take place.
 */
const char *get_roff_encoding (const char *device)
{
	int i;
	int found = 0;
	const char *roff_encoding = NULL, *less_charset = NULL;

	for (i = 0; device_table[i].roff_device; ++i) {
		if (STREQ (device_table[i].roff_device, device)) {
			found = 1;
			roff_encoding = device_table[i].roff_encoding;
			less_charset = device_table[i].less_charset;
			break;
		}
	}

	if (!found) {
		less_charset = fallback_less_charset;
		roff_encoding = fallback_roff_encoding;
	}

#ifdef MULTIBYTE_GROFF
	/* An ugly special case is needed here. The utf8 device normally
	 * takes ISO-8859-1 input. However, with the multibyte patch, when
	 * recoding from EUC-JP it takes UTF-8 input instead. This is evil,
	 * but there's not much that can be done about it apart from waiting
	 * for groff 2.0.
	 */
	if (STREQ (device, "utf8")) {
		const char *ctype = setlocale (LC_CTYPE, NULL);
		if (STREQ (ctype, "ja_JP.UTF-8"))
			roff_encoding = "UTF-8";
	}
#endif /* MULTIBYTE_GROFF */

	putenv (strappend (NULL, "LESSCHARSET=", less_charset, NULL));
	return roff_encoding;
}
