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

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

struct item {
	char *path;
};

struct source {
	LIST_ENTRY(source) sources;
	char *path;
};

struct node {
	unsigned long long number;
	unsigned long long type;
	unsigned long long owner_mode;
	unsigned long long group_mode;
	unsigned long long other_mode;
	unsigned long long uid;
	unsigned long long gid;
	unsigned long long size;
	char name[PATH_MAX];
	UT_hash_handle hh;
};

struct entry_regular_file {
};

struct entry_directory {
	struct node *parent;
};

struct entry {
	struct node *node;
	union {
		struct entry_regular_file regular_file;
		struct entry_directory directory;
	};
	UT_hash_handle hh;
};

static LIST_HEAD(sources, source) sources;

static unsigned long long nodes_id		= 0;
static struct node *nodes_table		= NULL;
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
	close(fd);
	return 0;
}

static int node_delete (struct node *node)
{
	HASH_DEL(nodes_table, node);
	free(node);
	return 0;
}

static struct node * node_new (const struct stat *stbuf, const char *name)
{
	struct node *node;
	node = malloc(sizeof(struct node));
	if (node == NULL) {
		fprintf(stderr, "malloc failed\n");
		return NULL;
	}
	snprintf(node->name, PATH_MAX, "%s", name);
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
	node->uid = (stbuf->st_uid & SMASHFS_INODE_UID_MASK);
	node->gid = (stbuf->st_gid & SMASHFS_INODE_GID_MASK);
	node->size = 0;
	node->number = nodes_id++;
	node->number = node->number;
	HASH_ADD(hh, nodes_table, number, sizeof(node->number), node);
	return node;
}

static int entry_add_child (struct entry *parent, struct entry *child)
{
	struct node *pnode;
	struct node *cnode;
	if (parent == NULL) {
		fprintf(stderr, "parent is null\n");
		return -1;
	}
	cnode = child->node;
	pnode = parent->node;
	if (pnode->type != smashfs_inode_type_directory) {
		fprintf(stderr, "parent is not a directory\n");
		return -1;
	}
	child->directory.parent = parent->node;
	return 0;
}

static int entry_delete (struct entry *entry)
{
	HASH_DEL(entries_table, entry);
	free(entry);
	return 0;
}

static struct entry * entry_new (struct node *node)
{
	struct entry *entry;
	entry = malloc(sizeof(struct entry));
	if (entry == NULL) {
		fprintf(stderr, "malloc failed\n");
		return NULL;
	}
	entry->node = node;
	if (entry->node->type == smashfs_inode_type_regular_file) {
	} else if (entry->node->type == smashfs_inode_type_directory) {
	} else {
		fprintf(stderr, "unknown type: %lld\n", entry->node->type);
		free(entry);
		return NULL;
	}
	HASH_ADD(hh, entries_table, node, sizeof(entry->node), entry);
	return entry;
}

