/*
 * decompress.h: interface to decompression abstraction layer
 *
 * Copyright (C) 2007 Colin Watson.
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

#ifndef MAN_DECOMPRESS_H
#define MAN_DECOMPRESS_H

#include "pipeline.h"

struct decompress;
typedef struct decompress decompress;

/* Open a decompressor reading from FILENAME. The caller must start the
 * resulting pipeline.
 */
decompress *decompress_open (const char *filename);

/* Open a decompressor reading from file descriptor FD. The caller must
 * start the resulting pipeline.
 */
decompress *decompress_fdopen (int fd);

/* Get the pipeline corresponding to a decompressor.  Raises an assertion
 * failure if this is not a pipeline-based decompressor.
 */
pipeline *decompress_get_pipeline (decompress *d);

/* Start the processes in a pipeline-based decompressor.
 */
void decompress_start (decompress *d);

/* Read len bytes of data from the decompressor, returning the data block.
 * len is updated with the number of bytes read.
 */
const char *decompress_read (decompress *d, size_t *len);

/* Look ahead in the decompressor's output for len bytes of data, returning
 * the data block.  len is updated with the number of bytes read.  The
 * starting position of the next read or peek is not affected by this call.
 */
const char *decompress_peek (decompress *d, size_t *len);

/* Skip over and discard len bytes of data from the peek cache. Asserts that
 * enough data is available to skip, so you may want to check using
 * pipeline_peek_size first.
 */
void decompress_peek_skip (decompress *d, size_t len);

/* Read a line of data from the decompressor, returning it. */
const char *decompress_readline (decompress *d);

/* Look ahead in the decompressor's output for a line of data, returning it.
 * The starting position of the next read or peek is not affected by this
 * call.
 */
const char *decompress_peekline (decompress *d);

/* Wait for a decompressor to complete and return its combined exit status.
 */
int decompress_wait (decompress *d);

/* Destroy a decompressor.  Safely does nothing on NULL.  For pipeline-based
 * decompressors, may wait for the pipeline to complete if it has not already
 * done so.
 */
void decompress_free (decompress *d);

#endif /* MAN_DECOMPRESS_H */
