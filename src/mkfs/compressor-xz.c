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

#include <string.h>
#include <lzma.h>

#define MEMLIMIT		(256 * 1024 * 1024)

int xz_compress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	size_t lzma_len;
	size_t lzma_pos;
	lzma_ret lzma_err;
	lzma_check lzma_ck;
	unsigned char *lzma;
	lzma_len = dsize;
	lzma = dst;
	lzma_pos = 0;
	lzma_ck = LZMA_CHECK_CRC64;
	if (!lzma_check_is_supported(lzma_ck)) {
		lzma_ck = LZMA_CHECK_CRC32;
	}
	lzma_ck = LZMA_CHECK_NONE;
	lzma_err = lzma_easy_buffer_encode(6, lzma_ck, NULL, src, ssize, lzma, &lzma_pos, lzma_len);
	if (lzma_err == LZMA_OK) {
		return lzma_pos;
	} else {
		return -1;
	}
}

int xz_uncompress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	size_t src_pos = 0;
	size_t dest_pos = 0;
	uint64_t memlimit = MEMLIMIT;
	lzma_ret res = lzma_stream_buffer_decode(&memlimit, 0, NULL, src, &src_pos, ssize, dst, &dest_pos, dsize);
	return res == LZMA_OK && (ssize == src_pos) ? (int) dest_pos : -1;
}
