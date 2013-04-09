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
#include <stdlib.h>
#include <lzo/lzoconf.h>
#include <lzo/lzo1x.h>

int lzo_compress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	int rc;
	lzo_uint outlen;
	lzo_voidp workmem;
	(void) dsize;
	workmem = malloc(LZO1X_999_MEM_COMPRESS);
	if (workmem == NULL) {
		return -1;
	}
	rc = lzo1x_999_compress((lzo_bytep) src, ssize, dst, &outlen, workmem);
	if (rc != LZO_E_OK) {
		free(workmem);
		return -1;
	}
	if (outlen >= ssize) {
		free(workmem);
		return -1;
	}
	return outlen;
}

int lzo_uncompress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	int res;
	lzo_uint bytes = dsize;
	res = lzo1x_decompress_safe((lzo_bytep) src, ssize, (lzo_bytep) dst, &bytes, NULL);
	return res == LZO_E_OK ? (int) bytes : -1;
}
