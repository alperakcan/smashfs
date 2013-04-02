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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fts.h>

#include <sys/queue.h>

#include "../include/smashfs.h"

#include "uthash.h"
#include "buffer.h"
#include "bitbuffer.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

struct item {
	char *path;
};

struct source {
	LIST_ENTRY(source) sources;
	char *path;
};

struct node_regular_file {
	long long size;
	char content[0];
};

struct node_directory_entry {
	long long number;
	char name[0];
};

struct node_directory {
	long long parent;
	long long nentries;
	struct node_directory_entry entries[0];
};

struct node_symbolic_link {
	char path[0];
};

struct node {
	long long number;
	long long type;
	long long owner_mode;
	long long group_mode;
	long long other_mode;
	long long uid;
	long long gid;
	long long ctime;
	long long mtime;
	long long size;
	long long block;
	long long index;
	union {
		void *pointer;
		struct node_regular_file *regular_file;
		struct node_directory *directory;
		struct node_symbolic_link *symbolic_link;
	};
	UT_hash_handle hh;
};

static LIST_HEAD(sources, source) sources;

static unsigned long long nduplicates		= 0;
static unsigned long long nregular_files	= 0;
static unsigned long long ndirectories		= 0;
static unsigned long long nsymbolic_links	= 0;
static unsigned long long nodes_id		= 0;
static struct node *nodes_table			= NULL;

static int debug				= 0;
static char *output				= NULL;
static unsigned int block_size			= 1024 * 1024;

static int no_group_mode			= 0;
static int no_other_mode			= 0;
static int no_uid				= 0;
static int no_gid				= 0;
static int no_ctime				= 0;
static int no_mtime				= 0;

static unsigned int slog (unsigned int block)
{
	unsigned int i;
	for (i = 12; i <= 20; i++) {
		if (block == (unsigned int) (1 << i)) {
			return i;
		}
	}
	return 0;
}

static unsigned long long blog (long long number)
{
	unsigned long long i;
	if (number < 0) {
		return 0;
	}
	for (i = 1; i < 64; i++) {
		if (number < ((long long) 1 << i)) {
			break;
		}
	}
	return i;
}

static int nodes_sort_by_number (struct node *a, struct node *b)
{
	if (a->number == b->number) {
		return 0;
	}
	return (a->number < b->number) ? -1 : 1;
}

static int nodes_sort_by_type (struct node *a, struct node *b)
{
	if (a->type == b->type) {
		return 0;
	}
	return (a->type < b->type) ? -1 : 1;
}

