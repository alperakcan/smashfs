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

#include "../include/smashfs.h"

#include "compressor.h"
#include "compressor-none.h"
#include "compressor-gzip.h"
#include "compressor-lzma.h"
#include "compressor-lzo.h"
#include "compressor-xz.h"

struct compressor {
	char *name;
	enum smashfs_compression_type type;
	int (*compress) (void *src, unsigned int ssize, void *dst, unsigned int dsize);
	int (*uncompress) (void *src, unsigned int ssize, void *dst, unsigned int dsize);
};

struct compressor *compressors[] = {
	& (struct compressor) { "none", smashfs_compression_type_none, none_compress, none_uncompress },
	& (struct compressor) { "gzip", smashfs_compression_type_gzip, gzip_compress, gzip_uncompress },
	& (struct compressor) { "lzma", smashfs_compression_type_lzma, lzma_compress, lzma_uncompress },
	& (struct compressor) { "lzo" , smashfs_compression_type_lzo , lzo_compress , lzo_uncompress  },
	& (struct compressor) { "xz"  , smashfs_compression_type_xz  , xz_compress  , xz_uncompress   },
	NULL
};

struct compressor * compressor_create_name (const char *name)
{
	struct compressor **c;
	for (c = compressors; *c; c++) {
		if (strcmp((*c)->name, name) == 0) {
			return *c;
		}
	}
	return NULL;
}

struct compressor * compressor_create_type (enum smashfs_compression_type type)
{
	struct compressor **c;
	for (c = compressors; *c; c++) {
		if ((*c)->type == type) {
			return *c;
		}
	}
	return NULL;
}

int compressor_destroy (struct compressor *compressor)
{
	(void) compressor;
	return 0;
}

enum smashfs_compression_type compressor_type (struct compressor *compressor)
{
	return compressor->type;
}

int compressor_compress (struct compressor *compressor, void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	return compressor->compress(src, ssize, dst, dsize);
}

int compressor_uncompress (struct compressor *compressor, void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	return compressor->uncompress(src, ssize, dst, dsize);
}
