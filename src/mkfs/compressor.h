
struct compressor;

struct compressor * compressor_create_name (const char *name);
struct compressor * compressor_create_type (enum smashfs_compression_type type);
int compressor_destroy (struct compressor *compressor);
enum smashfs_compression_type compressor_type (struct compressor *compressor);
int compressor_compress (struct compressor *compressor, void *src, unsigned int ssize, void *dst, unsigned int dsize);
int compressor_uncompress (struct compressor *compressor, void *src, unsigned int ssize, void *dst, unsigned int dsize);
