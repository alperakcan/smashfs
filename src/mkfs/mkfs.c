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

#include "uthash.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

struct item {
	char *path;
};

struct source {
	LIST_ENTRY(source) head;
	char *path;
};

struct inode {
	unsigned long long number;
	char name[SMASHFS_NAME_LEN];
	struct smashfs_inode inode;
	UT_hash_handle hh;
};

struct entry {
	struct inode *inode;
	union {
		struct smashfs_inode_regular_file *regular_file;
		struct smashfs_inode_directory *directory;
	};
	UT_hash_handle hh;
};

static LIST_HEAD(sources, source) sources;

static unsigned long long inodes_id		= 0;
static struct inode *inodes_table		= NULL;
static struct entry *entries_table		= NULL;

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

static int output_write (void)
{
	int rc;
	int fd;
	struct inode *inode;
	struct inode *ninode;
	struct smashfs_super_block super;
	fprintf(stdout, "writing file: %s\n", output);
	fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		fprintf(stderr, "open failed for: %s\n", output);
		return -1;
	}
	fprintf(stdout, "  writing super block (%zd bytes)\n", sizeof(struct smashfs_super_block));
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
	fprintf(stdout, "  writing inodes table (%zd bytes (%d inodes))\n", sizeof(struct smashfs_inode) * HASH_CNT(hh, inodes_table), HASH_CNT(hh, inodes_table));
	HASH_ITER(hh, inodes_table, inode, ninode) {
		rc = write(fd, &inode->inode, sizeof(struct smashfs_inode));
		if (rc != sizeof(struct smashfs_inode)) {
			fprintf(stderr, "write failed for inode\n");
			close(fd);
			return -1;
		}
	}
	close(fd);
	return 0;
}

static int inode_delete (struct inode *inode)
{
	HASH_DEL(inodes_table, inode);
	free(inode);
	return 0;
}

static struct inode * inode_new (const struct stat const *stbuf, const char const *name)
{
	struct inode *inode;
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
	if (name == NULL) {
		fprintf(stderr, "name is null\n");
		return NULL;
	}
	inode = malloc(sizeof(struct inode));
	if (inode == NULL) {
		fprintf(stderr, "malloc failed\n");
		return NULL;
	}
	snprintf(inode->name, SMASHFS_NAME_LEN, "%s", name);
	if (S_ISREG(stbuf->st_mode)) {
		inode->inode.type = smashfs_inode_type_regular_file;
	} else if (S_ISDIR(stbuf->st_mode)) {
		inode->inode.type = smashfs_inode_type_directory;
	} else if (S_ISCHR(stbuf->st_mode)) {
		inode->inode.type = smashfs_inode_type_character_device;
	} else if (S_ISBLK(stbuf->st_mode)) {
		inode->inode.type = smashfs_inode_type_block_device;
	} else if (S_ISFIFO(stbuf->st_mode)) {
		inode->inode.type = smashfs_inode_type_fifo;
	} else if (S_ISLNK(stbuf->st_mode)) {
		inode->inode.type = smashfs_inode_type_symbolic_link;
	} else if (S_ISSOCK(stbuf->st_mode)) {
		inode->inode.type = smashfs_inode_type_socket;
	} else {
		fprintf(stderr, "unknown mode: 0x%08x\n", stbuf->st_mode);
		free(inode);
		return NULL;
	}
	inode->inode.owner_mode = 0;
	if (stbuf->st_mode & S_IRUSR) {
		inode->inode.owner_mode |= smashfs_inode_mode_read;
	}
	if (stbuf->st_mode & S_IWUSR) {
		inode->inode.owner_mode |= smashfs_inode_mode_write;
	}
	if (stbuf->st_mode & S_IXUSR) {
		inode->inode.owner_mode |= smashfs_inode_mode_execute;
	}
	inode->inode.group_mode = 0;
	if (stbuf->st_mode & S_IRGRP) {
		inode->inode.group_mode |= smashfs_inode_mode_read;
	}
	if (stbuf->st_mode & S_IWGRP) {
		inode->inode.group_mode |= smashfs_inode_mode_write;
	}
	if (stbuf->st_mode & S_IXGRP) {
		inode->inode.group_mode |= smashfs_inode_mode_execute;
	}
	inode->inode.other_mode = 0;
	if (stbuf->st_mode & S_IROTH) {
		inode->inode.other_mode |= smashfs_inode_mode_read;
	}
	if (stbuf->st_mode & S_IWOTH) {
		inode->inode.other_mode |= smashfs_inode_mode_write;
	}
	if (stbuf->st_mode & S_IXOTH) {
		inode->inode.other_mode |= smashfs_inode_mode_execute;
	}
	inode->inode.uid = (stbuf->st_uid & SMASHFS_INODE_UID_MASK);
	inode->inode.gid = (stbuf->st_gid & SMASHFS_INODE_GID_MASK);
	inode->inode.size = 0;
	inode->inode.number = inodes_id++;
	inode->number = inode->inode.number;
	HASH_ADD(hh, inodes_table, number, sizeof(inode->number), inode);
	return inode;
}

