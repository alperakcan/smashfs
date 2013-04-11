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

#include <stdio.h>
#include <string.h>
#include <zlib.h>

int gzip_compress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	int rc;
	uLong zlen;
	z_stream stream;
	zlen = compressBound(ssize);
	if (zlen > dsize) {
		fprintf(stderr, "not enough space\n");
		return -1;
	}
	stream.next_in = (Bytef *) src;
	stream.avail_in = (uInt) ssize;
	stream.next_out = dst;
	stream.avail_out = (uInt) zlen;
	if ((uLong) stream.avail_out != zlen) {
		fprintf(stderr, "avail out and zlen is different\n");
		return -1;
	}
	stream.zalloc = (alloc_func) 0;
	stream.zfree = (free_func) 0;
	stream.opaque = (voidpf) 0;
	rc = deflateInit2(&stream, 9, Z_DEFLATED, -MAX_WBITS, 9, Z_DEFAULT_STRATEGY);
	if (rc != Z_OK) {
		fprintf(stderr, "deflateinit2 failed\n");
		return -1;
	}
	rc = deflate(&stream, Z_FINISH);
	if (rc != Z_STREAM_END) {
		fprintf(stderr, "deflate failed\n");
		return -1;
	}
	zlen = stream.total_out;
	rc = deflateEnd(&stream);
	if (rc != Z_OK) {
		fprintf(stderr, "deflateEnd failed\n");
		return -1;
	}
	return zlen;
}

int gzip_uncompress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	int rc;
	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = ssize;
	strm.next_in = src;
	strm.avail_out = dsize;
	strm.next_out = dst;
	rc = inflateInit2(&strm, -MAX_WBITS);
	if (rc != Z_OK) {
		fprintf(stderr, "inflateInit2 failed\n");
		return -1;
	}
	rc = inflate(&strm, Z_FINISH);
	if (rc != Z_STREAM_END) {
		fprintf(stderr, "inflate failed\n");
		inflateEnd(&strm);
		return -1;
	}
	rc = inflateEnd(&strm);
	if (rc != Z_OK) {
		fprintf(stderr, "inflateEnd failed\n");
		return -1;
	}
	return (int) strm.total_out;
}