static int output_write (void)
{
	int fd;

	ssize_t rc;
	ssize_t size;

	long long e;
	long long s;

	struct node *node;
	struct node *nnode;

	struct smashfs_super_block super;

	long long offset;
	long long index;
	long long block;

	long long min_inode_ctime;
	long long min_inode_mtime;

	long long max_inode_size;
	long long max_inode_number;
	long long max_inode_type;
	long long max_inode_owner_mode;
	long long max_inode_group_mode;
	long long max_inode_other_mode;
	long long max_inode_uid;
	long long max_inode_gid;
	long long max_inode_ctime;
	long long max_inode_mtime;
	long long max_inode_block;
	long long max_inode_index;

	long long max_inode_directory_parent;
	long long max_inode_directory_nentries;
	long long max_inode_directory_entries_number;

	struct buffer inode_buffer;
	struct buffer entry_buffer;
	struct buffer super_buffer;
	struct bitbuffer bitbuffer;

	fprintf(stdout, "writing file: %s\n", output);

	fprintf(stdout, "  calculating super max/min bits (1/2)\n");

	min_inode_ctime      = LONG_LONG_MAX;
	min_inode_mtime      = LONG_LONG_MAX;

	max_inode_number     = -1;
	max_inode_type       = -1;
	max_inode_owner_mode = -1;
	max_inode_group_mode = -1;
	max_inode_other_mode = -1;
	max_inode_uid        = -1;
	max_inode_gid        = -1;
	max_inode_ctime      = -1;
	max_inode_mtime      = -1;

	max_inode_directory_parent       = -1;
	max_inode_directory_nentries     = -1;
	max_inode_directory_entries_number = -1;

	HASH_ITER(hh, nodes_table, node, nnode) {
		max_inode_number     = MAX(max_inode_number, node->number);
		max_inode_type       = MAX(max_inode_type, node->type);
		max_inode_owner_mode = MAX(max_inode_owner_mode, node->owner_mode);
		max_inode_group_mode = MAX(max_inode_group_mode, node->group_mode);
		max_inode_other_mode = MAX(max_inode_other_mode, node->other_mode);
		max_inode_uid        = MAX(max_inode_uid, node->uid);
		max_inode_gid        = MAX(max_inode_gid, node->gid);
		max_inode_ctime      = MAX(max_inode_ctime, node->ctime);
		max_inode_mtime      = MAX(max_inode_mtime, node->mtime);
		min_inode_ctime      = MIN(max_inode_ctime, min_inode_ctime);
		min_inode_mtime      = MIN(max_inode_mtime, min_inode_mtime);
		if (node->type == smashfs_inode_type_regular_file) {
		} else if (node->type == smashfs_inode_type_directory) {
			max_inode_directory_parent   = MAX(max_inode_directory_parent  , node->directory->parent);
			max_inode_directory_nentries = MAX(max_inode_directory_nentries, node->directory->nentries);
			size = sizeof(struct node_directory);
			for (e = 0; e < node->directory->nentries; e++) {
				max_inode_directory_entries_number = MAX(max_inode_directory_entries_number, ((struct node_directory_entry *) (((unsigned char *) node->directory) + size))->number);
				size += sizeof(struct node_directory_entry) + strlen(((struct node_directory_entry *) (((unsigned char *) node->directory) + size))->name) + 1;
			}
		} else if (node->type == smashfs_inode_type_symbolic_link) {
		} else {
			fprintf(stderr, "unknown type: %lld\n", node->type);
		}
	}

	if (no_group_mode) { max_inode_group_mode = -1; }
	if (no_other_mode) { max_inode_other_mode = -1; }
	if (no_uid)        { max_inode_uid = -1; }
	if (no_gid)        { max_inode_gid = -1; }
	if (no_ctime)      { max_inode_ctime = -1; }
	if (no_mtime)      { max_inode_mtime = -1; }

	fprintf(stdout, "  setting super block (1/3)\n");

	super.magic      = SMASHFS_MAGIC;
	super.version    = SMASHFS_VERSION_0;
	super.ctime      = 0;
	super.block_size = block_size;
	super.block_log2 = slog(block_size);
	super.inodes     = HASH_CNT(hh, nodes_table);
	super.root       = 0;

	super.min.inode.ctime = min_inode_ctime;
	super.min.inode.mtime = min_inode_mtime;

	super.bits.inode.type       = blog(max_inode_type);
	super.bits.inode.owner_mode = blog(max_inode_owner_mode);
	super.bits.inode.group_mode = blog(max_inode_group_mode);
	super.bits.inode.other_mode = blog(max_inode_other_mode);
	super.bits.inode.uid        = blog(max_inode_uid);
	super.bits.inode.gid        = blog(max_inode_gid);
	super.bits.inode.ctime      = blog(max_inode_ctime - min_inode_ctime);
	super.bits.inode.mtime      = blog(max_inode_mtime - min_inode_mtime);

	super.bits.inode.directory.parent         = blog(max_inode_directory_parent);
	super.bits.inode.directory.nentries       = blog(max_inode_directory_nentries);
	super.bits.inode.directory.entries.number = blog(max_inode_directory_entries_number);

	fprintf(stdout, "  sorting inodes table by type\n");

	HASH_SRT(hh, nodes_table, nodes_sort_by_type);

	fprintf(stdout, "  filling entry blocks\n");

	offset = 0;
	buffer_init(&entry_buffer);
	HASH_ITER(hh, nodes_table, node, nnode) {
		if (node->type == smashfs_inode_type_regular_file) {
			rc = buffer_add(&entry_buffer, node->regular_file->content, node->regular_file->size);
			if (rc < 0) {
				fprintf(stdout, "buffer add failed\n");
				buffer_uninit(&entry_buffer);
				return -1;
			}
			node->size = rc;
			index = offset & ((1 << super.block_log2) - 1);
			block = offset >> super.block_log2;
			node->block = block;
			node->index = index;
			offset += node->size;
		} else if (node->type == smashfs_inode_type_directory) {
			size  = 0;
			size += super.bits.inode.directory.parent;
			size += super.bits.inode.directory.nentries;
			size  = (size + 7) / 8;
			rc = bitbuffer_init(&bitbuffer, size);
			if (rc != 0) {
				fprintf(stderr, "bitbuffer init failed\n");
				buffer_uninit(&entry_buffer);
				return -1;
			}
			bitbuffer_putbits(&bitbuffer, super.bits.inode.directory.parent  , node->directory->parent);
			bitbuffer_putbits(&bitbuffer, super.bits.inode.directory.nentries, node->directory->nentries);
			rc = buffer_add(&entry_buffer, bitbuffer_buffer(&bitbuffer), size);
			if (rc < 0) {
				fprintf(stdout, "buffer add failed\n");
				bitbuffer_uninit(&bitbuffer);
				buffer_uninit(&entry_buffer);
				return -1;
			}
			node->size = rc;
			bitbuffer_uninit(&bitbuffer);
			s = sizeof(struct node_directory);
			for (e = 0; e < node->directory->nentries; e++) {
				size  = 0;
				size += super.bits.inode.directory.entries.number;
				size  = (size + 7) / 8;
				rc = bitbuffer_init(&bitbuffer, size);
				if (rc != 0) {
					fprintf(stderr, "bitbuffer init failed\n");
					buffer_uninit(&entry_buffer);
					return -1;
				}
				bitbuffer_putbits(&bitbuffer, super.bits.inode.directory.entries.number, ((struct node_directory_entry *) (((unsigned char *) node->directory) + s))->number);
				rc = buffer_add(&entry_buffer, bitbuffer_buffer(&bitbuffer), size);
				if (rc < 0) {
					fprintf(stdout, "buffer add failed\n");
					bitbuffer_uninit(&bitbuffer);
					buffer_uninit(&entry_buffer);
					return -1;
				}
				node->size += rc;
				rc = buffer_add(&entry_buffer, ((struct node_directory_entry *) (((unsigned char *) node->directory) + s))->name, strlen(((struct node_directory_entry *) (((unsigned char *) node->directory) + s))->name) + 1);
				if (rc < 0) {
					fprintf(stdout, "buffer add failed\n");
					bitbuffer_uninit(&bitbuffer);
					buffer_uninit(&entry_buffer);
					return -1;
				}
				node->size += rc;
				bitbuffer_uninit(&bitbuffer);
				s += sizeof(struct node_directory_entry) + strlen(((struct node_directory_entry *) (((unsigned char *) node->directory) + s))->name) + 1;
			}
			index = offset & ((1 << super.block_log2) - 1);
			block = offset >> super.block_log2;
			node->block = block;
			node->index = index;
			offset += node->size;
		} else if (node->type == smashfs_inode_type_symbolic_link) {
			rc = buffer_add(&entry_buffer, node->symbolic_link->path, strlen(node->symbolic_link->path) + 1);
			if (rc < 0) {
				fprintf(stdout, "buffer add failed\n");
				buffer_uninit(&entry_buffer);
				return -1;
			}
			node->size = rc;
			index = offset & ((1 << super.block_log2) - 1);
			block = offset >> super.block_log2;
			node->block = block;
			node->index = index;
			offset += node->size;
		} else {
			fprintf(stderr, "unknown type: %lld\n", node->type);
		}
	}

	fprintf(stdout, "  calculating super max/min bits (2/2)\n");

	max_inode_size  = -1;
	max_inode_block = -1;
	max_inode_index = -1;
	HASH_ITER(hh, nodes_table, node, nnode) {
		max_inode_size  = MAX(max_inode_size, node->size);
		max_inode_block = MAX(max_inode_block, node->block);
		max_inode_index = MAX(max_inode_index, node->index);
	}

	fprintf(stdout, "  setting super block (2/3)\n");

	super.bits.inode.size  = blog(max_inode_size);
	super.bits.inode.block = blog(max_inode_block);
	super.bits.inode.index = blog(max_inode_index);

	fprintf(stdout, "  calculating inode size\n");

	max_inode_size  = 0;
	max_inode_size += super.bits.inode.type;
	max_inode_size += super.bits.inode.owner_mode;
	max_inode_size += super.bits.inode.group_mode;
	max_inode_size += super.bits.inode.other_mode;
	max_inode_size += super.bits.inode.uid;
	max_inode_size += super.bits.inode.gid;
	max_inode_size += super.bits.inode.ctime;
	max_inode_size += super.bits.inode.mtime;
	max_inode_size += super.bits.inode.size;
	max_inode_size += super.bits.inode.block;
	max_inode_size += super.bits.inode.index;
	size = (super.inodes * max_inode_size + 7) / 8;

	fprintf(stdout, "  sorting inodes table by number\n");

	HASH_SRT(hh, nodes_table, nodes_sort_by_number);

	fprintf(stdout, "  filling inodes table\n");

	buffer_init(&inode_buffer);
	rc = bitbuffer_init(&bitbuffer, size);
	if (rc != 0) {
		fprintf(stderr, "bitbuffer init failed\n");
		buffer_uninit(&inode_buffer);
		buffer_uninit(&entry_buffer);
		return -1;
	}
	HASH_ITER(hh, nodes_table, node, nnode) {
		bitbuffer_putbits(&bitbuffer, super.bits.inode.type      , node->type);
		bitbuffer_putbits(&bitbuffer, super.bits.inode.owner_mode, node->owner_mode);
		bitbuffer_putbits(&bitbuffer, super.bits.inode.group_mode, node->group_mode);
		bitbuffer_putbits(&bitbuffer, super.bits.inode.other_mode, node->other_mode);
		bitbuffer_putbits(&bitbuffer, super.bits.inode.uid       , node->uid);
		bitbuffer_putbits(&bitbuffer, super.bits.inode.gid       , node->gid);
		bitbuffer_putbits(&bitbuffer, super.bits.inode.ctime     , node->ctime - super.min.inode.ctime);
		bitbuffer_putbits(&bitbuffer, super.bits.inode.mtime     , node->mtime - super.min.inode.mtime);
		bitbuffer_putbits(&bitbuffer, super.bits.inode.size      , node->size);
		bitbuffer_putbits(&bitbuffer, super.bits.inode.block     , node->block);
		bitbuffer_putbits(&bitbuffer, super.bits.inode.index     , node->index);
		if (debug > 2) {
			fprintf(stdout, "    node: %lld, size: %lld, block: %lld, index: %lld\n", node->number, node->size, node->block, node->index);
		}
	}
	rc = buffer_add(&inode_buffer, bitbuffer_buffer(&bitbuffer), size);
	if (rc < 0) {
		fprintf(stdout, "buffer add failed\n");
		bitbuffer_uninit(&bitbuffer);
		buffer_uninit(&inode_buffer);
		buffer_uninit(&entry_buffer);
		return -1;
	}
	bitbuffer_uninit(&bitbuffer);

	fprintf(stdout, "  setting super block (1/3)\n");
	super.inodes_offset  = sizeof(struct smashfs_super_block);
	super.inodes_size    = buffer_length(&inode_buffer);
	super.entries_offset = super.inodes_offset + super.inodes_size;
	super.entries_size   = buffer_length(&entry_buffer);

	fprintf(stdout, "  filling super block\n");

	if (debug) {
		fprintf(stdout, "  super block:\n");
		fprintf(stdout, "    magic         : 0x%08x, %u\n", super.magic, super.magic);
		fprintf(stdout, "    version       : 0x%08x, %u\n", super.version, super.version);
		fprintf(stdout, "    ctime         : 0x%08x, %u\n", super.ctime, super.ctime);
		fprintf(stdout, "    block_size    : 0x%08x, %u\n", super.block_size, super.block_size);
		fprintf(stdout, "    block_log2    : 0x%08x, %u\n", super.block_log2, super.block_log2);
		fprintf(stdout, "    inodes        : 0x%08x, %u\n", super.inodes, super.inodes);
		fprintf(stdout, "    root          : 0x%08x, %u\n", super.root, super.root);
		fprintf(stdout, "    inodes_offset : 0x%08x, %u\n", super.inodes_offset, super.inodes_offset);
		fprintf(stdout, "    inodes_size   : 0x%08x, %u\n", super.inodes_size, super.inodes_size);
		fprintf(stdout, "    entries_offset: 0x%08x, %u\n", super.entries_offset, super.entries_offset);
		fprintf(stdout, "    entries_size  : 0x%08x, %u\n", super.entries_size, super.entries_size);
		fprintf(stdout, "    bits:\n");
		fprintf(stdout, "      min:\n");
		fprintf(stdout, "        ctime : 0x%08x, %u\n", super.min.inode.ctime, super.min.inode.ctime);
		fprintf(stdout, "        mtime : 0x%08x, %u\n", super.min.inode.mtime, super.min.inode.mtime);
		fprintf(stdout, "      inode:\n");
		fprintf(stdout, "        type      : %u\n", super.bits.inode.type);
		fprintf(stdout, "        owner_mode: %u\n", super.bits.inode.owner_mode);
		fprintf(stdout, "        group_mode: %u\n", super.bits.inode.group_mode);
		fprintf(stdout, "        other_mode: %u\n", super.bits.inode.other_mode);
		fprintf(stdout, "        uid       : %u\n", super.bits.inode.uid);
		fprintf(stdout, "        gid       : %u\n", super.bits.inode.gid);
		fprintf(stdout, "        ctime     : %u\n", super.bits.inode.ctime);
		fprintf(stdout, "        mtime     : %u\n", super.bits.inode.mtime);
		fprintf(stdout, "        size      : %u\n", super.bits.inode.size);
		fprintf(stdout, "        block     : %u\n", super.bits.inode.block);
		fprintf(stdout, "        index     : %u\n", super.bits.inode.index);
		fprintf(stdout, "        regular_file:\n");
		fprintf(stdout, "        directory:\n");
		fprintf(stdout, "          parent   : %u\n", super.bits.inode.directory.parent);
		fprintf(stdout, "          nentries : %u\n", super.bits.inode.directory.nentries);
		fprintf(stdout, "          entries:\n");
		fprintf(stdout, "            number : %u\n", super.bits.inode.directory.entries.number);
		fprintf(stdout, "        symbolic_link:\n");
	}

	buffer_init(&super_buffer);
	rc = buffer_add(&super_buffer, &super, sizeof(struct smashfs_super_block));
	if (rc < 0) {
		fprintf(stderr, "buffer add failed for super block\n");
		buffer_uninit(&super_buffer);
		buffer_uninit(&inode_buffer);
		buffer_uninit(&entry_buffer);
		return -1;
	}

	fprintf(stdout, "  buffers:\n");
	fprintf(stdout, "    super: %lld bytes\n", buffer_length(&super_buffer));
	fprintf(stdout, "    inode: %lld bytes\n", buffer_length(&inode_buffer));
	fprintf(stdout, "    entry: %lld bytes\n", buffer_length(&entry_buffer));
	fprintf(stdout, "    total: %lld bytes\n", buffer_length(&super_buffer) + buffer_length(&inode_buffer) + buffer_length(&entry_buffer));

	fd = open(output, O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (fd < 0) {
		fprintf(stderr, "open failed for %s\n", output);
		buffer_uninit(&super_buffer);
		buffer_uninit(&inode_buffer);
		buffer_uninit(&entry_buffer);
		return -1;
	}

	rc = write(fd, buffer_buffer(&super_buffer), buffer_length(&super_buffer));
	if (rc != buffer_length(&super_buffer)) {
		fprintf(stderr, "write failed\n");
		close(fd);
		unlink(output);
		buffer_uninit(&super_buffer);
		buffer_uninit(&inode_buffer);
		buffer_uninit(&entry_buffer);
		return -1;
	}

	rc = write(fd, buffer_buffer(&inode_buffer), buffer_length(&inode_buffer));
	if (rc != buffer_length(&inode_buffer)) {
		fprintf(stderr, "write failed\n");
		close(fd);
		unlink(output);
		buffer_uninit(&super_buffer);
		buffer_uninit(&inode_buffer);
		buffer_uninit(&entry_buffer);
		return -1;
	}

	rc = write(fd, buffer_buffer(&entry_buffer), buffer_length(&entry_buffer));
	if (rc != buffer_length(&entry_buffer)) {
		fprintf(stderr, "write failed\n");
		close(fd);
		unlink(output);
		buffer_uninit(&super_buffer);
		buffer_uninit(&inode_buffer);
		buffer_uninit(&entry_buffer);
		return -1;
	}

	close(fd);

	buffer_uninit(&super_buffer);
	buffer_uninit(&inode_buffer);
	buffer_uninit(&entry_buffer);

	return 0;
}

static int node_delete (struct node *node)
{
	HASH_DEL(nodes_table, node);
	free(node->pointer);
	free(node);
	return 0;
}

static struct node * node_new (FTSENT *entry)
{
	int fd;
	ssize_t r;
	long long e;
	long long s;
	int duplicate;
	struct node *node;
	struct stat *stbuf;
	struct node *dnode;
	struct node *ndnode;
	struct node *parent;
	struct node_directory *directory;
	struct node_directory_entry *directory_entry;
	fd = -1;
	duplicate = 0;
	stbuf = entry->fts_statp;
	node = malloc(sizeof(struct node));
	if (node == NULL) {
		fprintf(stderr, "malloc failed\n");
		return NULL;
	}
	entry->fts_pointer = node;
	node->number = nodes_id;
	node->pointer = NULL;
	if (S_ISREG(stbuf->st_mode)) {
		node->type = smashfs_inode_type_regular_file;
	} else if (S_ISDIR(stbuf->st_mode)) {
		node->type = smashfs_inode_type_directory;
	} else if (S_ISCHR(stbuf->st_mode)) {
		node->type = smashfs_inode_type_character_device;
	} else if (S_ISBLK(stbuf->st_mode)) {
		node->type = smashfs_inode_type_block_device;
	} else if (S_ISFIFO(stbuf->st_mode)) {
		node->type = smashfs_inode_type_fifo;
	} else if (S_ISLNK(stbuf->st_mode)) {
		node->type = smashfs_inode_type_symbolic_link;
	} else if (S_ISSOCK(stbuf->st_mode)) {
		node->type = smashfs_inode_type_socket;
	} else {
		fprintf(stderr, "unknown mode: 0x%08x\n", stbuf->st_mode);
		free(node);
		return NULL;
	}
	node->owner_mode = 0;
	if (stbuf->st_mode & S_IRUSR) {
		node->owner_mode |= smashfs_inode_mode_read;
	}
	if (stbuf->st_mode & S_IWUSR) {
		node->owner_mode |= smashfs_inode_mode_write;
	}
	if (stbuf->st_mode & S_IXUSR) {
		node->owner_mode |= smashfs_inode_mode_execute;
	}
	node->group_mode = 0;
	if (stbuf->st_mode & S_IRGRP) {
		node->group_mode |= smashfs_inode_mode_read;
	}
	if (stbuf->st_mode & S_IWGRP) {
		node->group_mode |= smashfs_inode_mode_write;
	}
	if (stbuf->st_mode & S_IXGRP) {
		node->group_mode |= smashfs_inode_mode_execute;
	}
	node->other_mode = 0;
	if (stbuf->st_mode & S_IROTH) {
		node->other_mode |= smashfs_inode_mode_read;
	}
	if (stbuf->st_mode & S_IWOTH) {
		node->other_mode |= smashfs_inode_mode_write;
	}
	if (stbuf->st_mode & S_IXOTH) {
		node->other_mode |= smashfs_inode_mode_execute;
	}
	node->uid = stbuf->st_uid;
	node->gid = stbuf->st_gid;
	node->ctime = stbuf->st_ctime;
	node->mtime = stbuf->st_mtime;
	if (node->type == smashfs_inode_type_regular_file) {
		fd = open(entry->fts_accpath, O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "open failed\n");
			goto bail;
		}
		node->regular_file = malloc(sizeof(struct node_regular_file) + stbuf->st_size);
		if (node->regular_file == NULL) {
			fprintf(stderr, "malloc failed\n");
			goto bail;
		}
		node->regular_file->size = stbuf->st_size;
		r = read(fd, node->regular_file->content, node->regular_file->size);
		if (r != (ssize_t) node->regular_file->size) {
			fprintf(stderr, "read failed path: %s, size %lld, ret: %zd\n", entry->fts_accpath, node->regular_file->size, r);
			goto bail;
		}
		HASH_ITER(hh, nodes_table, dnode, ndnode) {
			if (dnode->type != smashfs_inode_type_regular_file) {
				continue;
			}
			if (dnode->regular_file->size != node->regular_file->size) {
				continue;
			}
			if (memcmp(dnode->regular_file->content, node->regular_file->content, node->regular_file->size) != 0) {
				continue;
			}
			free(node->pointer);
			free(node);
			node = dnode;
			duplicate = 1;
			break;
		}
		close(fd);
		fd = -1;
	} else if (node->type == smashfs_inode_type_directory) {
		node->directory = malloc(sizeof(struct node_directory));
		if (node->directory == NULL) {
			fprintf(stderr, "malloc failed\n");
			goto bail;
		}
		node->directory->parent = 0;
		node->directory->nentries = 0;
		parent = entry->fts_parent->fts_pointer;
		if (parent == NULL) {
			goto out;
		}
		node->directory->parent = parent->number;
	} else if (node->type == smashfs_inode_type_symbolic_link) {
		node->symbolic_link = malloc(sizeof(struct node_symbolic_link) + stbuf->st_size + 1);
		if (node->symbolic_link == NULL) {
			fprintf(stderr, "malloc failed\n");
			goto bail;
		}
		r = readlink(entry->fts_accpath, node->symbolic_link->path, stbuf->st_size);
		if (r < 0) {
			fprintf(stderr, "readlink failed\n");
			goto bail;
		}
		if (r > stbuf->st_size) {
			fprintf(stderr, "readlink failed\n");
			goto bail;
		}
		node->symbolic_link->path[r] = '\0';
		HASH_ITER(hh, nodes_table, dnode, ndnode) {
			if (dnode->type != smashfs_inode_type_symbolic_link) {
				continue;
			}
			if (strcmp(dnode->symbolic_link->path, node->symbolic_link->path) != 0) {
				continue;
			}
			free(node->pointer);
			free(node);
			node = dnode;
			duplicate = 1;
			break;
		}
	} else {
		fprintf(stderr, "unknown node type: %lld\n", node->type);
		goto bail;
	}
	parent = entry->fts_parent->fts_pointer;
	if (parent == NULL) {
		goto out;
	}
	if (parent->type != smashfs_inode_type_directory) {
		fprintf(stderr, "parent is not a directory\n");
		goto bail;
	}
	directory = parent->directory;
	s = sizeof(struct node_directory);
	for (e = 0; e < directory->nentries; e++) {
		directory_entry = (struct node_directory_entry *) (((unsigned char *) directory) + s);
		s += sizeof(struct node_directory_entry) + strlen(directory_entry->name) + 1;
	}
	directory = malloc(s + sizeof(struct node_directory_entry) + strlen(entry->fts_name) + 1);
	if (directory == NULL) {
		fprintf(stderr, "malloc failed\n");
		goto bail;
	}
	memcpy(directory, parent->directory, s);
	directory_entry = (struct node_directory_entry *) (((unsigned char *) directory) + s);
	directory_entry->number = node->number;
	memcpy(directory_entry->name, entry->fts_name, strlen(entry->fts_name) + 1);
	directory->nentries += 1;
	free(parent->directory);
	parent->directory = directory;
out:
	if (duplicate == 0) {
		HASH_ADD(hh, nodes_table, number, sizeof(node->number), node);
		nodes_id += 1;
	} else {
		nduplicates += 1;
	}
	if (node->type == smashfs_inode_type_regular_file) {
		nregular_files += 1;
	}
	if (node->type == smashfs_inode_type_directory) {
		ndirectories += 1;
	}
	if (node->type == smashfs_inode_type_symbolic_link) {
		nsymbolic_links += 1;
	}
	return node;
bail:
	close(fd);
	if (duplicate == 0) {
		free(node->pointer);
		free(node);
	}
	return NULL;
}

