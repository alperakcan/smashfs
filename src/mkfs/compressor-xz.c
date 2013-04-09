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
	lzma_err = lzma_easy_buffer_encode(9 | LZMA_PRESET_EXTREME, lzma_ck, NULL, src, ssize, lzma, &lzma_pos, lzma_len);
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
