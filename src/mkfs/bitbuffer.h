/*
 * Copyright (c) 2011-2013, Alper Akcan <alper.akcan@gmail.com>.
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

struct bitbuffer {
	unsigned char *buffer;
	unsigned char *end;
	int size;
	int bsize;
	int index;
	int external;
};

int bitbuffer_init (struct bitbuffer *bitbuffer, int size);
int bitbuffer_init_from_buffer (struct bitbuffer *bitbuffer, unsigned char *buffer, int size);
void bitbuffer_uninit (struct bitbuffer *bitbuffer);
void * bitbuffer_buffer (struct bitbuffer *bitbuffer);
unsigned int bitbuffer_getlength (struct bitbuffer *bitbuffer);
unsigned int bitbuffer_getbitlength (struct bitbuffer *bitbuffer);
unsigned int bitbuffer_getpos (struct bitbuffer *bitbuffer);
unsigned int bitbuffer_setpos (struct bitbuffer *bitbuffer, unsigned int pos);
void bitbuffer_putbit (struct bitbuffer *bitbuffer, unsigned int value);
void bitbuffer_putbits (struct bitbuffer *bitbuffer, int n, unsigned int value);
unsigned int bitbuffer_getbit (struct bitbuffer *bitbuffer);
unsigned int bitbuffer_getbits (struct bitbuffer *bitbuffer, int n);
unsigned int bitbuffer_getbuffer (struct bitbuffer *bitbuffer, char *buffer, int n);
unsigned int bitbuffer_skipbits (struct bitbuffer *bitbuffer, int n);
unsigned int bitbuffer_showbits (struct bitbuffer *bitbuffer, int n);
unsigned int bitbuffer_copybits (struct bitbuffer *pbitbuffer, struct bitbuffer *gbitbuffer, int n);
