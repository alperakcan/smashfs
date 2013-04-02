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
	uint32_t inodes_offset;
	uint32_t inodes_size;
	uint32_t entries_offset;
	uint32_t entries_size;
	struct {
		struct {
			uint32_t type;
			uint32_t owner_mode;
			uint32_t group_mode;
			uint32_t other_mode;
			uint32_t uid;
			uint32_t gid;
			uint32_t ctime;
			uint32_t mtime;
			uint32_t size;
			uint32_t block;
			uint32_t index;
			struct {
				char content[0];
			} regular_file;
			struct {
				uint32_t parent;
				uint32_t nentries;
				struct {
					uint32_t number;
					char path[0];
				} entries;
			} directory;
			struct {
				char path[0];
			} symbolic_link;
		} inode;
	} bits;
	struct {
		struct {
			uint32_t ctime;
			uint32_t mtime;
		} inode;
	} min;
} __attribute__((packed));
