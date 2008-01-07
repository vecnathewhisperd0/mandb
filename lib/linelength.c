/*
 * linelength.c: find the terminal line length
 * Preferences: 1. MANWIDTH, 2. ioctl, 3. COLUMNS, 4. 80
 *
 * Originally adapted from Andries Brouwer's man implementation, also
 * released under the GPL: authors believed to include Martin Schulze and
 * Jon Tombs, dated 1995/09/02.
 *
 * Changes for man-db copyright (C) 2001, 2003, 2007 Colin Watson.
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
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

static int line_length = -1;

int get_line_length (void)
{
	const char *columns;
	int width;

	if (line_length != -1)
		return line_length;

	line_length = 80;

	columns = getenv ("MANWIDTH");
	if (columns != NULL) {
		width = atoi (columns);
		if (width > 0)
			return line_length = width;
	}

#ifdef TIOCGWINSZ
	/* Jon Tombs */
	if (isatty (STDIN_FILENO) && isatty (STDOUT_FILENO)) {
		struct winsize wsz;

		if (ioctl (STDIN_FILENO, TIOCGWINSZ, &wsz))
			perror ("TIOCGWINSZ failed");
		else if (wsz.ws_col)
			return line_length = wsz.ws_col;
	}
#endif

	columns = getenv ("COLUMNS");
	if (columns != NULL) {
		width = atoi (columns);
		if (width > 0)
			return line_length = width;
	}

	return line_length = 80;
}
