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
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "../include/smashfs.h"

#include "uthash.h"
#include "buffer.h"
#include "bitbuffer.h"
#include "compressor.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static int debug			= 0;
static char *source			= NULL;
static char *output			= NULL;
static struct compressor *compressor	= NULL;

struct buffer inode_buffer		= BUFFER_INITIALIZER;
struct buffer block_buffer		= BUFFER_INITIALIZER;
struct buffer entry_buffer		= BUFFER_INITIALIZER;
struct smashfs_super_block super;

long long max_inode_size;
long long max_block_size;

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
};

struct block {
	long long offset;
	long long size;
	long long compressed_size;
};

static int node_fill (long long number, struct node *node)
{
	int rc;
	struct bitbuffer bitbuffer;
	rc = bitbuffer_init_from_buffer(&bitbuffer, buffer_buffer(&inode_buffer), buffer_length(&inode_buffer));
	if (rc != 0) {
		fprintf(stderr, "bitbuffer init from buffer failed\n");
		return -1;
	}
	bitbuffer_setpos(&bitbuffer, number * max_inode_size);
	node->number     = number;
	node->type       = bitbuffer_getbits(&bitbuffer, super.bits.inode.type);
	node->owner_mode = bitbuffer_getbits(&bitbuffer, super.bits.inode.owner_mode);
	node->group_mode = bitbuffer_getbits(&bitbuffer, super.bits.inode.group_mode);
	node->other_mode = bitbuffer_getbits(&bitbuffer, super.bits.inode.other_mode);
	node->uid        = bitbuffer_getbits(&bitbuffer, super.bits.inode.uid);
	node->gid        = bitbuffer_getbits(&bitbuffer, super.bits.inode.gid);
	node->ctime      = bitbuffer_getbits(&bitbuffer, super.bits.inode.ctime);
	node->mtime      = bitbuffer_getbits(&bitbuffer, super.bits.inode.mtime);
	node->size       = bitbuffer_getbits(&bitbuffer, super.bits.inode.size);
	node->block      = bitbuffer_getbits(&bitbuffer, super.bits.inode.block);
	node->index      = bitbuffer_getbits(&bitbuffer, super.bits.inode.index);
	bitbuffer_uninit(&bitbuffer);
	if (super.bits.inode.group_mode == 0) {
		node->group_mode = node->owner_mode;
	}
	if (super.bits.inode.other_mode == 0) {
		node->other_mode = node->owner_mode;
	}
	if (super.bits.inode.uid == 0) {
		node->uid = 0;
	}
	if (super.bits.inode.gid == 0) {
		node->gid = 0;
	}
	if (super.bits.inode.ctime == 0) {
		node->ctime = super.ctime;
	}
	if (super.bits.inode.mtime == 0) {
		node->mtime = node->ctime;
	}
	return 0;
}

static int block_fill (long long number, struct block *block)
{
	int rc;
	struct bitbuffer bitbuffer;
	rc = bitbuffer_init_from_buffer(&bitbuffer, buffer_buffer(&block_buffer), buffer_length(&block_buffer));
	if (rc != 0) {
		fprintf(stderr, "bitbuffer init from buffer failed\n");
		return -1;
	}
	bitbuffer_setpos(&bitbuffer, number * max_block_size);
	block->offset  = bitbuffer_getbits(&bitbuffer, super.bits.block.offset);
	block->size  = bitbuffer_getbits(&bitbuffer, super.bits.block.size);
	block->compressed_size  = bitbuffer_getbits(&bitbuffer, super.bits.block.compressed_size);
	bitbuffer_uninit(&bitbuffer);
	return 0;
}

static int block_read (long long offset, long long size, void *dst, unsigned int dsize)
{
	int rc;
	rc = compressor_uncompress(compressor, buffer_buffer(&entry_buffer) + offset, size, dst, dsize);
	if (rc < 0) {
		fprintf(stderr, "compressor uncompress failed\n");
		return -1;
	}
	return 0;
}

