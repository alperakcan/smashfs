
#include <stdio.h>
#include <string.h>

#include <zlib.h>

int gzip_compress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	int rc;
	uLong zlen;
	z_stream stream;
	zlen = compressBound(ssize);
	if (zlen > dsize) {
		fprintf(stderr, "not enough space\n");
		return -1;
	}
	stream.next_in = (Bytef *) src;
	stream.avail_in = (uInt) ssize;
	stream.next_out = dst;
	stream.avail_out = (uInt) zlen;
	if ((uLong) stream.avail_out != zlen) {
		fprintf(stderr, "avail out and zlen is different\n");
		return -1;
	}
	stream.zalloc = (alloc_func) 0;
	stream.zfree = (free_func) 0;
	stream.opaque = (voidpf) 0;
	rc = deflateInit2(&stream, 9, Z_DEFLATED, -MAX_WBITS, 9, Z_DEFAULT_STRATEGY);
	if (rc != Z_OK) {
		fprintf(stderr, "deflateinit2 failed\n");
		return -1;
	}
	rc = deflate(&stream, Z_FINISH);
	if (rc != Z_STREAM_END) {
		fprintf(stderr, "deflate failed\n");
		return -1;
	}
	zlen = stream.total_out;
	rc = deflateEnd(&stream);
	if (rc != Z_OK) {
		fprintf(stderr, "deflateEnd failed\n");
		return -1;
	}
	return zlen;
}

int gzip_uncompress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	if (dsize < ssize) {
		return -1;
	}
	memcpy(dst, src, ssize);
	return dsize;
}
