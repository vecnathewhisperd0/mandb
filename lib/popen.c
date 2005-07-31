/* popen.c: pipe open and close functions.
 *
 * Copyright (C) 1994, 1995 Graeme W. Wilford. (Wilf.)
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
 * Originally by Wilf. (G.Wilford@ee.surrey.ac.uk)
 * Colin Watson moved this from src/man.c to a separate library file.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#ifdef STDC_HEADERS
#  include <string.h>
#  include <stdlib.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#endif /* STDC_HEADERS */
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif /* HAVE_SYS_WAIT_H */

/* Some pclose(), notably Ultrix's get confused if we use more than one
   concurrently, blech.  Define our own popen()/pclose() combo. */

static pid_t *fd2pid = NULL;	/* map fd to pid, unused entries are zero */
static int max_fd = -1;		/* max fd seen, fd2pid has max_fd+1 elements */

FILE *
popen (const char *cmd, const char *type)
{
	pid_t child;
	int for_reading;
	int pipe_fd[2];
	int fd;
	FILE *stream;

	/* check type */
	if (type && (type[0] == 'r'))
		for_reading = 1;
	else if (type && (type[0] == 'w'))
		for_reading = 0;
	else
		return NULL;

	if (pipe (pipe_fd))
		return NULL;

	child = vfork ();
	if (child == -1) {
		close (pipe_fd[0]);
		close (pipe_fd[1]);
		return NULL;
	} else if (child == 0) {
		/* if for_reading connect the writing end of pipe to stdout
		   else the reading end to stdin */
		if (dup2 (pipe_fd[for_reading], for_reading) == -1)
			_exit (127);

		/* close pipe fds */
		close (pipe_fd[0]);
		close (pipe_fd[1]);

		/* exec cmd in a shell */
		execl ("/bin/sh", "sh", "-c", cmd, NULL);
		/* if we hit this, execl() failed */
		_exit (127);
	}

	/* if for_reading make a stream from the reading end of pipe
	   else from the writing end */
	fd = pipe_fd[!for_reading];
	close (pipe_fd[for_reading]);
	stream = fdopen (fd, type);

	/* extend fd2pid up to index fd if necessary */
	if (fd > max_fd) {
		pid_t *new = malloc ((fd + 1) * sizeof (pid_t));
		if (new) {
			/* copy old entries */
			memcpy (new, fd2pid, (max_fd + 1) * sizeof (pid_t));
			/* zero new entries */
			memset (new+max_fd+1, 0,
				(fd - max_fd) * sizeof (pid_t));
			fd2pid = new;
			max_fd = fd;
		}
	}

	/* if we didn't get the stream or couldn't extend fd2pid, clean up & fail */
	if (!stream || (fd > max_fd)) {
		int res;
		int save = errno;

		kill (child, SIGKILL);

		if (stream)
			fclose (stream);
		else
			close (fd);

		do {		/* cope with non-restarting system calls */
			res = waitpid (child, NULL, 0);
		} while ((res == -1) && (errno == EINTR));

		errno = save;
		return NULL;
	}

	fd2pid[fd] = child;		/* save pid for pclose() */
	return stream;
}


int
pclose (FILE *stream)
{
	int fd = fileno (stream);
	pid_t child;
	int status;
	int res;
	int save;

	if ((fd > max_fd) || !fd2pid[fd])
		return -1;
	child = fd2pid[fd];
	fd2pid[fd] = 0;

	if (fclose (stream))
		return -1;

	save = errno;
	do {			/* cope with non-restarting system calls */
		res = waitpid (child, &status, 0);
	} while ((res == -1) && (errno == EINTR));
	if (res != -1) errno = save;

	return status;
}
