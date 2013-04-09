/*
 * Copyright (c) 2013, Alper Akcan <alper.akcan@gmail.com>.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/zlib.h>

int gzip_compress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	(void) src;
	(void) ssize;
	(void) dst;
	(void) dsize;
	return -1;
}

int gzip_uncompress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	int rc;
	z_stream stream;

	stream.workspace = vmalloc(zlib_inflate_workspacesize());
	if (stream.workspace == NULL) {
		return -1;
	}

	stream.next_in = NULL;
	stream.avail_in = 0;
	zlib_inflateInit2(&stream, -MAX_WBITS);

	stream.next_in = src;
	stream.avail_in = ssize;

	stream.next_out = dst;
	stream.avail_out = dsize;

	rc = zlib_inflateReset(&stream);
	if (rc != Z_OK) {
		zlib_inflateEnd(&stream);
		zlib_inflateInit2(&stream, -MAX_WBITS);
	}

	rc = zlib_inflate(&stream, Z_FINISH);
	if (rc != Z_STREAM_END) {
		zlib_inflateEnd(&stream);
		vfree(stream.workspace);
		return -1;
	}

	zlib_inflateEnd(&stream);
	vfree(stream.workspace);
	return stream.total_out;
}
