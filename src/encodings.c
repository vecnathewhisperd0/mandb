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
 * The standard output encoding is the encoding assumed for cat pages for
 * each language directory. It must *not* be used to discover the actual
 * output encoding displayed to the user; that is determined by the locale.
 * TODO: it would be useful to be able to change the standard output
 * encoding in the configuration file.
 *
 * This table is expected to change over time, particularly as man pages
 * begin to move towards UTF-8. Feel free to patch this for your
 * distribution; send me updates for languages I've missed.
 *
 * Explicit encodings in the directory name (e.g. de_DE.UTF-8) override this
 * table.
 */
struct directory_entry {
	const char *lang_dir;
	const char *source_encoding;
	const char *standard_output_encoding;
};

static struct directory_entry directory_table[] = {
	{ "C",		"ISO-8859-1",	"ANSI_X3.4-1968"	}, /* English */
	{ "POSIX",	"ISO-8859-1",	"ANSI_X3.4-1968"	}, /* English */
	{ "da",		"ISO-8859-1",	"ISO-8859-1"		}, /* Danish */
	{ "de",		"ISO-8859-1",	"ISO-8859-1"		}, /* German */
	{ "en",		"ISO-8859-1",	"ISO-8859-1"		}, /* English */
	{ "es",		"ISO-8859-1",	"ISO-8859-1"		}, /* Spanish */
	{ "fi",		"ISO-8859-1",	"ISO-8859-1"		}, /* Finnish */
	{ "fr",		"ISO-8859-1",	"ISO-8859-1"		}, /* French */
	{ "ga",		"ISO-8859-1",	"ISO-8859-1"		}, /* Irish */
	{ "gl",		"ISO-8859-1",	"ISO-8859-1"		}, /* Galician */
	{ "id",		"ISO-8859-1",	"ISO-8859-1"		}, /* Indonesian */
	{ "is",		"ISO-8859-1",	"ISO-8859-1"		}, /* Icelandic */
	{ "it",		"ISO-8859-1",	"ISO-8859-1"		}, /* Italian */
	{ "nl",		"ISO-8859-1",	"ISO-8859-1"		}, /* Dutch */
	{ "no",		"ISO-8859-1",	"ISO-8859-1"		}, /* Norwegian */
	{ "pt",		"ISO-8859-1",	"ISO-8859-1"		}, /* Portuguese */
	{ "sv",		"ISO-8859-1",	"ISO-8859-1"		}, /* Swedish */

#ifdef MULTIBYTE_GROFF
	/* These languages require a patched version of groff with the
	 * ascii8 and nippon devices.
	 */
	{ "cs",		"ISO-8859-2",	"ISO-8859-2"		}, /* Czech */
	{ "hr",		"ISO-8859-2",	"ISO-8859-2"		}, /* Croatian */
	{ "hu",		"ISO-8859-2",	"ISO-8859-2"		}, /* Hungarian */
	{ "ja",		"EUC-JP",	"EUC-JP"		}, /* Japanese */
	{ "ko",		"EUC-KR",	"EUC-KR"		}, /* Korean */
	{ "pl",		"ISO-8859-2",	"ISO-8859-2"		}, /* Polish */
	{ "ru",		"KOI8-R",	"KOI8-R"		}, /* Russian */
	{ "sk",		"ISO-8859-2",	"ISO-8859-2"		}, /* Slovak */
	{ "tr",		"ISO-8859-9",	"ISO-8859-9"		}, /* Turkish */
#endif /* MULTIBYTE_GROFF */

	{ NULL,		NULL,		"NULL"			}
};

static const char *fallback_source_encoding = "ISO-8859-1";

/* The default groff terminal output device to be used is determined based
 * on nl_langinfo(CODESET), which returns the character set used by the
 * current locale.
 */
struct charset_entry {
	const char *locale_charset;
	const char *default_device;
};

static struct charset_entry charset_table[] = {
	{ "ANSI_X3.4-1968",	"ascii"		},
	{ "ISO-8859-1",		"latin1"	},
	{ "ISO-8859-15",    	"latin1"	},
	{ "UTF-8",		"utf8"		},

#ifdef MULTIBYTE_GROFF
	{ "EUC-JP",		"nippon"	},
#endif /* MULTIBYTE_GROFF */

	{ NULL,			NULL		}
};

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
 */
struct device_entry {
	const char *roff_device;
	const char *roff_encoding;
	const char *output_encoding;
};

static struct device_entry device_table[] = {
	{ "ascii",	"ISO-8859-1",	"ANSI_X3.4-1968"	},
	{ "latin1",	"ISO-8859-1",	"ISO-8859-1"		},
	{ "utf8",	"ISO-8859-1",	"UTF-8"			},

#ifdef MULTIBYTE_GROFF
	{ "ascii8",	NULL,		NULL			},
	{ "nippon",	"EUC-JP",	"EUC-JP"		},
#endif /* MULTIBYTE_GROFF */