static struct smashfs_inode_regular_file * regular_file_new (void)
{
	struct smashfs_inode_regular_file *regular_file;
	regular_file = malloc(sizeof(struct smashfs_inode_regular_file));
	if (regular_file == NULL) {
		fprintf(stderr, "malloc failed\n");
		return NULL;
	}
	return regular_file;
}

static struct smashfs_inode_directory * directory_new (void)
{
	struct smashfs_inode_directory *directory;
	directory = malloc(sizeof(struct smashfs_inode_directory));
	if (directory == NULL) {
		fprintf(stderr, "malloc failed\n");
		return NULL;
	}
	directory->parent = -1;
	directory->nentries = 0;
	return directory;
}

static int entry_add_child (struct entry *parent, struct entry *child)
{
	struct inode *pinode;
	struct inode *cinode;
	struct smashfs_inode *psinode;
	struct smashfs_inode *csinode;
	struct smashfs_inode_directory *directory;
	struct smashfs_inode_directory_entry *directory_entry;
	if (parent == NULL) {
		fprintf(stderr, "parent is null\n");
		return -1;
	}
	cinode = child->inode;
	csinode = &cinode->inode;
	pinode = parent->inode;
	psinode = &pinode->inode;
	if (psinode->type != smashfs_inode_type_directory) {
		fprintf(stderr, "parent is not a directory\n");
		return -1;
	}
	directory = malloc(sizeof(struct smashfs_inode_directory) + ((parent->directory->nentries + 1) * sizeof(struct smashfs_inode_directory_entry)));
	if (directory == NULL) {
		fprintf(stderr, "malloc failed\n");
		return -1;
	}
	memcpy(directory, parent->directory, sizeof(struct smashfs_inode_directory) + parent->directory->nentries * sizeof(struct smashfs_inode_directory_entry));
	directory->entries[directory->nentries].number = child->inode->inode.number;
	child->directory->parent = psinode->number;
	free(directory);
	return 0;
}

static int entry_delete (struct entry *entry)
{
	HASH_DEL(entries_table, entry);
	free(entry);
	return 0;
}

static struct entry * entry_new (struct inode *inode)
{
	struct entry *entry;
	entry = malloc(sizeof(struct entry));
	if (entry == NULL) {
		fprintf(stderr, "malloc failed\n");
		return NULL;
	}
	entry->inode = inode;
	if (entry->inode->inode.type == smashfs_inode_type_regular_file) {
		entry->regular_file = regular_file_new();
		if (entry->regular_file == NULL) {
			fprintf(stderr, "new regular file failed\n");
			free(entry);
			return NULL;
		}
	} else if (entry->inode->inode.type == smashfs_inode_type_directory) {
		entry->directory = directory_new();
		if (entry->directory == NULL) {
			fprintf(stderr, "new directory failed\n");
			free(entry);
			return NULL;
		}
	} else {
		fprintf(stderr, "unknown type: %d\n", entry->inode->inode.type);
		free(entry);
		return NULL;
	}
	HASH_ADD(hh, entries_table, inode, sizeof(entry->inode), entry);
	return entry;
}

static void sources_scan (void)
{
	FTS *tree;
	FTSENT *node;
	char **paths;
	struct entry *entry;
	struct entry *parent;
	struct inode *inode;
	unsigned int nsources;
	struct source *source;
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
		inode = inode_new(node->fts_statp, node->fts_name);
		if (inode == NULL) {
			fprintf(stderr, "new inode failed\n");
			continue;
		}
		entry = entry_new(inode);
		if (entry == NULL) {
			inode_delete(inode);
			fprintf(stderr, "new entry failed\n");
			continue;
		}
		node->fts_pointer = entry;
		parent = node->fts_parent->fts_pointer;
		if (debug) {
			int l;
			for (l = 0; l < node->fts_level; l++) {
				fprintf(stdout, " ");
			}
			fprintf(stdout, "%s %s (inode: %d, parent: %d)\n",
					(inode->inode.type == smashfs_inode_type_regular_file) ? "(f)" :
					  (inode->inode.type == smashfs_inode_type_directory) ? "(d)" :
					  (inode->inode.type == smashfs_inode_type_symbolic_link) ? "(s)" :
					  "?",
					node->fts_name,
					inode->inode.number,
					(parent != NULL) ? (int) parent->inode->inode.number : -1);
		}
		if (parent != NULL) {
			entry_add_child(parent, entry);
		}
	}
	fts_close(tree);
	free(paths);
}

static void help_print (const char *pname)
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
				help_print(argv[0]);
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
	sources_scan();
	output_write();
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
