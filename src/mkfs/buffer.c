
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include "buffer.h"

int buffer_init (struct buffer *buffer)
{
	buffer->buffer = NULL;
	buffer->size = 0;
	buffer->length = 0;
	return 0;
}

int buffer_uninit (struct buffer *buffer)
{
	free(buffer->buffer);
	return 0;
}

long long buffer_length (struct buffer *buffer)
{
	return buffer->length;
}

int buffer_add (struct buffer *buffer, const void *data, unsigned int size)
{
	unsigned char *b;
	if (buffer->length + size >= buffer->size) {
		while (buffer->length + size >= buffer->size) {
			if (buffer->size == 0) {
				buffer->size = 1;
			} else {
				buffer->size = buffer->size * 2;
			}
		}
		b = malloc(buffer->size);
		if (b == NULL) {
			fprintf(stderr, "malloc failed\n");
			return -1;
		}
		memcpy(b, buffer->buffer, buffer->length);
		free(buffer->buffer);
		buffer->buffer = b;
	}
	memcpy(buffer->buffer, data, size);
	buffer->length += size;
	return size;
}
