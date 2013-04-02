
#include <stdlib.h>
#include <string.h>

#include "compressor.h"
#include "compressor-none.h"

struct compressor {
	char *name;
	int (*compress) (void *data, unsigned int size);
	int (*uncompress) (void *src, unsigned int ssize, void *dst, unsigned int dsize);
};

struct compressor *compressors[] = {
	& (struct compressor) { "none", none_compress, none_uncompress },
	NULL
};

struct compressor * compressor_create (const char *id)
{
	struct compressor **c;
	for (c = compressors; *c; c++) {
		if (strcmp((*c)->name, id) == 0) {
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
