/*
 * Alper Akcan - 14.09.2011
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
