
struct smashfs_super_info {
	int devblksize;
	int devblksize_log2;
	int block_size;
	int block_log2;
	int inodes;
	int bytes_used;
};

int smashfs_fill_super (struct super_block *sb, void *data, int silent);
