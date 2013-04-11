/*
 * Copyright (c) 2009-2013, Alper Akcan <alper.akcan@gmail.com>.
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

#include <linux/module.h>
#include "bitbuffer.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

int bitbuffer_init_from_buffer (struct bitbuffer *bitbuffer, unsigned char *buffer, int size)
{
	struct bitbuffer *b;
	b = bitbuffer;
	b->buffer = buffer;
	b->size = size;
	b->bsize = size * 8;
	b->end = buffer + b->size;
	b->index = 0;
	b->external = 1;
	return 0;
}

void bitbuffer_uninit (struct bitbuffer *bitbuffer)
{
	bitbuffer_init_from_buffer(bitbuffer, NULL, 0);
}

void * bitbuffer_buffer (struct bitbuffer *bitbuffer)
{
	return bitbuffer->buffer;
}

unsigned int bitbuffer_getlength (struct bitbuffer *bitbuffer)
{
	return (bitbuffer_getbitlength(bitbuffer) + 7 ) / 8;
}

unsigned int bitbuffer_getbitlength (struct bitbuffer *bitbuffer)
{
	return MAX(bitbuffer->bsize - bitbuffer->index, 0);
}

unsigned int bitbuffer_getpos (struct bitbuffer *bitbuffer)
{
	return bitbuffer->index;
}

unsigned int bitbuffer_setpos (struct bitbuffer *bitbuffer, unsigned int pos)
{
	bitbuffer->index = pos;
	return 0;
}

void bitbuffer_putbit (struct bitbuffer *bitbuffer, unsigned int value)
{
	int bit;
	int byte;
	bit = bitbuffer->index % 8;
	byte = bitbuffer->index / 8;
	bitbuffer->buffer[byte] |= ((value & 0x01) << (7 - bit));
	bitbuffer->index++;
}

void bitbuffer_putbits (struct bitbuffer *bitbuffer, int n, unsigned int value)
{
	unsigned int v = value;
	while (n) {
		bitbuffer_putbit(bitbuffer, (v >> (n - 1)) & 0x01);
		n -= 1;
	}
}

unsigned int bitbuffer_getbit (struct bitbuffer *bitbuffer)
{
	int bit;
	int byte;
	bit = bitbuffer->index % 8;
	byte = bitbuffer->index / 8;
	bitbuffer->index++;
	return (bitbuffer->buffer[byte] >> (7 - bit)) & 0x01;
}

unsigned int bitbuffer_getbits (struct bitbuffer *bitbuffer, int n)
{
	unsigned int tmp = 0;
	while (n) {
		if (((bitbuffer->index % 8) == 0) && (n >= 8)) {
			while (n >= 8) {
				tmp = (tmp << 0x08) | bitbuffer->buffer[bitbuffer->index / 8];
				bitbuffer->index += 8;
				n -= 8;
			}
		} else {
			tmp = (tmp << 1) | bitbuffer_getbit(bitbuffer);
			n -= 1;
		}
	}
	return tmp;
}

unsigned int bitbuffer_getbuffer (struct bitbuffer *bitbuffer, char *buffer, int n)
{
	int i;
	if ((bitbuffer->index % 8) != 0) {
		for (i = 0; i < n; i++) {
			buffer[i] = bitbuffer_getbits(bitbuffer, 8);
		}
	} else {
		memcpy(buffer, &bitbuffer->buffer[bitbuffer->index / 8], n);
		bitbuffer->index += n * 8;
	}
	return 0;
}

unsigned int bitbuffer_skipbits (struct bitbuffer *bitbuffer, int n)
{
	bitbuffer->index += n;
	return 0;
}

unsigned int bitbuffer_showbits (struct bitbuffer *bitbuffer, int n)
{
	int index;
	unsigned int tmp = 0;
	index = bitbuffer->index;
	tmp = bitbuffer_getbits(bitbuffer, n);
	bitbuffer->index = index;
	return tmp;
}

unsigned int bitbuffer_copybits (struct bitbuffer *pbitbuffer, struct bitbuffer *gbitbuffer, int n)
{
	unsigned int el = bitbuffer_getbits(gbitbuffer, n);
	bitbuffer_putbits(pbitbuffer, n, el);
	return el;
}
