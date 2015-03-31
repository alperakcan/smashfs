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
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/zlib.h>

#include "compressor-gzip.h"

struct gzip {
	z_stream stream;
};

void * gzip_create (void)
{
	struct gzip *gzip;
	gzip = kmalloc(sizeof(z_stream), GFP_KERNEL);
	if (gzip == NULL) {
		return NULL;
	}
	gzip->stream.workspace = vmalloc(zlib_inflate_workspacesize());
	if (gzip->stream.workspace == NULL) {
		kfree(gzip);
		return NULL;
	}
	return gzip;
}

void gzip_destroy (void *context)
{
	struct gzip *gzip;
	if (context == NULL) {
		return;
	}
	gzip = context;
	vfree(gzip->stream.workspace);
	kfree(gzip);
}

int gzip_compress (void *context, void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	(void) context;
	(void) src;
	(void) ssize;
	(void) dst;
	(void) dsize;
	return -1;
}

int gzip_uncompress (void *context, void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	int rc;
	z_stream *stream;
	struct gzip *gzip;

	gzip = context;
	stream = &gzip->stream;

	stream->next_in = NULL;
	stream->avail_in = 0;
	zlib_inflateInit2(stream, -MAX_WBITS);

	stream->next_in = src;
	stream->avail_in = ssize;

	stream->next_out = dst;
	stream->avail_out = dsize;

	rc = zlib_inflate(stream, Z_FINISH);
	if (rc != Z_STREAM_END) {
		zlib_inflateEnd(stream);
		return -1;
	}

	zlib_inflateEnd(stream);
	return stream->total_out;
}
