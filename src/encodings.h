/*
 * encodings.h: Interface to locale and encoding handling for man
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

char *get_page_encoding (const char *lang);
const char *get_source_encoding (const char *lang);
char *get_standard_output_encoding (const char *lang);
const char *get_locale_charset (void);
const char *get_default_device (const char *locale_charset,
				const char *source_encoding);
const char *get_roff_encoding (const char *device,
			       const char *source_encoding);
const char *get_output_encoding (const char *device);
const char *get_less_charset (const char *locale_charset);
