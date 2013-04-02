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

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../include/smashfs.h"

#include "compressor.h"
#include "compressor-none.h"
#include "compressor-gzip.h"
#include "compressor-lzma.h"

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
