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
#include <lzma.h>

#define LZMA_OPTIONS		6
#define MEMLIMIT		(256 * 1024 * 1024)

#define MAX(a, b)		(((a) > (b)) ? (a) : (b))

int lzma_compress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	int res;
	lzma_options_lzma opt;
	lzma_stream strm = LZMA_STREAM_INIT;
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
	return (int) strm.total_out;
bail:	return -1;
}

int lzma_uncompress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	int rc;
	lzma_stream strm = LZMA_STREAM_INIT;
	rc = lzma_alone_decoder(&strm, MEMLIMIT);
	if(rc != LZMA_OK) {
		fprintf(stderr, "lzma_alone_encoder failed\n");
		lzma_end(&strm);
		goto bail;
	}
	strm.next_out = dst;
	strm.avail_out = dsize;
	strm.next_in = src;
	strm.avail_in = ssize;
	rc = lzma_code(&strm, LZMA_FINISH);
	lzma_end(&strm);
	if (rc == LZMA_STREAM_END) {
		return dsize;
	}
bail:	return -1;
}