static void sources_scan (void)
{
	struct node *node;
	struct entry *entry;
	struct entry *parent;
	unsigned int nsources;
	struct source *source;

	char **paths;
	FTS *fts_tree;
	FTSENT *fts_node;

	nsources = 0;
	LIST_FOREACH(source, &sources, sources) {
		nsources += 1;
	}
	fprintf(stdout, "scanning sources: %d\n", nsources);
	paths = malloc(sizeof(char *) * (nsources + 1));
	if (paths == NULL) {
		fprintf(stderr, "malloc failed\n");
		return;
	}
	nsources = 0;
	LIST_FOREACH(source, &sources, sources) {
		fprintf(stdout, "  setting path: %d, as: %s\n", nsources, source->path);
		paths[nsources] = source->path;
		nsources += 1;
	}
	paths[nsources] = NULL;
	fts_tree = fts_open(paths, FTS_COMFOLLOW /* | FTS_NOCHDIR */ | FTS_PHYSICAL /* | FTS_SEEDOT */, NULL);
	if (fts_tree == NULL) {
		fprintf(stderr, "fts_open failed\n");
		free(paths);
		return;
	}
	while ((fts_node = fts_read(fts_tree))) {
		if (fts_node->fts_info == FTS_NSOK) {
			fprintf(stderr, "A file for which no stat(2) information was\n"
					"requested. The contents of the fts_statp field are\n"
					"undefined.\n");
			fprintf(stderr, "  path: %s\n", fts_node->fts_path);
			fprintf(stderr, "  info: 0x%08x\n", fts_node->fts_info);
			continue;
		}
		if (fts_node->fts_info == FTS_ERR) {
			fprintf(stderr, "This is an error return, and the fts_errno field\n"
					"will be set to indicate what caused the error.\n");
			fprintf(stderr, "  path : %s\n", fts_node->fts_path);
			fprintf(stderr, "  errno: %d\n", fts_node->fts_errno);
			continue;
		}
		if (fts_node->fts_info == FTS_NS) {
			fprintf(stderr, "A file for which no stat(2) information was\n"
					"available. The contents of the fts_statp field are\n"
					"undefined. This is an error return, and the\n"
					"fts_errno field will be set to indicate what caused\n"
					"the error.\n");
			fprintf(stderr, "  path : %s\n", fts_node->fts_path);
			fprintf(stderr, "  errno: %d\n", fts_node->fts_errno);
			continue;
		}
		if (fts_node->fts_info == FTS_DNR) {
			fprintf(stderr, "A directory which cannot be read. This is an error\n"
					"return, and the fts_errno field will be set to\n"
					"indicate what caused the error.\n");
			fprintf(stderr, "  path : %s\n", fts_node->fts_path);
			fprintf(stderr, "  errno: %d\n", fts_node->fts_errno);
			continue;
		}
		if (fts_node->fts_info == FTS_DP) {
			continue;
		}
		node = node_new(fts_node->fts_statp, fts_node->fts_name);
		if (node == NULL) {
			fprintf(stderr, "node new failed\n");
			continue;
		}
		entry = entry_new(node);
		if (entry == NULL) {
			fprintf(stderr, "entry new failed\n");
			node_delete(node);
			continue;
		}
		fts_node->fts_pointer = entry;
		parent = fts_node->fts_parent->fts_pointer;
		if (debug) {
			int l;
			for (l = 0; l < fts_node->fts_level; l++) {
				fprintf(stdout, " ");
			}
			fprintf(stdout, "%s %s (node: %lld, parent: %lld)\n",
					(node->type == smashfs_inode_type_regular_file) ? "(f)" :
					  (node->type == smashfs_inode_type_directory) ? "(d)" :
					  (node->type == smashfs_inode_type_symbolic_link) ? "(s)" :
					  "?",
					fts_node->fts_name,
					node->number,
					(parent != NULL) ? parent->node->number : (unsigned long long) -1);
		}
		if (parent != NULL) {
			entry_add_child(parent, entry);
		}
	}
	fts_close(fts_tree);
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
	int rc;
	int option_index;
	struct node *node;
	struct node *nnode;
	struct entry *entry;
	struct entry *nentry;
	struct source *source;
	unsigned int nsources;
	static struct option long_options[] = {
		{"source"    , required_argument, 0, 's' },
		{"output"    , required_argument, 0, 'o' },
		{"block_size", required_argument, 0, 'b' },
		{"debug"     , no_argument      , 0, 'd' },
		{"help"      , no_argument      , 0, 'h' },
		{ 0          , 0                , 0,  0 }
	};
	rc = 0;
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
	fprintf(stdout, "statistics:\n");
	fprintf(stdout, "  nodes : %d\n", HASH_CNT(hh, nodes_table));
	fprintf(stdout, "  entries: %d\n", HASH_CNT(hh, entries_table));
	output_write();
bail:
	while (sources.lh_first != NULL) {
		source = sources.lh_first;;
		LIST_REMOVE(source, sources);
		free(source->path);
		free(source);
	}
	HASH_ITER(hh, entries_table, entry, nentry) {
		HASH_DEL(entries_table, entry);
		entry_delete(entry);
	}
	HASH_ITER(hh, nodes_table, node, nnode) {
		HASH_DEL(nodes_table, node);
		node_delete(node);
	}
	free(output);
	return rc;
}
