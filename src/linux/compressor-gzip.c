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
	if (!stream.workspace ) {
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
		zlib_inflateInit(&stream);
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
