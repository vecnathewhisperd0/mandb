/*
 * orderfiles.c: order file accesses to optimise disk load
 *
 * Copyright (C) 2014 Colin Watson.
 *
 * Inspired by and loosely based on dpkg/src/filesdb.c, which is:
 *   Copyright (C) 1995 Ian Jackson <ian@chiark.greenend.org.uk>
 *   Copyright (C) 2000,2001 Wichert Akkerman <wakkerma@debian.org>
 *   Copyright (C) 2008-2014 Guillem Jover <guillem@debian.org>
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

#ifdef HAVE_LINUX_FIEMAP_H
#  include <linux/fiemap.h>
#  include <linux/fs.h>
#  include <sys/ioctl.h>
#  include <sys/vfs.h>
#endif /* HAVE_LINUX_FIEMAP_H */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "manconfig.h"

#include "hashtable.h"

struct hashtable *physical_offsets = NULL;

#if defined(HAVE_LINUX_FIEMAP_H)
int compare_physical_offsets (const void *a, const void *b)
{
	const char *left = *(const char **) a;
	const char *right = *(const char **) b;
	uint64_t *left_offset_p = hashtable_lookup (physical_offsets,
						    left, strlen (left));
	uint64_t *right_offset_p = hashtable_lookup (physical_offsets,
						     right, strlen (right));
	uint64_t left_offset = left_offset_p ? *left_offset_p : UINT64_MAX;
	uint64_t right_offset = right_offset_p ? *right_offset_p : UINT64_MAX;

	if (left_offset < right_offset)
		return -1;
	else if (left_offset > right_offset)
		return 1;
	else
		return 0;
}

void order_files (const char *dir, char **basenames, size_t n_basenames)
{
	int dir_fd_open_flags;
	int dir_fd;
	struct statfs fs;
	size_t i;

	dir_fd_open_flags = O_SEARCH | O_DIRECTORY;
#ifdef O_PATH
	dir_fd_open_flags |= O_PATH;
#endif
	dir_fd = open (dir, dir_fd_open_flags);
	if (dir_fd < 0)
		return;

	if (fstatfs (dir_fd, &fs) < 0) {
		close (dir_fd);
		return;
	}

	/* Sort files by the physical locations of their first blocks, in an
	 * attempt to minimise disk drive head movements.  This assumes that
	 * files are small enough that they are likely to be in one block or
	 * a small number of contiguous blocks, which seems a reasonable
	 * assumption for manual pages.
	 */
	physical_offsets = hashtable_create (plain_hashtable_free);
	for (i = 0; i < n_basenames; ++i) {
		struct {
			struct fiemap fiemap;
			struct fiemap_extent extent;
		} fm;
		int fd;

		fd = openat (dir_fd, basenames[i], O_RDONLY);
		if (fd < 0)
			continue;

		memset (&fm, 0, sizeof (fm));
		fm.fiemap.fm_start = 0;
		fm.fiemap.fm_length = fs.f_bsize;
		fm.fiemap.fm_flags = 0;
		fm.fiemap.fm_extent_count = 1;

		if (ioctl (fd, FS_IOC_FIEMAP, (unsigned long) &fm) == 0) {
			uint64_t *offset = XMALLOC (uint64_t);
			*offset = fm.fiemap.fm_extents[0].fe_physical;
			hashtable_install (physical_offsets, basenames[i],
					   strlen (basenames[i]), offset);
		}

		close (fd);
	}
	qsort (basenames, n_basenames, sizeof *basenames,
	       compare_physical_offsets);
	hashtable_free (physical_offsets);
	physical_offsets = NULL;
	close (dir_fd);
}
#elif defined(HAVE_POSIX_FADVISE)
void order_files (const char *dir, char **basenames, size_t n_basenames)
{
	int dir_fd_open_flags;
	int dir_fd;
	size_t i;

	dir_fd_open_flags = O_SEARCH | O_DIRECTORY;
#ifdef O_PATH
	dir_fd_open_flags |= O_PATH;
#endif
	dir_fd = open (dir, dir_fd_open_flags);
	if (dir_fd < 0)
		return;

	/* While we can't actually order the files, we can at least ask the
	 * kernel to preload them.
	 */
	for (i = 0; i < n_basenames; ++i) {
		int fd = openat (dir_fd, basenames[i], O_RDONLY | O_NONBLOCK);
		if (fd >= 0) {
			posix_fadvise (fd, 0, 0, POSIX_FADV_WILLNEED);
			close (fd);
		}
	}

	close (dir_fd);
}
#else
void order_files (const char *dir, char **basenames, size_t n_basenames)
{
}
#endif
