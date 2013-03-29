
#define SMASHFS_MKTAG(a, b, c, d)	(((a) << 0x18) | ((b) << 0x10) | ((c) << 0x08) | ((d) << 0x00))
#define SMASHFS_MAGIC			SMASHFS_MKTAG('S', 'M', 'S', 'H')
#define SMASHFS_VERSION_0		SMASHFS_MKTAG('V', '0', '0', '0')

#define SMASHFS_START			0
#define SMASHFS_NAME_LEN		256

enum smashfs_inode_type {
	smashfs_inode_type_unknown,
	smashfs_inode_type_socket,
	smashfs_inode_type_symbolic_link,
	smashfs_inode_type_regular_file,
	smashfs_inode_type_block_device,
	smashfs_inode_type_directory,
	smashfs_inode_type_character_device,
	smashfs_inode_type_fifo,
	smashfs_inode_type_last,
};

struct smashfs_super {
	uint32_t magic;
	uint32_t version;
	uint32_t ctime;
	uint32_t inodes;
	uint32_t block_size;
	uint32_t block_log2;
};

struct smashfs_inode {
	uint32_t type;
	uint32_t number;
	uint32_t mode;
	uint32_t nlink;
	uint32_t uid;
	uint32_t gui;
	uint32_t rdev;
	uint32_t size;
	uint32_t blksize;
	uint32_t blocks;
	uint32_t atime;
	uint32_t mtime;
	uint32_t ctime;
};

struct smashfs_inode_socket {
	struct smashfs_inode inode;
};

struct smashfs_inode_symbolic_link {
	struct smashfs_inode inode;
};

struct smashfs_inode_regular_file {
	struct smashfs_inode inode;
};

struct smashfs_inode_block_device {
	struct smashfs_inode inode;
};

struct smashfs_inode_directory {
	struct smashfs_inode inode;
};

struct smashfs_inode_character_device {
	struct smashfs_inode inode;
};

struct smashfs_inode_fifo {
	struct smashfs_inode inode;
};
