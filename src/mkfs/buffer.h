
struct buffer {
	unsigned char *buffer;
	long long size;
	long long length;
};

int buffer_init (struct buffer *buffer);
int buffer_uninit (struct buffer *buffer);
long long buffer_length (struct buffer *buffer);
int buffer_add (struct buffer *buffer, const void *data, unsigned int size);
