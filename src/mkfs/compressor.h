
struct compressor;

struct compressor * compressor_create (const char *id);
int compressor_destroy (struct compressor *compressor);
