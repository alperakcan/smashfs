/*
 * Copyright (c) 2013, Alper Akcan <alper.akcan@gmail.com>.
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

#define SMASHFS_MKTAG(a, b, c, d)		(((a) << 0x18) | ((b) << 0x10) | ((c) << 0x08) | ((d) << 0x00))
#define SMASHFS_MAGIC				SMASHFS_MKTAG('S', 'M', 'S', 'H')
#define SMASHFS_VERSION_0			SMASHFS_MKTAG('V', '0', '0', '0')

#define SMASHFS_START				0
#define SMASHFS_NAME_LENGTH			256

#define SMASHFS_INODE_NUMBER_LENGTH		32
#define SMASHFS_INODE_NUMBER_MAX		((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_NUMBER_LENGTH) - 1))
#define SMASHFS_INODE_NUMBER_MASK		((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_NUMBER_LENGTH) - 1))

#define SMASHFS_INODE_TYPE_LENGTH		3
#define SMASHFS_INODE_TYPE_MAX			((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_TYPE_LENGTH) - 1))
#define SMASHFS_INODE_TYPE_MASK			((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_TYPE_LENGTH) - 1))

#define SMASHFS_INODE_MODE_LENGTH		3
#define SMASHFS_INODE_MODE_MAX			((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_MODE_LENGTH) - 1))
#define SMASHFS_INODE_MODE_MASK			((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_MODE_LENGTH) - 1))

#define SMASHFS_INODE_UID_LENGTH		16
#define SMASHFS_INODE_UID_MAX			((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_UID_LENGTH) - 1))
#define SMASHFS_INODE_UID_MASK			((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_UID_LENGTH) - 1))

#define SMASHFS_INODE_GID_LENGTH		16
#define SMASHFS_INODE_GID_MAX			((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_GID_LENGTH) - 1))
#define SMASHFS_INODE_GID_MASK			((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_GID_LENGTH) - 1))

#define SMASHFS_INODE_SIZE_LENGTH		32
#define SMASHFS_INODE_SIZE_MAX			((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_SIZE_LENGTH) - 1))
#define SMASHFS_INODE_SIZE_MASK			((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_SIZE_LENGTH) - 1))

#define SMASHFS_INODE_SIZE			(SMASHFS_INODE_NUMBER_LENGTH + \
						 SMASHFS_INODE_TYPE_LENGTH + \
						 SMASHFS_INODE_MODE_LENGTH + \
						 SMASHFS_INODE_MODE_LENGTH + \
						 SMASHFS_INODE_MODE_LENGTH + \
						 SMASHFS_INODE_UID_LENGTH + \
						 SMASHFS_INODE_GID_LENGTH + \
						 SMASHFS_INODE_SIZE_LENGTH)

#define SMASHFS_INODE_PADDING_LENGTH		((((SMASHFS_INODE_SIZE + 31) / 32) * 32) - SMASHFS_INODE_SIZE)

#define SMASHFS_INODE_DIRECTORY_ENTRIES_LENGTH	32
#define SMASHFS_INODE_DIRECTORY_ENTRIES__MAX	((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_DIRECTORY_ENTRIES_LENGTH) - 1))
#define SMASHFS_INODE_DIRECTORY_ENTRIES__MASK	((unsigned int) (((unsigned long long) 1 << SMASHFS_INODE_DIRECTORY_ENTRIES_LENGTH) - 1))

enum smashfs_inode_type {
	smashfs_inode_type_regular_file		= 0x01,
	smashfs_inode_type_directory		= 0x02,
	smashfs_inode_type_character_device	= 0x03,
	smashfs_inode_type_block_device		= 0x04,
	smashfs_inode_type_symbolic_link	= 0x05,
	smashfs_inode_type_fifo			= 0x06,
	smashfs_inode_type_socket		= 0x07,
	smashfs_inode_type_mask			= 0x07
};

enum smashfs_inode_mode {
	smashfs_inode_mode_read			= 0x01,
	smashfs_inode_mode_write		= 0x02,
	smashfs_inode_mode_execute		= 0x04,
	smashfs_inode_mode_mask			= 0x07
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
	uint64_t number				: SMASHFS_INODE_NUMBER_LENGTH;
	uint64_t type				: SMASHFS_INODE_TYPE_LENGTH;
	uint64_t owner_mode			: SMASHFS_INODE_MODE_LENGTH;
	uint64_t group_mode			: SMASHFS_INODE_MODE_LENGTH;
	uint64_t other_mode			: SMASHFS_INODE_MODE_LENGTH;
	uint64_t uid				: SMASHFS_INODE_UID_LENGTH;
	uint64_t gid				: SMASHFS_INODE_GID_LENGTH;
	uint64_t size				: SMASHFS_INODE_SIZE_LENGTH;
#if (SMASHFS_INODE_PADDING_LENGTH > 0)
	uint64_t padding			: SMASHFS_INODE_PADDING_LENGTH;
#endif
} __attribute__((packed));

struct smashfs_inode_regular_file {
};

struct smashfs_inode_directory_entry {
	uint64_t number				: SMASHFS_INODE_NUMBER_LENGTH;
	char name[0];
} __attribute__((packed));

struct smashfs_inode_directory {
	uint64_t parent				: SMASHFS_INODE_NUMBER_LENGTH;
	uint64_t nentries			: SMASHFS_INODE_DIRECTORY_ENTRIES_LENGTH;
	struct smashfs_inode_directory_entry entries[0];
};

struct smashfs_inode_character_device {
};

struct smashfs_inode_block_device {
};

struct smashfs_inode_symbolic_link {
};

struct smashfs_inode_fifo {
};

struct smashfs_inode_socket {
};
