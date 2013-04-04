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
#include <lzma.h>

#define LZMA_PROPS_SIZE		5
#define LZMA_UNCOMP_SIZE	8
#define LZMA_HEADER_SIZE	(LZMA_PROPS_SIZE + LZMA_UNCOMP_SIZE)

#define LZMA_OPTIONS		6
#define MEMLIMIT		(256 * 1024 * 1024)

#define MAX(a, b)		(((a) > (b)) ? (a) : (b))

int lzma_compress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	unsigned char *d = (unsigned char *) dst;
	lzma_options_lzma opt;
	lzma_stream strm = LZMA_STREAM_INIT;
	int res;
	if (ssize + LZMA_HEADER_SIZE > dsize) {
		fprintf(stderr, "not enough space\n");
		goto bail;
	}
	lzma_lzma_preset(&opt, LZMA_OPTIONS);
	opt.dict_size = MAX(4096, ssize);
	res = lzma_alone_encoder(&strm, &opt);
	if(res != LZMA_OK) {
		fprintf(stderr, "lzma_alone_encoder failed\n");
		lzma_end(&strm);
		goto bail;
	}
	strm.next_out = dst;
	strm.avail_out = dsize;
	strm.next_in = src;
	strm.avail_in = ssize;
	res = lzma_code(&strm, LZMA_FINISH);
	lzma_end(&strm);
	if(res != LZMA_STREAM_END) {
		fprintf(stderr, "lzma_code failed\n");
		goto bail;
	}
	d[LZMA_PROPS_SIZE] = ssize & 255;
	d[LZMA_PROPS_SIZE + 1] = (ssize >> 8) & 255;
	d[LZMA_PROPS_SIZE + 2] = (ssize >> 16) & 255;
	d[LZMA_PROPS_SIZE + 3] = (ssize >> 24) & 255;
	d[LZMA_PROPS_SIZE + 4] = 0;
	d[LZMA_PROPS_SIZE + 5] = 0;
	d[LZMA_PROPS_SIZE + 6] = 0;
	d[LZMA_PROPS_SIZE + 7] = 0;
	return (int) strm.total_out;
bail:	return -1;
}

int lzma_uncompress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	int rc;
	lzma_stream strm = LZMA_STREAM_INIT;
	unsigned char lzma_header[LZMA_HEADER_SIZE];
	rc = lzma_alone_decoder(&strm, MEMLIMIT);
	if(rc != LZMA_OK) {
		fprintf(stderr, "lzma_alone_encoder failed\n");
		lzma_end(&strm);
		goto bail;
	}
	memcpy(lzma_header, src, LZMA_HEADER_SIZE);
	memset(lzma_header + LZMA_PROPS_SIZE, 255, LZMA_UNCOMP_SIZE);
	strm.next_out = dst;
	strm.avail_out = dsize;
	strm.next_in = lzma_header;
	strm.avail_in = LZMA_HEADER_SIZE;
	rc = lzma_code(&strm, LZMA_RUN);
	if (rc != LZMA_OK || strm.avail_in != 0) {
		fprintf(stderr, "lzma_alone_encoder failed\n");
		lzma_end(&strm);
		goto bail;
	}
	strm.next_in = src + LZMA_HEADER_SIZE;
	strm.avail_in = ssize - LZMA_HEADER_SIZE;
	rc = lzma_code(&strm, LZMA_FINISH);
	lzma_end(&strm);
	if (rc == LZMA_STREAM_END) {
		return dsize;
	}
bail:	return -1;
}