	{ NULL,		NULL,		NULL			}
};

static const char *fallback_roff_encoding = "ISO-8859-1";

/* Setting less_charset to iso8859 tells the less pager that characters
 * between 0xA0 and 0xFF are displayable, not that its input is encoded in
 * ISO-8859-*. TODO: Perhaps using LESSCHARDEF would be better.
 */
struct less_charset_entry {
	const char *locale_charset;
	const char *less_charset;
};

static struct less_charset_entry less_charset_table[] = {
	{ "ANSI_X3.4-1968",	"ascii"		},
	{ "ISO-8859-1",		"iso8859"	},
	{ "UTF-8",		"utf-8"		},

#ifdef MULTIBYTE_GROFF
	{ "EUC-JP",		"ja"		},
	{ "KOI8-R",		"koi8-r"	},
#endif /* MULTIBYTE_GROFF */

	{ NULL,			NULL		}
};

static const char *fallback_less_charset = "iso8859";


/* Return the assumed encoding of the source man page, based on the
 * directory in which it was found. The caller should attempt to recode from
 * this to whatever encoding is expected by groff.
 *
 * The caller should free the returned string when it is finished with it.
 */
char *get_page_encoding (const char *lang)
{
	const struct directory_entry *entry;
	const char *dot;

	if (!lang || !*lang) {
		/* Guess based on the locale. */
		lang = setlocale (LC_MESSAGES, NULL);
		if (!lang)
			return xstrdup (fallback_source_encoding);
	}

	dot = strchr (lang, '.');
	if (dot)
		/* TODO: The FHS has the worst specification of what's
		 * supposed to go after the dot here that I've ever seen. To
		 * quote from version 2.1:
		 *
		 * "It is recommended that this be a numeric representation
		 * if possible (ISO standards, especially), not include
		 * additional punctuation symbols, and that any letters be
		 * in lowercase."
		 *
		 * Any sane standard would use directory names like
		 * de_DE.ISO-8859-1; the examples in the FHS recommend
		 * de_DE.88591 instead. Considering that there is no other
		 * conceivable use for encodings in directory names other
		 * than to pass them to iconv or similar, this is quite
		 * startlingly useless.
		 *
		 * My plan is to ignore the current FHS specification on the
		 * grounds that it's obviously wrong, and petition to have
		 * it changed.
		 */
		return xstrndup (dot + 1, strcspn (dot + 1, ",@"));

	for (entry = directory_table; entry->lang_dir; ++entry)
		if (STRNEQ (entry->lang_dir, lang, strlen (entry->lang_dir)))
			return xstrdup (entry->source_encoding);

	return xstrdup (fallback_source_encoding);
}

/* Return the canonical encoding for source man pages in the specified
 * language. This ignores any encoding specification in the language
 * directory name. The source encoding should be used as a basis for
 * determining the correct roff device to use: that is, the caller should
 * behave as if it is recoding from the page encoding to the source encoding
 * first, although in practice it should recode directly from the page
 * encoding to the roff encoding.
 *
 * Here are a few concrete examples of why these distinctions are important:
 *
 *   /usr/share/man/en_GB.UTF-8, locale C
 *     page encoding = UTF-8
 *     source encoding = ISO-8859-1
 *     roff encoding = ISO-8859-1
 *     output encoding = UTF-8
 *     UTF-8 -> iconv -> ISO-8859-1 -> groff -Tascii -> ANSI_X3.4-1968
 *
 *   /usr/share/man/pl_PL.UTF-8, locale pl_PL.UTF-8
 *     page encoding = UTF-8
 *     source encoding = ISO-8859-2
 *     roff encoding = ISO-8859-2
 *     output encoding = ISO-8859-2
 *     UTF-8 -> iconv -> ISO-8859-2 -> groff -Tascii8
 *                    -> ISO-8859-2 -> iconv -> UTF-8
 *
 *   /usr/share/man/ja_JP.EUC-JP, locale ja_JP.UTF-8
 *     page encoding = EUC-JP
 *     source encoding = EUC-JP
 *     roff encoding = UTF-8
 *     output encoding = UTF-8
 *     EUC-JP -> iconv -> UTF-8 -> groff -Tutf8 -> UTF-8
 */
const char *get_source_encoding (const char *lang)
{
	const struct directory_entry *entry;

	if (!lang || !*lang) {
		/* Guess based on the locale. */
		lang = setlocale (LC_MESSAGES, NULL);
		if (!lang)
			return fallback_source_encoding;
	}

	for (entry = directory_table; entry->lang_dir; ++entry)
		if (STRNEQ (entry->lang_dir, lang, strlen (entry->lang_dir)))
			return entry->source_encoding;

	return fallback_source_encoding;
}

/* Return the standard output encoding for the source man page, based on the
 * directory in which it was found. This should only be used to determine
 * whether a cat page can be saved.
 *
 * The caller should free the returned string when it is finished with it.
 */
