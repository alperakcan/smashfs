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

#include "../include/smashfs.h"

#include "uthash.h"
#include "buffer.h"
#include "bitbuffer.h"

static int debug			= 0;
static char *source			= NULL;
static char *output			= NULL;

struct buffer inode_buffer;
struct buffer entry_buffer;
struct smashfs_super_block super_block;

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
	int option_index;
	static struct option long_options[] = {
		{"source"    , required_argument, 0, 's' },
		{"output"    , required_argument, 0, 'o' },
		{"debug"     , no_argument      , 0, 'd' },
		{"help"      , no_argument      , 0, 'h' },
		{ 0          , 0                , 0,  0 }
	};
	fd = -1;
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
	fd = open(source, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open failed for %s\n", source);
		rc = -1;
		goto bail;
	}
	rc = read(fd, &super_block, sizeof(struct smashfs_super_block));
	if (rc != sizeof(struct smashfs_super_block)) {
		fprintf(stderr, "could not read super block\n");
		rc = -1;
		goto bail;
	}
	if (debug) {
		fprintf(stdout, "  super block:\n");
		fprintf(stdout, "    magic         : 0x%08x, %u\n", super_block.magic, super_block.magic);
		fprintf(stdout, "    version       : 0x%08x, %u\n", super_block.version, super_block.version);
		fprintf(stdout, "    ctime         : 0x%08x, %u\n", super_block.ctime, super_block.ctime);
		fprintf(stdout, "    block_size    : 0x%08x, %u\n", super_block.block_size, super_block.block_size);
		fprintf(stdout, "    block_log2    : 0x%08x, %u\n", super_block.block_log2, super_block.block_log2);
		fprintf(stdout, "    inodes        : 0x%08x, %u\n", super_block.inodes, super_block.inodes);
		fprintf(stdout, "    root          : 0x%08x, %u\n", super_block.root, super_block.root);
		fprintf(stdout, "    inodes_offset : 0x%08x, %u\n", super_block.inodes_offset, super_block.inodes_offset);
		fprintf(stdout, "    inodes_size   : 0x%08x, %u\n", super_block.inodes_size, super_block.inodes_size);
		fprintf(stdout, "    entries_offset: 0x%08x, %u\n", super_block.entries_offset, super_block.entries_offset);
		fprintf(stdout, "    entries_size  : 0x%08x, %u\n", super_block.entries_size, super_block.entries_size);
		fprintf(stdout, "    bits:\n");
		fprintf(stdout, "      min:\n");
		fprintf(stdout, "        ctime : 0x%08x, %u\n", super_block.min.inode.ctime, super_block.min.inode.ctime);
		fprintf(stdout, "        mtime : 0x%08x, %u\n", super_block.min.inode.mtime, super_block.min.inode.mtime);
		fprintf(stdout, "      inode:\n");
		fprintf(stdout, "        number    : %u\n", super_block.bits.inode.number);
		fprintf(stdout, "        type      : %u\n", super_block.bits.inode.type);
		fprintf(stdout, "        owner_mode: %u\n", super_block.bits.inode.owner_mode);
		fprintf(stdout, "        group_mode: %u\n", super_block.bits.inode.group_mode);
		fprintf(stdout, "        other_mode: %u\n", super_block.bits.inode.other_mode);
		fprintf(stdout, "        uid       : %u\n", super_block.bits.inode.uid);
		fprintf(stdout, "        gid       : %u\n", super_block.bits.inode.gid);
		fprintf(stdout, "        ctime     : %u\n", super_block.bits.inode.ctime);
		fprintf(stdout, "        mtime     : %u\n", super_block.bits.inode.mtime);
		fprintf(stdout, "        size      : %u\n", super_block.bits.inode.size);
		fprintf(stdout, "        block     : %u\n", super_block.bits.inode.block);
		fprintf(stdout, "        index     : %u\n", super_block.bits.inode.index);
		fprintf(stdout, "        regular_file:\n");
		fprintf(stdout, "        directory:\n");
		fprintf(stdout, "          parent   : %u\n", super_block.bits.inode.directory.parent);
		fprintf(stdout, "          nentries : %u\n", super_block.bits.inode.directory.nentries);
		fprintf(stdout, "          entries:\n");
		fprintf(stdout, "            number : %u\n", super_block.bits.inode.directory.entries.number);
		fprintf(stdout, "        symbolic_link:\n");
	}
	rc = lseek(fd, super_block.inodes_offset, SEEK_SET);
	if (rc != (int) super_block.inodes_offset) {
		fprintf(stderr, "seek failed for inodes\n");
		rc = -1;
		goto bail;
	}
	rc = 0;
bail:
	close(fd);
	free(source);
	free(output);
	buffer_uninit(&entry_buffer);
	buffer_uninit(&inode_buffer);
	return rc;
}
