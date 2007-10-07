%{

/*
 * lexgrog.l: extract 'whatis' info from nroff man / formatted cat pages.
 *  
 * Copyright (C) 1994, 1995 Graeme W. Wilford. (Wilf.)
 * Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007 Colin Watson.
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
 * Wed Oct 12 18:46:11 BST 1994  Wilf. (G.Wilford@ee.surrey.ac.uk) 
 * 
 * CJW: Detect grap and vgrind. Understand fill requests. Other improvements
 * in the syntax accepted.
 */

#undef PROFILE

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/stat.h>
#include <errno.h>

#ifndef STDC_HEADERS
extern int errno;
#endif

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_STRING_H
#  include <string.h>
#elif defined (HAVE_STRINGS_H)
#  include <strings.h>
#endif /* HAVE_STRING_H */

#ifdef HAVE_LIBGEN_H
#  include <libgen.h>
#endif /* HAVE_LIBGEN_H */

#include "lib/gettext.h"
#define _(String) gettext (String)

#include "manconfig.h"
#include "lib/error.h"
#include "lib/pipeline.h"
#include "lib/decompress.h"
#include "descriptions.h"
#include "security.h"
#include "encodings.h"
#include "ult_src.h"

#define YY_READ_BUF_SIZE	1024
#define MAX_NAME		2048

#ifdef PROFILE
static int ctr[YY_NUM_RULES];
#  define YY_USER_ACTION ++ctr[yy_act];
#endif

static void add_str_to_whatis (const char *string, size_t length);
static void add_char_to_whatis (unsigned char c);
static void add_separator_to_whatis (void);
static void newline_found (void);

static char newname[MAX_NAME];
static char *p_name;
static const char *fname;
static char filters[MAX_FILTERS];

static int fill_mode;
static int waiting_for_quote;

static pipeline *decomp;

#define YY_INPUT(buf,result,max_size) { \
	size_t size = max_size; \
	const char *block = pipeline_read (decomp, &size); \
	if (block && size != 0) { \
		memcpy (buf, block, size); \
		buf[size] = '\0'; \
		result = size; \
	} else \
		result = YY_NULL; \
}

#ifdef TEST

#include <stdio.h>
#ifdef HAVE_GETOPT_H
#  include <getopt.h>
#else /* !HAVE_GETOPT_H */
#  include "lib/getopt.h"
#endif /* HAVE_GETOPT_H */

char *program_name;
int quiet = 1;

static const struct option long_options[] =
{
	{"man",		no_argument,		0,	'm'},
	{"cat",		no_argument,		0,	'c'},
	{"whatis",	no_argument,		0,	'w'},
	{"filters",	no_argument,		0,	'f'},
	{"encoding",	required_argument,	0,	'E'},
	{"help",	no_argument,		0,	'h'},
	{"version",	no_argument,		0,	'V'},
	{0, 0, 0, 0}
};

static const char args[] = "mcwfE:hV";

#endif /* TEST */
%}

%option ecs meta-ecs
%option 8bit batch caseful never-interactive 
%option nostdinit
%option warn
%option noyywrap nounput

%x MAN_PRENAME
%x MAN_NAME
%x CAT_NAME
%x CAT_FILE
%x MAN_FILE
%x CAT_REST
%x MAN_REST
%x FORCE_EXIT