char *get_standard_output_encoding (const char *lang)
{
	const struct directory_entry *entry;
	const char *dot;

	if (!lang || !*lang) {
		/* Guess based on the locale. */
		lang = setlocale (LC_MESSAGES, NULL);
		if (!lang)
			return NULL;
	}

	dot = strchr (lang, '.');
	if (dot)
		/* The cat directory will have a corresponding name to the
		 * man directory including an explicit character set, so the
		 * pages it contains should have that encoding.
		 */
		return xstrndup (dot + 1, strcspn (dot + 1, ",@"));

	for (entry = directory_table; entry->lang_dir; ++entry)
		if (STRNEQ (entry->lang_dir, lang, strlen (entry->lang_dir)))
			return xstrdup (entry->standard_output_encoding);

	return NULL;
}

/* Return the current locale's character set. */
const char *get_locale_charset (void)
{
	const char *ctype;
	const char *charset;
	char *saved_locale;

	/* We need to modify LC_CTYPE temporarily in order to look at the
	 * codeset, so save it first.
	 */
	saved_locale = xstrdup (setlocale (LC_CTYPE, NULL));

	ctype = setlocale (LC_CTYPE, "");

#ifdef HAVE_LANGINFO_CODESET
	charset = nl_langinfo (CODESET);
#else
	/* If the libc can't tell us, then try to guess from LC_CTYPE. */
	charset = strchr (ctype, '.');
	if (charset)
		++charset;
#endif

	/* Restore LC_CTYPE to its value on entry to this function. */
	setlocale (LC_CTYPE, saved_locale);

	if (charset && *charset)
		return charset;
	else
		return NULL;
}

/* Can we take this input encoding and produce this output encoding, perhaps
 * with the help of some iconv pipes? */
static int compatible_encodings (const char *input, const char *output)
{
	if (STREQ (input, output))
		return 1;

	/* If the input is ASCII, recoding should be easy. Try it. */
	if (STREQ (input, "ANSI_X3.4-1968"))
		return 1;

	/* If the input is UTF-8, it's either a simple recoding of whatever
	 * we want or else it probably won't work at all no matter what we
	 * do. We might as well try it for now.
	 */
	if (STREQ (input, "UTF-8"))
		return 1;

#ifdef MULTIBYTE_GROFF
	/* Special case for ja_JP.UTF-8, which takes UTF-8 input recoded
	 * from EUC-JP and produces UTF-8 output. This is rather filthy.
	 */
	if (STREQ (input, "EUC-JP") && STREQ (output, "UTF-8"))
		return 1;
#endif /* MULTIBYTE_GROFF */

	return 0;
}

/* Return the default groff device for the given character set. This may be
 * overridden by the user. The page's source encoding is needed to ensure
 * that the device is compatible: consider ru_RU.UTF-8, which needs ascii8
 * and a trailing iconv pipe to recode to UTF-8.
 *
 * All this encoding compatibility stuff feels like a slightly nasty hack,
 * but I haven't yet come up with a cleaner way to do it.
 */
const char *get_default_device (const char *locale_charset,
				const char *source_encoding)
{
	const struct charset_entry *entry;

	if (!locale_charset)
		return fallback_default_device;

	for (entry = charset_table; entry->locale_charset; ++entry) {
		if (STREQ (entry->locale_charset, locale_charset)) {
			const char *roff_encoding =
				get_roff_encoding (entry->default_device,
						   source_encoding);
			if (compatible_encodings (source_encoding,
						  roff_encoding))
				return entry->default_device;
		}
	}

	return fallback_default_device;
}

/* Find the input encoding expected by groff, and set the LESSCHARSET
 * environment variable appropriately.
 */
const char *get_roff_encoding (const char *device, const char *source_encoding)
{
	const struct device_entry *entry;
	int found = 0;
	const char *roff_encoding = NULL;

	for (entry = device_table; entry->roff_device; ++entry) {
		if (STREQ (entry->roff_device, device)) {
			found = 1;
			roff_encoding = entry->roff_encoding;
			break;
		}
	}

	if (!found)
		roff_encoding = fallback_roff_encoding;

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

	return roff_encoding ? roff_encoding : source_encoding;
}

/* Find the output encoding that this device will produce, or NULL if it
 * will simply pass through the input encoding.
 */
const char *get_output_encoding (const char *device)
{
	const struct device_entry *entry;

	for (entry = device_table; entry->roff_device; ++entry)
		if (STREQ (entry->roff_device, device))
			return entry->output_encoding;

	return NULL;
}

/* Return the value of LESSCHARSET appropriate for this locale. */
const char *get_less_charset (const char *locale_charset)
{
	const struct less_charset_entry *entry;

	for (entry = less_charset_table; entry->locale_charset; ++entry)
		if (STREQ (entry->locale_charset, locale_charset))
			return entry->less_charset;

	return fallback_less_charset;
}
