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
