
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../include/smashfs.h"

#include "compressor.h"
#include "compressor-none.h"
#include "compressor-gzip.h"

struct compressor {
	char *name;
	enum smashfs_compression_type type;
	int (*compress) (void *src, unsigned int ssize, void *dst, unsigned int dsize);
	int (*uncompress) (void *src, unsigned int ssize, void *dst, unsigned int dsize);
};

struct compressor *compressors[] = {
	& (struct compressor) { "none", smashfs_compression_type_none, none_compress, none_uncompress },
	& (struct compressor) { "gzip", smashfs_compression_type_gzip, gzip_compress, gzip_uncompress },
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