static void sources_scan (void)
{
	FTS *tree;
	FTSENT *entry;
	char **spaths;
	struct node *node;
	struct source *source;
	unsigned int nsources;
	nsources = 0;
	LIST_FOREACH(source, &sources, sources) {
		nsources += 1;
	}
	fprintf(stdout, "scanning sources: %d\n", nsources);
	spaths = malloc(sizeof(char *) * (nsources + 1));
	if (spaths == NULL) {
		fprintf(stderr, "malloc failed\n");
		return;
	}
	nsources = 0;
	LIST_FOREACH(source, &sources, sources) {
		fprintf(stdout, "  setting path: %d, as: %s\n", nsources, source->path);
		spaths[nsources] = source->path;
		nsources += 1;
	}
	spaths[nsources] = NULL;
	tree = fts_open(spaths, FTS_COMFOLLOW /* | FTS_NOCHDIR */ | FTS_PHYSICAL /* | FTS_SEEDOT */, NULL);
	if (tree == NULL) {
		fprintf(stderr, "fts_open failed\n");
		free(spaths);
		return;
	}
	fprintf(stdout, "  traversing source paths\n");
	while ((entry = fts_read(tree))) {
		if (entry->fts_info == FTS_NSOK) {
			fprintf(stderr, "A file for which no stat(2) information was\n"
					"requested. The contents of the fts_statp field are\n"
					"undefined.\n");
			fprintf(stderr, "  path: %s\n", entry->fts_path);
			fprintf(stderr, "  info: 0x%08x\n", entry->fts_info);
			continue;
		}
		if (entry->fts_info == FTS_ERR) {
			fprintf(stderr, "This is an error return, and the fts_errno field\n"
					"will be set to indicate what caused the error.\n");
			fprintf(stderr, "  path : %s\n", entry->fts_path);
			fprintf(stderr, "  errno: %d\n", entry->fts_errno);
			continue;
		}
		if (entry->fts_info == FTS_NS) {
			fprintf(stderr, "A file for which no stat(2) information was\n"
					"available. The contents of the fts_statp field are\n"
					"undefined. This is an error return, and the\n"
					"fts_errno field will be set to indicate what caused\n"
					"the error.\n");
			fprintf(stderr, "  path : %s\n", entry->fts_path);
			fprintf(stderr, "  errno: %d\n", entry->fts_errno);
			continue;
		}
		if (entry->fts_info == FTS_DNR) {
			fprintf(stderr, "A directory which cannot be read. This is an error\n"
					"return, and the fts_errno field will be set to\n"
					"indicate what caused the error.\n");
			fprintf(stderr, "  path : %s\n", entry->fts_path);
			fprintf(stderr, "  errno: %d\n", entry->fts_errno);
			continue;
		}
		if (entry->fts_info == FTS_DP) {
			continue;
		}
		node = node_new(entry);
		if (node == NULL) {
			fprintf(stderr, "node new failed\n");
			continue;
		}
		if (debug > 1) {
			int l;
			struct node *parent;
			parent = entry->fts_parent->fts_pointer;
			for (l = 0; l < entry->fts_level; l++) {
				fprintf(stdout, " ");
			}
			fprintf(stdout, "  %s %s (node: %lld, parent: %lld)\n",
					(node->type == smashfs_inode_type_regular_file) ? "(f)" :
					  (node->type == smashfs_inode_type_directory) ? "(d)" :
					  (node->type == smashfs_inode_type_symbolic_link) ? "(s)" :
					  "?",
					entry->fts_name,
					node->number,
					(parent != NULL) ? parent->number : -1);
		}
	}
	fts_close(tree);
	free(spaths);
	fprintf(stdout, "  found %d nodes\n", HASH_CNT(hh, nodes_table));
}

