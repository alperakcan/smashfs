/*
 * Copyright (c) 2013, Alper Akcan <alper.akcan@gmail.com>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of the FreeBSD Project.
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
