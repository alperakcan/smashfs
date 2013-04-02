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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void * buffer_buffer (struct buffer *buffer)
{
	return buffer->buffer;
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
	memcpy(buffer->buffer + buffer->length, data, size);
	buffer->length += size;
	return size;
}
