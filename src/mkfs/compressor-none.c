
#include <string.h>

int none_compress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	if (dsize < ssize) {
		return -1;
	}
	memcpy(dst, src, ssize);
	return ssize;
}

int none_uncompress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	if (dsize < ssize) {
		return -1;
	}
	memcpy(dst, src, ssize);
	return ssize;
}
