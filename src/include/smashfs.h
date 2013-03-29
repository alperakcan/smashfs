
#define SMASHFS_MKTAG(a, b, c, d)	(((a) << 0x18) | ((b) << 0x10) | ((c) << 0x08) | ((d) << 0x00))
#define SMASHFS_MAGIC			SMASHFS_MKTAG('S', 'M', 'S', 'H')
#define SMASHFS_VERSION_0		SMASHFS_MKTAG('V', '0', '0', '0')

#define SMASHFS_START			0
#define SMASHFS_NAME_LEN		255

#define SMASHFS_INODE_NUMBER_LENGTH	32
#define SMASHFS_INODE_NUMBER_MAX	((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_NUMBER_LENGTH) - 1))
#define SMASHFS_INODE_NUMBER_MASK	((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_NUMBER_LENGTH) - 1))

#define SMASHFS_INODE_TYPE_LENGTH	3
#define SMASHFS_INODE_TYPE_MAX		((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_TYPE_LENGTH) - 1))
#define SMASHFS_INODE_TYPE_MASK		((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_TYPE_LENGTH) - 1))

#define SMASHFS_INODE_MODE_LENGTH	3
#define SMASHFS_INODE_MODE_MAX		((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_MODE_LENGTH) - 1))
#define SMASHFS_INODE_MODE_MASK		((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_MODE_LENGTH) - 1))

#define SMASHFS_INODE_UID_LENGTH	16
#define SMASHFS_INODE_UID_MAX		((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_UID_LENGTH) - 1))
#define SMASHFS_INODE_UID_MASK		((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_UID_LENGTH) - 1))

#define SMASHFS_INODE_GID_LENGTH	16
#define SMASHFS_INODE_GID_MAX		((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_GID_LENGTH) - 1))
#define SMASHFS_INODE_GID_MASK		((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_GID_LENGTH) - 1))

#define SMASHFS_INODE_SIZE_LENGTH	32
#define SMASHFS_INODE_SIZE_MAX		((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_SIZE_LENGTH) - 1))
#define SMASHFS_INODE_SIZE_MASK		((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_SIZE_LENGTH) - 1))

enum smashfs_inode_type {
	smashfs_inode_type_regular_file      = 0x01,
	smashfs_inode_type_directory         = 0x02,
	smashfs_inode_type_character_device  = 0x03,
	smashfs_inode_type_block_device      = 0x04,
	smashfs_inode_type_symbolic_link     = 0x05,
	smashfs_inode_type_fifo              = 0x06,
	smashfs_inode_type_socket            = 0x07,
	smashfs_inode_type_mask              = 0x07
};

enum smashfs_inode_mode {
	smashfs_inode_mode_read              = 0x01,
	smashfs_inode_mode_write             = 0x02,
	smashfs_inode_mode_execute           = 0x04,
	smashfs_inode_mode_mask              = 0x07
};

struct smashfs_super_block {
	uint32_t magic;
	uint32_t version;
	uint32_t ctime;
	uint32_t inodes;
	uint32_t block_size;
	uint32_t block_log2;
	uint32_t root;
};

struct smashfs_inode {
	uint32_t number        : SMASHFS_INODE_NUMBER_LENGTH;
	uint32_t type          : SMASHFS_INODE_TYPE_LENGTH;
	uint32_t owner_mode    : SMASHFS_INODE_MODE_LENGTH;
	uint32_t group_mode    : SMASHFS_INODE_MODE_LENGTH;
	uint32_t other_mode    : SMASHFS_INODE_MODE_LENGTH;
	uint32_t uid           : SMASHFS_INODE_UID_LENGTH;
	uint32_t gid           : SMASHFS_INODE_GID_LENGTH;
	uint32_t size          : SMASHFS_INODE_SIZE_LENGTH;
} __attribute__((packed));

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
