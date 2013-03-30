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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fts.h>

#include <sys/queue.h>

#include "../include/smashfs.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

struct item {
	char *path;
};

struct source {
	LIST_ENTRY(source) head;
	char *path;
};

static LIST_HEAD(sources, source) sources;

static unsigned long long ninodes		= 0;
static unsigned long long sinodes		= 0;
static struct smashfs_inode *inodes		= NULL;

static int debug				= 0;
static char *output				= NULL;
static unsigned int block_size			= 4096;

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

static int write_output (void)
{
	int rc;
	int fd;
	struct smashfs_super_block super;
	fprintf(stdout, "writing file: %s\n", output);
	fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		fprintf(stderr, "open failed for: %s\n", output);
		return -1;
	}
	fprintf(stdout, "  writing super block\n");
	super.magic = SMASHFS_MAGIC;
	super.version = SMASHFS_VERSION_0;
	super.ctime = 0;
	super.block_size = block_size;
	super.block_log2 = slog(block_size);
	super.inodes = 0;
	super.root = 0;
	rc = write(fd, &super, sizeof(struct smashfs_super_block));
	if (rc != sizeof(struct smashfs_super_block)) {
		fprintf(stderr, "write failed for super block\n");
		close(fd);
		return -1;
	}
	fprintf(stdout, "  writing inode table (%d bytes)\n", sizeof(struct smashfs_inode) * ninodes);
	rc = write(fd, inodes, sizeof(struct smashfs_inode) * ninodes);
	if (rc != (int) (sizeof(struct smashfs_inode) * ninodes)) {
		fprintf(stderr, "write failed for inode table\n");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

static struct smashfs_inode * new_inode (const struct stat const *stbuf)
{
	struct smashfs_inode *inode;
	struct smashfs_inode *tinodes;
	if (ninodes + 1 >= sinodes) {
		if (sinodes == 0) {
			sinodes = 1;
		} else {
			sinodes *= 2;
		}
		tinodes = malloc(sizeof(struct smashfs_inode) * sinodes);
		if (tinodes == NULL) {
			fprintf(stderr, "malloc failed\n");
			return NULL;
		}
		if (ninodes > 0) {
			memcpy(tinodes, inodes, ninodes * sizeof(struct smashfs_inode));
		}
		if (inodes != NULL) {
			free(inodes);
			inodes = NULL;
		}
		inodes = tinodes;
	}
	if (stbuf->st_uid > SMASHFS_INODE_UID_MAX) {
		fprintf(stderr, "uid: %d is greater than allowed limit: %d\n", stbuf->st_uid, SMASHFS_INODE_UID_MAX);
		return NULL;
	}
	if (stbuf->st_gid > SMASHFS_INODE_GID_MAX) {
		fprintf(stderr, "gid: %d is greater than allowed limit: %d\n", stbuf->st_gid, SMASHFS_INODE_GID_MAX);
		return NULL;
	}
	if (stbuf->st_size > SMASHFS_INODE_SIZE_MAX) {
		fprintf(stderr, "size: %lld is greater than allowed limit: %d\n", (unsigned long long) stbuf->st_size, SMASHFS_INODE_SIZE_MAX);
		return NULL;
	}
	inode = &inodes[ninodes];
	inode->number = ninodes;
	ninodes += 1;
	if (S_ISREG(stbuf->st_mode)) {
		inode->type = smashfs_inode_type_regular_file;
	} else if (S_ISDIR(stbuf->st_mode)) {
		inode->type = smashfs_inode_type_directory;
	} else if (S_ISCHR(stbuf->st_mode)) {
		inode->type = smashfs_inode_type_character_device;
	} else if (S_ISBLK(stbuf->st_mode)) {
		inode->type = smashfs_inode_type_block_device;
	} else if (S_ISFIFO(stbuf->st_mode)) {
		inode->type = smashfs_inode_type_fifo;
	} else if (S_ISLNK(stbuf->st_mode)) {
		inode->type = smashfs_inode_type_symbolic_link;
	} else if (S_ISSOCK(stbuf->st_mode)) {
		inode->type = smashfs_inode_type_socket;
	} else {
		fprintf(stderr, "unknown mode: 0x%08x\n", stbuf->st_mode);
		free(inode);
		return NULL;
	}
	inode->owner_mode = 0;
	if (stbuf->st_mode & S_IRUSR) {
		inode->owner_mode |= smashfs_inode_mode_read;
	}
	if (stbuf->st_mode & S_IWUSR) {
		inode->owner_mode |= smashfs_inode_mode_write;
	}
	if (stbuf->st_mode & S_IXUSR) {
		inode->owner_mode |= smashfs_inode_mode_execute;
	}
	inode->group_mode = 0;
	if (stbuf->st_mode & S_IRGRP) {
		inode->group_mode |= smashfs_inode_mode_read;
	}
	if (stbuf->st_mode & S_IWGRP) {
		inode->group_mode |= smashfs_inode_mode_write;
	}
	if (stbuf->st_mode & S_IXGRP) {
		inode->group_mode |= smashfs_inode_mode_execute;
	}
	inode->other_mode = 0;
	if (stbuf->st_mode & S_IROTH) {
		inode->other_mode |= smashfs_inode_mode_read;
	}
	if (stbuf->st_mode & S_IWOTH) {
		inode->other_mode |= smashfs_inode_mode_write;
	}
	if (stbuf->st_mode & S_IXOTH) {
		inode->other_mode |= smashfs_inode_mode_execute;
	}
	inode->uid = (stbuf->st_uid & SMASHFS_INODE_UID_MASK);
	inode->gid = (stbuf->st_gid & SMASHFS_INODE_GID_MASK);
	return inode;
}

static void scan_sources (void)
{
	FTS *tree;
	FTSENT *node;
	char **paths;
	struct source *source;
	unsigned int nsources;
	struct smashfs_inode *inode;
	nsources = 0;
	LIST_FOREACH(source, &sources, head) {
		nsources += 1;
	}
	fprintf(stdout, "scanning sources: %d\n", nsources);
	paths = malloc(sizeof(char *) * (nsources + 1));
	if (paths == NULL) {
		fprintf(stderr, "malloc failed\n");
		return;
	}
	nsources = 0;
	LIST_FOREACH(source, &sources, head) {
		fprintf(stdout, "  setting path: %d, as: %s\n", nsources, source->path);
		paths[nsources] = source->path;
		nsources += 1;
	}
	paths[nsources] = NULL;
	tree = fts_open(paths, FTS_COMFOLLOW /* | FTS_NOCHDIR */ | FTS_PHYSICAL /* | FTS_SEEDOT */, NULL);
	if (tree == NULL) {
		fprintf(stderr, "fts_open failed\n");
		free(paths);
		return;
	}
	while ((node = fts_read(tree))) {
		if (node->fts_info == FTS_NSOK) {
			fprintf(stderr, "A file for which no stat(2) information was\n"
					"requested. The contents of the fts_statp field are\n"
					"undefined.\n");
			fprintf(stderr, "  path: %s\n", node->fts_path);
			fprintf(stderr, "  info: 0x%08x\n", node->fts_info);
			continue;
		}
		if (node->fts_info == FTS_ERR) {
			fprintf(stderr, "This is an error return, and the fts_errno field\n"
					"will be set to indicate what caused the error.\n");
			fprintf(stderr, "  path : %s\n", node->fts_path);
			fprintf(stderr, "  errno: %d\n", node->fts_errno);
			continue;
		}
		if (node->fts_info == FTS_NS) {
			fprintf(stderr, "A file for which no stat(2) information was\n"
					"available. The contents of the fts_statp field are\n"
					"undefined. This is an error return, and the\n"
					"fts_errno field will be set to indicate what caused\n"
					"the error.\n");
			fprintf(stderr, "  path : %s\n", node->fts_path);
			fprintf(stderr, "  errno: %d\n", node->fts_errno);
			continue;
		}
		if (node->fts_info == FTS_DNR) {
			fprintf(stderr, "A directory which cannot be read. This is an error\n"
					"return, and the fts_errno field will be set to\n"
					"indicate what caused the error.\n");
			fprintf(stderr, "  path : %s\n", node->fts_path);
			fprintf(stderr, "  errno: %d\n", node->fts_errno);
			continue;
		}
		if (node->fts_info == FTS_DP) {
			continue;
		}
		inode = new_inode(node->fts_statp);
		if (inode == NULL) {
			fprintf(stderr, "new inode failed\n");
			continue;
		}
		node->fts_pointer = inode;
		if (debug) {
			int l;
			for (l = 0; l < node->fts_level; l++) {
				fprintf(stdout, " ");
			}
			fprintf(stdout, "%s %s (inode: %d, parent: %d)\n",
					(inode->type == smashfs_inode_type_regular_file) ? "(f)" :
					  (inode->type == smashfs_inode_type_directory) ? "(d)" :
					  (inode->type == smashfs_inode_type_symbolic_link) ? "(s)" :
					  "?",
					node->fts_name,
					inode->number,
					(node->fts_parent->fts_pointer != NULL) ? (int) ((struct smashfs_inode *) node->fts_parent->fts_pointer)->number : -1);
		}
	}
	fts_close(tree);
	free(paths);
}

static void print_help (const char *pname)
{
	fprintf(stdout, "%s usage;\n", pname);
	fprintf(stdout, "  -s, --source     : source directory/file\n");
	fprintf(stdout, "  -o, --output     : output file\n");
	fprintf(stdout, "  -b, --block_size : block size (default: %d)\n", block_size);
	fprintf(stdout, "  -d, --debug      : enable debug output (default: %d)\n", debug);
}

int main (int argc, char *argv[])
{
	int c;
	int option_index;
	unsigned int nsources;
	struct source *source;
	static struct option long_options[] = {
		{"source"    , required_argument, 0, 's' },
		{"output"    , required_argument, 0, 'o' },
		{"block_size", required_argument, 0, 'b' },
		{"debug"     , no_argument      , 0, 'd' },
		{"help"      , no_argument      , 0, 'h' },
		{ 0          , 0                , 0,  0 }
	};
	option_index = 0;
	while ((c = getopt_long(argc, argv, "hds:o:", long_options, &option_index)) != -1) {
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
				LIST_INSERT_HEAD(&sources, source, head);
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
			case 'h':
				print_help(argv[0]);
				exit(0);
		}
	}
	nsources = 0;
	LIST_FOREACH(source, &sources, head) {
		nsources += 1;
	}
	if (nsources == 0) {
		fprintf(stderr, "no source directory/file specified, quiting.\n");
		goto bail;
	}
	if (output == NULL) {
		fprintf(stderr, "no output file specified, quiting.\n");
		goto bail;
	}
	scan_sources();
	write_output();
	while (sources.lh_first != NULL) {
		source = sources.lh_first;;
		LIST_REMOVE(source, head);
		free(source->path);
		free(source);
	}
	free(output);
	return 0;
bail:
	while (sources.lh_first != NULL) {
		source = sources.lh_first;;
		LIST_REMOVE(source, head);
		free(source->path);
		free(source);
	}
	free(output);
	return -1;
}
