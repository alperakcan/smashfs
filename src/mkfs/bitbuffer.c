/*
 * Alper Akcan - 14.09.2011
 */

#include <string.h>

#include "bitbuffer.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

int bitbuffer_init (struct bitbuffer *bitbuffer, unsigned char *buffer, int size)
{
	struct bitbuffer *b;
	b = bitbuffer;
	b->buffer = buffer;
	b->size = size;
	b->bsize = size * 8;
	b->end = buffer + b->size;
	b->index = 0;
	return 0;
}

void bitbuffer_uninit (struct bitbuffer *bitbuffer)
{
	(void) bitbuffer;
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