static void help_print (const char *pname)
{
	fprintf(stdout, "%s usage;\n", pname);
	fprintf(stdout, "  -s, --source     : source directory/file\n");
	fprintf(stdout, "  -o, --output     : output file\n");
	fprintf(stdout, "  -b, --block_size : block size (default: %d)\n", block_size);
	fprintf(stdout, "  -d, --debug      : enable debug output (default: %d)\n", debug);
	fprintf(stdout, "  --no_group_mode  : disable group mode\n");
	fprintf(stdout, "  --no_other_mode  : disable other mode\n");
	fprintf(stdout, "  --no_uid         : disable uid\n");
	fprintf(stdout, "  --no_gid         : disable gid\n");
	fprintf(stdout, "  --no_ctime       : disable ctime\n");
	fprintf(stdout, "  --no_mtime       : disable mtime\n");
}

int main (int argc, char *argv[])
{
	int c;
	int rc;
	int option_index;
	struct node *node;
	struct node *nnode;
	struct source *source;
	unsigned int nsources;
	static struct option long_options[] = {
		{"source"       , required_argument, 0, 's' },
		{"output"       , required_argument, 0, 'o' },
		{"block_size"   , required_argument, 0, 'b' },
		{"debug"        , no_argument      , 0, 'd' },
		{"no_group_mode", no_argument      , 0, 0x100 },
		{"no_other_mode", no_argument      , 0, 0x101 },
		{"no_uid"       , no_argument      , 0, 0x102 },
		{"no_gid"       , no_argument      , 0, 0x103 },
		{"no_ctime"     , no_argument      , 0, 0x104 },
		{"no_mtime"     , no_argument      , 0, 0x105 },
		{"help"         , no_argument      , 0, 'h' },
		{ 0             , 0                , 0,  0 }
	};
	rc = 0;
	option_index = 0;
	while ((c = getopt_long(argc, argv, "hds:o:b:g:t:", long_options, &option_index)) != -1) {
		switch (c) {
			case 's':
				source = malloc(sizeof(struct source));
				if (source == NULL) {
					fprintf(stderr, "malloc failed for source: %s, skipping.\n", optarg);
					break;
				}
				memset(source, 0, sizeof(struct source));
				source->path = strdup(optarg);
				if (source->path == NULL) {
					fprintf(stderr, "strdup failed for path: %s, skipping.\n", optarg);
					free(source);
					break;
				}
				LIST_INSERT_HEAD(&sources, source, sources);
				break;
			case 'o':
				output = strdup(optarg);
				if (output == NULL) {
					fprintf(stderr, "strdup failed for output: %s, skipping.\n", optarg);
					break;
				}
				break;
			case 'b':
				block_size = atoi(optarg);
				block_size = MIN(block_size, 1 << 20);
				block_size = MAX(block_size, 1 << 12);
				break;
			case 'd':
				debug += 1;
				break;
			case 0x100:
				no_group_mode = 1;
				break;
			case 0x101:
				no_other_mode = 1;
				break;
			case 0x102:
				no_uid = 1;
				break;
			case 0x103:
				no_gid = 1;
				break;
			case 0x104:
				no_ctime = 1;
				break;
			case 0x105:
				no_mtime = 1;
				break;
			case 'h':
				help_print(argv[0]);
				exit(0);
		}
	}
	nsources = 0;
	LIST_FOREACH(source, &sources, sources) {
		nsources += 1;
	}
	if (nsources == 0) {
		fprintf(stderr, "no source directory/file specified, quiting.\n");
		rc = -1;
		goto bail;
	}
	if (output == NULL) {
		fprintf(stderr, "no output file specified, quiting.\n");
		rc = -1;
		goto bail;
	}
	sources_scan();
	output_write();
	fprintf(stdout, "statistics:\n");
	fprintf(stdout, "  duplicates    : %lld\n", nduplicates);
	fprintf(stdout, "  regular_files : %lld\n", nregular_files);
	fprintf(stdout, "  directories   : %lld\n", ndirectories);
	fprintf(stdout, "  symbolic_links: %lld\n", nsymbolic_links);
bail:
	while (sources.lh_first != NULL) {
		source = sources.lh_first;;
		LIST_REMOVE(source, sources);
		free(source->path);
		free(source);
	}
	HASH_ITER(hh, nodes_table, node, nnode) {
		node_delete(node);
	}
	free(output);
	return rc;
}