static void traverse (long long inode, const char *name, long long level)
{
	int rc;
	int fd;
	mode_t mode;
	long long e;
	long long l;
	long long s;
	long long b;
	long long i;
	long long directory_parent;
	long long directory_nentries;
	long long directory_entry_number;
	unsigned char *buffer;
	unsigned char *bbuffer;
	unsigned char *nbuffer;
	struct node node;
	struct block block;
	struct bitbuffer bitbuffer;
	rc = node_fill(inode, &node);
	if (rc != 0) {
		fprintf(stderr, "node fill failed\n");
		return;
	}
	rc = block_fill(node.block, &block);
	if (rc != 0) {
		fprintf(stderr, "block fill failed\n");
		return;
	}
	if (debug > 1) {
		fprintf(stdout, "  ");
		for (l = 0; l < level; l++) {
			fprintf(stdout, "  ");
		}
		fprintf(stdout, "%s %s [number: %lld",
				(node.type == smashfs_inode_type_directory) ? "(d)" :
				(node.type == smashfs_inode_type_regular_file) ? "(f)" :
				(node.type == smashfs_inode_type_symbolic_link) ? "(l)" :
				"?",
				name,
				node.number);
	}
	mode = 0;
	if (node.owner_mode & smashfs_inode_mode_read) {
		mode |= S_IRUSR;
	}
	if (node.owner_mode & smashfs_inode_mode_write) {
		mode |= S_IWUSR;
	}
	if (node.owner_mode & smashfs_inode_mode_execute) {
		mode |= S_IXUSR;
	}
	if (node.group_mode & smashfs_inode_mode_read) {
		mode |= S_IRGRP;
	}
	if (node.group_mode & smashfs_inode_mode_write) {
		mode |= S_IWGRP;
	}
	if (node.group_mode & smashfs_inode_mode_execute) {
		mode |= S_IXGRP;
	}
	if (node.other_mode & smashfs_inode_mode_read) {
		mode |= S_IROTH;
	}
	if (node.other_mode & smashfs_inode_mode_write) {
		mode |= S_IWOTH;
	}
	if (node.other_mode & smashfs_inode_mode_execute) {
		mode |= S_IXOTH;
	}
	if (node.type == smashfs_inode_type_directory) {
		rc = mkdir(name, mode);
		if (rc != 0 && errno != EEXIST) {
			fprintf(stderr, "mkdir failed for: %s\n", name);
			return;
		}
		rc = chdir(name);
		if (rc != 0) {
			fprintf(stderr, "chdir failed\n");
			return;
		}
		s = 0;
		i = node.index;
		b = node.block;
		nbuffer = malloc(node.size);
		if (nbuffer == NULL) {
			fprintf(stderr, "malloc failed\n");
			chdir("..");
			return;
		}
		while (s < node.size) {
			rc = block_fill(b, &block);
			if (rc != 0) {
				fprintf(stderr, "block fill failed\n");
				free(nbuffer);
				chdir("..");
				return;
			}
			bbuffer = malloc(block.size);
			if (bbuffer == NULL) {
				fprintf(stderr, "malloc failed\n");
				free(bbuffer);
				free(nbuffer);
				chdir("..");
				return;
			}
			rc = block_read(block.offset, block.compressed_size, bbuffer, block.size);
			if (rc != 0) {
				fprintf(stderr, "block read failed\n");
				free(bbuffer);
				free(nbuffer);
				chdir("..");
				return;
			}
			memcpy(nbuffer + s, bbuffer + i, MIN(node.size - s, block.size - i));
			free(bbuffer);
			s += block.size;
			b += 1;
			i = 0;
		}
		buffer = nbuffer;
		bitbuffer_init_from_buffer(&bitbuffer, buffer, node.size);
		directory_parent   = bitbuffer_getbits(&bitbuffer, super.bits.inode.directory.parent);
		directory_nentries = bitbuffer_getbits(&bitbuffer, super.bits.inode.directory.nentries);
		bitbuffer_uninit(&bitbuffer);
		if (debug > 1) {
			fprintf(stdout, ", parent: %lld, nentries: %lld]\n", directory_parent, directory_nentries);
		}
		s  = super.bits.inode.directory.parent;
		s += super.bits.inode.directory.nentries;
		s  = (s + 7) / 8;
		buffer += s;
		for (e = 0; e < directory_nentries; e++) {
			s  = 0;
			s += super.bits.inode.directory.entries.number;
			s  = (s + 7) / 8;
			bitbuffer_init_from_buffer(&bitbuffer, buffer, s);
			directory_entry_number = bitbuffer_getbits(&bitbuffer, super.bits.inode.directory.entries.number);
			bitbuffer_uninit(&bitbuffer);
			buffer += s;
			traverse(directory_entry_number, (char *) buffer, level + 1);
			buffer += strlen((char *) buffer) + 1;
		}
		rc = chdir("..");
		if (rc != 0) {
			fprintf(stderr, "chdir failed\n");
			free(nbuffer);
			return;
		}
		free(nbuffer);
	} else if (node.type == smashfs_inode_type_regular_file) {
		if (debug > 1) {
			fprintf(stdout, "]\n");
		}
		unlink(name);
		fd = open(name, O_CREAT | O_TRUNC | O_WRONLY, mode);
		if (fd < 0) {
			fprintf(stderr, "open failed\n");
			return;
		}
		s = 0;
		i = node.index;
		b = node.block;
		while (s < node.size) {
			rc = block_fill(b, &block);
			if (rc != 0) {
				fprintf(stderr, "block fill failed\n");
				close(fd);
				unlink(name);
				return;
			}
			bbuffer = malloc(block.size);
			if (bbuffer == NULL) {
				fprintf(stderr, "malloc failed\n");
				close(fd);
				unlink(name);
				return;
			}
			rc = block_read(block.offset, block.compressed_size, bbuffer, block.size);
			if (rc != 0) {
				fprintf(stderr, "block read failed\n");
				free(bbuffer);
				close(fd);
				unlink(name);
				return;
			}
			rc = write(fd, bbuffer + i, MIN(node.size - s, block.size - i));
			if (rc != MIN(node.size - s, block.size - i)) {
				fprintf(stderr, "write failed, rc: %d\n", rc);
				free(bbuffer);
				close(fd);
				unlink(name);
				return;
			}
			free(bbuffer);
			s += block.size;
			b += 1;
			i = 0;
		}
		close(fd);
	} else if (node.type == smashfs_inode_type_symbolic_link) {
		if (debug > 1) {
			fprintf(stdout, ", path: %s]\n", buffer);
		}
		unlink(name);
		s = 0;
		i = node.index;
		b = node.block;
		nbuffer = malloc(node.size);
		if (nbuffer == NULL) {
			fprintf(stderr, "malloc failed\n");
			chdir("..");
			return;
		}
		while (s < node.size) {
			rc = block_fill(b, &block);
			if (rc != 0) {
				fprintf(stderr, "block fill failed\n");
				free(nbuffer);
				chdir("..");
				return;
			}
			bbuffer = malloc(block.size);
			if (bbuffer == NULL) {
				fprintf(stderr, "malloc failed\n");
				free(nbuffer);
				chdir("..");
				return;
			}
			rc = block_read(block.offset, block.compressed_size, bbuffer, block.size);
			if (rc != 0) {
				fprintf(stderr, "block read failed\n");
				free(bbuffer);
				free(nbuffer);
				chdir("..");
				return;
			}
			memcpy(nbuffer + s, bbuffer + i, MIN(node.size - s, block.size - i));
			free(bbuffer);
			s += block.size;
			b += 1;
			i = 0;
		}
		buffer = nbuffer;
		rc = symlink((char *) buffer, (char *) name);
		if (rc != 0) {
			fprintf(stderr, "symlink failed\n");
			free(nbuffer);
			return;
		}
		free(nbuffer);
	} else {
		fprintf(stderr, "unknown type: %lld\n", node.type);
		return;
	}
	chmod((char *) name, mode);
	rc = lchown((char *) name, node.uid, node.gid);
	if (rc != 0) {
		fprintf(stderr, "lchown failed\n");
	}
}