digit		[[:digit:]]
upper		[[:upper:]]
alpha		[[:alpha:]]
blank		[[:blank:]]
blank_eol	[[:blank:]\r\n]
eol		\r?\n
bol		{eol}+
next		{eol}*
empty		{eol}{blank}*
indent		{eol}{blank}+
dbl_quote	\"
font_change	\\f([[:upper:]1-4]|\({upper}{2})
size_change	\\s[+-]?{digit}
style_change	({font_change}{size_change}?|{size_change}{font_change}?)
typeface	\.(B[IR]?|I[BR]?|R[BI])
sec_request	\.[Ss][HhYySs]
comment		['.]\\{dbl_quote}

 /* Please add to this list if you know how. */
 /* Note that, since flex only supports UTF-8 by accident, character classes
  * including non-ASCII characters must be written out as (a|b|c|d) rather
  * than [abcd].
  */
bg_name		ИМЕ
cs_name		(J[Mm](É|é|\\\('[Ee]|E|e)[Nn][Oo]|N(Á|á)[Zz][Ee][Vv])
da_name		N[Aa][Vv][Nn]
de_name		B[Ee][Zz][Ee][Ii][Cc][Hh][Nn][Uu][Nn][Gg]
en_name		N[Aa][Mm][Ee]
es_name		N[Oo][Mm][Bb][Rr][Ee]
fi_name		N[Ii][Mm][Ii]
fr_name		N[Oo][Mm]
hu_name		N(É|é|E|e)[Vv]
id_name		N[Aa][Mm][Aa]
 /* NOME also works for gl, pt */
it_name		N[Oo][Mm][Ee]
ja_name		(名|̾)(前|称)
ko_name		이름
latin_name	N[Oo][Mm][Ee][Nn]
nl_name		N[Aa][Aa][Mm]
pl_name		N[Aa][Zz][Ww][Aa]
ru_name         (ИМЯ|НАЗВАНИЕ|НАИМЕНОВАНИЕ)
sk_name		M[Ee][Nn][Oo]
sr_name		НАЗИВ
sv_name		N[Aa][Mm][Nn]
tr_name		(İ|i)S(İ|i)M  
vi_name		TÊN
zh_CN_name	名{blank}?称{blank}?.*
zh_TW_name	(名{blank}?稱|命令名){blank}?.*
name		({bg_name}|{cs_name}|{da_name}|{de_name}|{en_name}|{es_name}|{fi_name}|{fr_name}|{hu_name}|{id_name}|{it_name}|{ja_name}|{ko_name}|{latin_name}|{nl_name}|{pl_name}|{ru_name}|{sk_name}|{sr_name}|{sv_name}|{tr_name}|{vi_name}|{zh_CN_name}|{zh_TW_name})
name_sec	{dbl_quote}?{style_change}?{name}{style_change}?({blank}*{dbl_quote})?

 /* eptgrv : eqn, pic, tbl, grap, refer, vgrind */
tbl_request	\.TS
eqn_request	\.EQ
pic_request	\.PS
grap_request	\.G1
ref1_request	\.R1
ref2_request	\.\[
vgrind_request	\.vS

%%

 /* begin NAME section processing */
<MAN_FILE>{sec_request}{blank_eol}+{name_sec}{blank}*		BEGIN (MAN_PRENAME);
<CAT_FILE>{empty}{2,}{name}{blank}*{indent}			BEGIN (CAT_NAME);

 /* general text matching */
<MAN_FILE>\.[^Ss\r\n].*					|
<MAN_FILE>\..{0,3}{dbl_quote}?.{0,4}{dbl_quote}? 	|
<MAN_FILE>{comment}.*					|
<CAT_FILE>.{1,9}					|
<CAT_FILE>[ ]*						|
<CAT_FILE>{eol}{2,}					|
<MAN_FILE,CAT_FILE>.|{eol}

<MAN_REST>{
	{bol}{tbl_request}		filters[TBL_FILTER] = 't';
	{bol}{eqn_request}		filters[EQN_FILTER] = 'e';
	{bol}{pic_request}		filters[PIC_FILTER] = 'p';
	{bol}{grap_request}		filters[GRAP_FILTER] = 'g';
	{bol}{ref1_request}		|
	{bol}{ref2_request}		filters[REF_FILTER] = 'r';
	{bol}{vgrind_request}		filters[VGRIND_FILTER] = 'v';
}
<MAN_REST><<EOF>>		{	/* exit */
					*p_name = '\0'; /* terminate the string */
					yyterminate ();
				}
<MAN_REST>.+|{eol}

 /* rules to end NAME section processing */
<FORCE_EXIT>.|{eol}		{	/* forced exit */
					*p_name = '\0'; /* terminate the string */
					yyterminate ();
				}

 /* Skip over initial spacing in NAME section. This is evil. */
<MAN_PRENAME>{bol}\.sp{blank}*		|
<MAN_PRENAME>{bol}\.br{blank}*		|
<MAN_PRENAME>{bol}\.LP{blank}*		|
<MAN_PRENAME>{bol}\.P[Pp]{blank}*	|
<MAN_PRENAME>{bol}\.P{blank}*

<MAN_PRENAME>{empty}{eol}		yyless (1);

<MAN_PRENAME>.|{eol}		{
					yyless (0);
					BEGIN (MAN_NAME);
				}

<MAN_NAME>{bol}{sec_request}{blank}*	| 	/* Another section */
<MAN_NAME>{bol}\.X{upper}{blank}+	|	/* special - hpux */
<MAN_NAME>{bol}\.sp{blank}*		|	/* vertical spacing */
<MAN_NAME>{bol}\.ig{blank}*		|	/* block comment */
<MAN_NAME>{bol}\.de[1i]?{blank}*	|	/* macro definition */
<MAN_NAME>{empty}{bol}.+		|
<MAN_NAME><<EOF>>		{	/* terminate the string */
					*p_name = '\0';
					BEGIN (MAN_REST);
				}

<CAT_NAME>{bol}S[yYeE]			|
<CAT_NAME>{eol}{2,}.+			|
<CAT_NAME>{next}__		{	/* terminate the string */
					*p_name = '\0';
					BEGIN (CAT_REST);
					yyterminate ();
				}

 /* ROFF request removal */
<MAN_NAME>{
 /* some include quoting; dealing with this is unpleasant */
	{bol}{typeface}{blank}+\"	{
						newline_found ();
						waiting_for_quote = 1;
					}

	{bol}{typeface}{blank}+		|	/* type face commands */
	{bol}\.ft{blank}.*		|	/* font change */
	{bol}\.V[be]{blank}.*		|	/* pod2man, verbatim mode */
	{bol}\.IX{blank}.*		|	/* .IX line */
	{next}{comment}.*		{	/* per line comments */
						newline_found ();
					}
}

 /* No-op requests */
<MAN_NAME>{bol}\.{blank}*$		newline_found ();
<MAN_NAME>{bol}\.\.$			newline_found ();

 /* Toggle fill mode */
<MAN_NAME>{bol}\.nf.*			fill_mode = 0;
<MAN_NAME>{bol}\.fi.*			fill_mode = 1;

<CAT_NAME>-{eol}{blank_eol}*		/* strip continuations */

 /* convert to DASH */
<MAN_NAME>{next}{blank}*\\\((mi|hy|em){blank}*	|
<MAN_NAME>{next}{blank_eol}+[-\\]-{blank}*	|
<MAN_NAME>{next}{blank_eol}*[-\\]-{blank}+	|
<CAT_NAME>{next}{blank}+-{1,2}{blank_eol}+	|
<MAN_NAME>{bol}\.Nd{blank}*			add_separator_to_whatis ();

 /* escape sequences and special characters */
<MAN_NAME>{
 	{next}\\[\\e]			add_char_to_whatis ('\\');
 	{next}\\('|\(aa)		add_char_to_whatis ('\'');
 	{next}\\(`|\(ga)		add_char_to_whatis ('`');
	{next}\\-			add_char_to_whatis ('-');
	{next}\\\.			add_char_to_whatis ('.');
	{next}((\\[ 0t~])|[ ]|\t)*	add_char_to_whatis (' ');
	{next}\\\((ru|ul)		add_char_to_whatis ('_');
	{next}\\\\t			add_char_to_whatis ('\t');

	{next}\\[|^&!%acdpruz{}\r\n]	/* various useless control chars */
	{next}\\[bhlLvx]{blank}*'[^']+'	/* various inline functions */

	{next}\\\$[1-9]			/* interpolate arg */

	{next}\\\*(\({alpha})?{alpha}	/* interpolate string */
	{next}\\\({alpha}{alpha}	/* special (non printable) character */
	{next}\\["#].* 			/* comment */

	{next}{font_change}		/* font changes */
	{next}\\k{alpha}		/* mark input place in register */

	{next}\\n(\({alpha})?{alpha}	/* interpolate number register */
	{next}\\o\"[^"]+\"		/* overstrike chars */

	{next}{size_change}		/* size changes */
	{next}\\w{blank}*'[^']+'[^ \t]*	/* width of string */

	{next}\\			/* catch all */

	{next}\(\\\|\){blank}*		/* function() in hpux */
}

 /* collapse spaces, escaped spaces, tabs, newlines to a single space */
<CAT_NAME>{next}((\\[ ])|{blank})*	add_char_to_whatis (' ');

 /* a ROFF break request, a paragraph request, or an indentation change
    usually means we have multiple whatis definitions, provide a separator
    for later processing */
<MAN_NAME>{
	{bol}\.br{blank}*		|
	{bol}\.LP{blank}*		|
	{bol}\.PP{blank}*		|
	{bol}\.P{blank}*		|
	{bol}\.IP{blank}.*		|
	{bol}\.HP{blank}.*		|
	{bol}\.RS{blank}.*		|
	{bol}\.RE{blank}.*		add_char_to_whatis ((char) 0x11);
}

<MAN_NAME>{bol}\.{alpha}{2}{blank}*  	{ /* very general roff requests */
						newline_found ();
					}

 /* pass words as a chunk. speed optimization */
<MAN_NAME>[[:alnum:]]*		add_str_to_whatis (yytext, yyleng);

 /* normalise the period (,) separators */
<CAT_NAME>{blank}*,[ \t\r\n]*		|
<MAN_NAME>{blank}*,{blank}*		add_str_to_whatis (", ", 2);

<CAT_NAME,MAN_NAME>{bol}.	{
					newline_found ();
					add_char_to_whatis (yytext[yyleng - 1]);
				}

<CAT_NAME,MAN_NAME>.			add_char_to_whatis (*yytext);

 /* default EOF rule */
<<EOF>>	return 1;

%%

/* print warning and force scanner to terminate */
static void too_big (void)
{
	error (0, 0,
	       _("warning: whatis for %s exceeds %d bytes, truncating."),
	       fname, MAX_NAME);

	BEGIN (FORCE_EXIT);
}

/* append a string to newname if enough room */
static void add_str_to_whatis (const char *string, size_t length)
{
	if (p_name - newname + length >= MAX_NAME)
		too_big ();
	else {
		(void) strncpy (p_name, string, length);
		p_name += length;
	}
} 

/* append a char to newname if enough room */
static void add_char_to_whatis (unsigned char c)
{
	if (p_name - newname + 1 >= MAX_NAME)
		too_big ();
	else if (waiting_for_quote && c == '"')
		waiting_for_quote = 0;
	else
		*p_name++ = c;
}

/* append the " - " separator to newname, trimming the first space if one's
 * already there
 */
static void add_separator_to_whatis (void)
{
	if (p_name != newname && *(p_name - 1) != ' ')
		add_char_to_whatis (' ');
	add_str_to_whatis ("- ", 2);
}

static void newline_found (void)
{
	/* If we are mid p_name and the last added char was not a space,
	 * best add one.
	 */
	if (p_name != newname && *(p_name - 1) != ' ') {
		if (fill_mode)
			add_char_to_whatis (' ');
		else
			add_char_to_whatis ((char) 0x11);
	}
	waiting_for_quote = 0;
}

int find_name (const char *file, const char *filename, lexgrog *p_lg,
	       const char *encoding)
{
	int ret;
	pipeline *p;
	char *page_encoding = NULL;

	if (strcmp (file, "-") == 0) {
		p = decompress_fdopen (dup (fileno (stdin)));
	} else {
		struct stat st;
		char *lang;

		if (stat (file, &st)) {
			error (0, errno, "%s", file);
			return 0;
		}

		if (S_ISDIR (st.st_mode)) {
			error (0, EISDIR, "%s", file);
			return 0;
		}

		drop_effective_privs ();
		p = decompress_open (file);
		if (!p) {
			error (0, errno, _("can't open %s"), file);
			regain_effective_privs ();
			return 0;
		}
		regain_effective_privs ();

		if (!encoding) {
			lang = lang_dir (file);
			page_encoding = get_page_encoding (lang);
			free (lang);
		}
	}
	if (!page_encoding && encoding)
		page_encoding = xstrdup (encoding);
	if (page_encoding)
		add_manconv (p, page_encoding, "UTF-8");
	free (page_encoding);
	pipeline_start (p);

	ret = find_name_decompressed (p, filename, p_lg);
	pipeline_free (p);
	return ret;
}

int find_name_decompressed (pipeline *p, const char *filename, lexgrog *p_lg)
{
	int ret;

	decomp = p;

	fname = filename;
	*(p_name = newname) = '\0';
	memset (filters, '_', sizeof (filters));

	fill_mode = 1;
	waiting_for_quote = 0;

	if (p_lg->type)
		BEGIN (CAT_FILE);
	else
		BEGIN (MAN_FILE);

	drop_effective_privs ();

	yyrestart (NULL);
	ret = yylex ();

	regain_effective_privs ();

	pipeline_wait (decomp);

	if (ret)
		return 0;
	else {
		char f_tmp[MAX_FILTERS];
		int j, k;

		/* wipe out any leading or trailing spaces */
		if (*newname) {
			for (p_name = strchr (newname, '\0');
			     *(p_name - 1) == ' ';
			     p_name--);
			if (*p_name == ' ')
				*p_name = '\0';
		}
		for (p_name = newname; *p_name == ' '; p_name++);
		p_lg->whatis = xstrdup (p_name);
		memset (f_tmp, '\0', MAX_FILTERS);
		f_tmp[0] = '-';
		for (j = k = 0; j < MAX_FILTERS; j++)
			if (filters[j] != '_')
				f_tmp[k++] = filters[j];
		p_lg->filters = xstrdup (f_tmp);
		return p_name[0];
	}
}

#ifdef TEST

static void usage (int status)
{
	printf (_("usage: %s [-mcwfhV] [-E encoding] file ...\n"), program_name);
	printf (_(
		"-m, --man                   parse as man page.\n"
		"-c, --cat                   parse as cat page.\n"
		"-w, --whatis                show whatis information.\n"
		"-f, --filters               show guessed series of "
					    "preprocessing filters.\n"
		"-E, --encoding encoding     override character set.\n"
		"-V, --version               show version.\n"
		"-h, --help                  show this usage message.\n"
		"\n"
		"The defaults are --man and --whatis.\n"));
	exit (status);
}

int main (int argc, char **argv)
{
	int c;
	int type = 0;
	int parse_man = 0, parse_cat = 0, show_whatis = 0, show_filters = 0;
	const char *encoding = NULL;
	int some_failed = 0;

	program_name = xstrdup (basename (argv[0]));

	while ((c = getopt_long (argc, argv, args,
				 long_options, NULL)) != -1) {
		switch (c) {
			case 'm':
				parse_man = 1;
				break;
			case 'c':
				parse_cat = 1;
				break;
			case 'w':
				show_whatis = 1;
				break;
			case 'f':
				show_filters = 1;
				break;
			case 'E':
				encoding = optarg;
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
	if (parse_man) {
		if (parse_cat) {
			error (0, 0, _("-m -c: incompatible options"));
			usage (FAIL);
		} else
			type = 0;
	} else {
		if (parse_cat)
			type = 1;
		else
			type = 0;	/* default = man */
	}
	if (!show_whatis && !show_filters)
		show_whatis = 1;

	pipeline_install_sigchld ();

	while (optind != argc) {
		lexgrog lg;
		const char *file;
		int found = 0;

		lg.type = type;

		if (STREQ (argv[optind], "-"))
			file = argv[optind];
		else {
			char *path, *pathend;
			struct stat statbuf;

			path = xstrdup (argv[optind]);
			pathend = strrchr (path, '/');
			if (pathend) {
				*pathend = '\0';
				pathend = strrchr (path, '/');
				if (pathend && STRNEQ (pathend + 1, "man", 3))
					*pathend = '\0';
				else {
					free (path);
					path = NULL;
				}
			} else {
				free (path);
				path = NULL;
			}

			file = ult_src (argv[optind], path ? path : ".",
					&statbuf, SO_LINK);
			if (path)
				free (path);
		}

		if (file && find_name (file, "-", &lg, encoding)) {
			struct page_description *descs =
				parse_descriptions (NULL, lg.whatis);
			const struct page_description *desc;
			for (desc = descs; desc; desc = desc->next) {
				if (!desc->name || !desc->whatis)
					continue;
				found = 1;
				printf ("%s", argv[optind]);
				if (show_filters)
					printf (" (%s)", lg.filters);
				if (show_whatis)
					printf (": \"%s - %s\"",
						desc->name, desc->whatis);
				printf ("\n");
			}
			free_descriptions (descs);
		}

		if (!found) {
			printf ("%s: parse failed\n", argv[optind]);
			some_failed = 1;
		}

		++optind;
	}

	if (some_failed)
		return FATAL;
	else
		return OK;
}

#endif /* TEST */

#ifdef PROFILE
void rule_profile (void)
{
	int i, tot = 0;

	printf ("found NAME in %d man, %d cat pages\n", ctr[1], ctr[2]);
	for (i = 3; i <= YY_NUM_RULES; i++) 
		if (ctr[i]) {
			printf ("rule[%d]: %d\n", i, ctr[i]);
			tot += ctr[i];
		}
	printf ("Total rules executed: %d\n", tot);
}
#else
void rule_profile (void) {}
#endif