static void help_print (const char *pname)
{
	fprintf(stdout, "%s usage;\n", pname);
	fprintf(stdout, "  -s, --source     : source directory/file\n");
	fprintf(stdout, "  -o, --output     : output file\n");
	fprintf(stdout, "  -d, --debug      : enable debug output (default: %d)\n", debug);
}

int main (int argc, char *argv[])
{
	int c;
	int fd;
	int rc;
	int rb;
	unsigned int i;
	unsigned int r;
	int option_index;
	unsigned int bsize;
	unsigned char *buffer;
	char *cwd;
	static struct option long_options[] = {
		{"source"    , required_argument, 0, 's' },
		{"output"    , required_argument, 0, 'o' },
		{"debug"     , no_argument      , 0, 'd' },
		{"help"      , no_argument      , 0, 'h' },
		{ 0          , 0                , 0,  0 }
	};
	fd = -1;
	cwd = NULL;
	bsize = 0;
	buffer = NULL;
	rc = 0;
	option_index = 0;
	buffer_init(&inode_buffer);
	buffer_init(&entry_buffer);
	while ((c = getopt_long(argc, argv, "hds:o:", long_options, &option_index)) != -1) {
		switch (c) {
			case 's':
				source = strdup(optarg);
				if (source == NULL) {
					fprintf(stderr, "strdup failed for source: %s, skipping.\n", optarg);
					break;
				}
				break;
			case 'o':
				output = strdup(optarg);
				if (output == NULL) {
					fprintf(stderr, "strdup failed for output: %s, skipping.\n", optarg);
					break;
				}
				break;
			case 'd':
				debug += 1;
				break;
			case 'h':
				help_print(argv[0]);
				exit(0);
		}
	}
	if (source == NULL) {
		fprintf(stderr, "no source file specified, quiting.\n");
		rc = -1;
		goto bail;
	}
	if (output == NULL) {
		fprintf(stderr, "no output file specified, quiting.\n");
		rc = -1;
		goto bail;
	}
	fprintf(stdout, "opening file: %s\n", source);
	fd = open(source, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open failed for %s\n", source);
		rc = -1;
		goto bail;
	}
	fprintf(stdout, "reading super block\n");
	rc = read(fd, &super, sizeof(struct smashfs_super_block));
	if (rc != sizeof(struct smashfs_super_block)) {
		fprintf(stderr, "could not read super block\n");
		rc = -1;
		goto bail;
	}
	if (super.magic != SMASHFS_MAGIC) {
		fprintf(stderr, "magic mismatch\n");
		rc = -1;
		goto bail;
	}
	if (debug > 0) {
		fprintf(stdout, "  super block:\n");
		fprintf(stdout, "    magic         : 0x%08x, %u\n", super.magic, super.magic);
		fprintf(stdout, "    version       : 0x%08x, %u\n", super.version, super.version);
		fprintf(stdout, "    ctime         : 0x%08x, %u\n", super.ctime, super.ctime);
		fprintf(stdout, "    block_size    : 0x%08x, %u\n", super.block_size, super.block_size);
		fprintf(stdout, "    block_log2    : 0x%08x, %u\n", super.block_log2, super.block_log2);
		fprintf(stdout, "    inodes        : 0x%08x, %u\n", super.inodes, super.inodes);
		fprintf(stdout, "    blocks        : 0x%08x, %u\n", super.blocks, super.blocks);
		fprintf(stdout, "    root          : 0x%08x, %u\n", super.root, super.root);
		fprintf(stdout, "    inodes_offset : 0x%08x, %u\n", super.inodes_offset, super.inodes_offset);
		fprintf(stdout, "    inodes_size   : 0x%08x, %u\n", super.inodes_size, super.inodes_size);
		fprintf(stdout, "    blocks_offset : 0x%08x, %u\n", super.blocks_offset, super.blocks_offset);
		fprintf(stdout, "    blocks_size   : 0x%08x, %u\n", super.blocks_size, super.blocks_size);
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
		fprintf(stdout, "      block:\n");
		fprintf(stdout, "        offset         : %u\n", super.bits.block.offset);
		fprintf(stdout, "        size           : %u\n", super.bits.block.size);
		fprintf(stdout, "        compressed_size: %u\n", super.bits.block.compressed_size);
	}
	fprintf(stdout, "creating compressor\n");
	compressor = compressor_create_type(super.compression_type);
	if (compressor == NULL) {
		fprintf(stderr, "compressor create failed\n");
		rc = -1;
		goto bail;
	}
	fprintf(stdout, "reading inode table\n");
	bsize = 1024;
	buffer = malloc(bsize);
	if (buffer == NULL) {
		fprintf(stderr, "malloc failed\n");
		rc = -1;
		goto bail;
	}
	rc = lseek(fd, super.inodes_offset, SEEK_SET);
	if (rc != (int) super.inodes_offset) {
		fprintf(stderr, "seek failed for inodes\n");
		rc = -1;
		goto bail;
	}
	r = 0;
	while (r < super.inodes_size) {
		rc = read(fd, buffer, bsize);
		if (rc <= 0) {
			fprintf(stderr, "read failed\n");
			rc = -1;
			goto bail;
		}
		rb = buffer_add(&inode_buffer, buffer, rc);
		if (rb != rc) {
			fprintf(stderr, "buffer add failed\n");
			rc = -1;
			goto bail;
		}
		r += rc;
	}
	fprintf(stdout, "reading block table\n");
	rc = lseek(fd, super.blocks_offset, SEEK_SET);
	if (rc != (int) super.blocks_offset) {
		fprintf(stderr, "seek failed for blocks\n");
		rc = -1;
		goto bail;
	}
	r = 0;
	while (r < super.blocks_size) {
		rc = read(fd, buffer, bsize);
		if (rc <= 0) {
			fprintf(stderr, "read failed (rc: %d)\n", rc);
			rc = -1;
			goto bail;
		}
		rb = buffer_add(&block_buffer, buffer, rc);
		if (rb != rc) {
			fprintf(stderr, "buffer add failed\n");
			rc = -1;
			goto bail;
		}
		r += rc;
	}
	fprintf(stdout, "reading entries\n");
	rc = lseek(fd, super.entries_offset, SEEK_SET);
	if (rc != (int) super.entries_offset) {
		fprintf(stderr, "seek failed for entries\n");
		rc = -1;
		goto bail;
	}
	r = 0;
	while (r < super.entries_size) {
		rc = read(fd, buffer, bsize);
		if (rc <= 0) {
			fprintf(stderr, "read failed (rc: %d)\n", rc);
			rc = -1;
			goto bail;
		}
		rb = buffer_add(&entry_buffer, buffer, rc);
		if (rb != rc) {
			fprintf(stderr, "buffer add failed\n");
			rc = -1;
			goto bail;
		}
		r += rc;
	}
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
	max_block_size  = 0;
	max_block_size += super.bits.block.offset;
	max_block_size += super.bits.block.size;
	max_block_size += super.bits.block.compressed_size;
	if (debug > 2) {
		struct bitbuffer bitbuffer;
		rc = bitbuffer_init_from_buffer(&bitbuffer, buffer_buffer(&inode_buffer), buffer_length(&inode_buffer));
		if (rc != 0) {
			fprintf(stderr, "bitbuffer init from buffer failed\n");
			rc = -1;
			goto bail;
		}
		fprintf(stdout, "  inodes:\n");
		#define print_inode_bitvalue(a) { \
			long long bitvalue; \
			if (super.bits.inode.a > 0) { \
				bitvalue = bitbuffer_getbits(&bitbuffer, super.bits.inode.a); \
				fprintf(stdout, "      %-10s: %lld\n", # a, bitvalue); \
			} \
		}
		for (i = 0; i < super.inodes; i++) {
			fprintf(stdout, "    inode: %d\n", i);
			print_inode_bitvalue(type);
			print_inode_bitvalue(owner_mode);
			print_inode_bitvalue(group_mode);
			print_inode_bitvalue(other_mode);
			print_inode_bitvalue(uid);
			print_inode_bitvalue(gid);
			print_inode_bitvalue(ctime);
			print_inode_bitvalue(mtime);
			print_inode_bitvalue(size);
			print_inode_bitvalue(block);
			print_inode_bitvalue(index);
		}
		bitbuffer_uninit(&bitbuffer);
	}
	fprintf(stdout, "creating output directory: %s\n", output);
	rc = mkdir(output, 0777);
	if (rc != 0 && errno != EEXIST) {
		fprintf(stderr, "mkdir failed for: %s\n", output);
		rc = -1;
		goto bail;
	}
	cwd = malloc(1024 * 1024);
	if (cwd == NULL) {
		fprintf(stderr, "malloc failed\n");
		rc = -1;
		goto bail;
	}
	cwd = getcwd(cwd, 1024 * 1024);
	rc = chdir(output);
	if (rc != 0) {
		fprintf(stderr, "chdir failed\n");
		rc = -1;
		goto bail;
	}
	fprintf(stdout, "traversing from root\n");
	traverse(super.root, "./", 0);
	rc = chdir(cwd);
	if (rc != 0) {
		fprintf(stderr, "chdir failed\n");
		rc = -1;
		goto bail;
	}
	rc = 0;
	fprintf(stdout, "finished\n");
bail:
	close(fd);
	free(cwd);
	free(buffer);
	free(source);
	free(output);
	buffer_uninit(&entry_buffer);
	buffer_uninit(&block_buffer);
	buffer_uninit(&inode_buffer);
	compressor_destroy(compressor);
	return rc;
}
