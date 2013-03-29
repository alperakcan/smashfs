
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

static char *output = NULL;
static unsigned int block_size = 4096;
static LIST_HEAD(sources, source) sources;

unsigned int slog (unsigned int block)
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
	super.magic = SMASHFS_MAGIC;
	super.version = SMASHFS_VERSION_0;
	super.ctime = 0;
	super.block_size = block_size;
	super.block_log2 = slog(block_size);
	super.inodes = 0;
	super.root = 0;
	rc = write(fd, &super, sizeof(struct smashfs_super_block));
	if (rc != sizeof(struct smashfs_super_block)) {
		fprintf(stderr, "write failed for super\n");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

static struct smashfs_inode * get_inode (const char *path)
{
	int rc;
	struct stat stbuf;
	struct smashfs_inode *inode;
	rc = stat(path, &stbuf);
	if (rc != 0) {
		fprintf(stderr, "stat for %s failed\n", path);
		return NULL;
	}
	if (stbuf.st_uid > SMASHFS_INODE_UID_MAX) {
		fprintf(stderr, "uid: %d is greater than allowed limit: %d\n", stbuf.st_uid, SMASHFS_INODE_UID_MAX);
		return NULL;
	}
	if (stbuf.st_gid > SMASHFS_INODE_GID_MAX) {
		fprintf(stderr, "gid: %d is greater than allowed limit: %d\n", stbuf.st_gid, SMASHFS_INODE_GID_MAX);
		return NULL;
	}
	if (stbuf.st_size > SMASHFS_INODE_SIZE_MAX) {
		fprintf(stderr, "size: %lld is greater than allowed limit: %d\n", (unsigned long long) stbuf.st_size, SMASHFS_INODE_SIZE_MAX);
		return NULL;
	}
	inode = malloc(sizeof(struct smashfs_inode));
	if (inode == NULL) {
		fprintf(stderr, "malloc failed for smashfs inode\n");
		return NULL;
	}
	if (S_ISREG(stbuf.st_mode)) {
		inode->type = smashfs_inode_type_regular_file;
	} else if (S_ISDIR(stbuf.st_mode)) {
		inode->type = smashfs_inode_type_directory;
	} else if (S_ISCHR(stbuf.st_mode)) {
		inode->type = smashfs_inode_type_character_device;
	} else if (S_ISBLK(stbuf.st_mode)) {
		inode->type = smashfs_inode_type_block_device;
	} else if (S_ISFIFO(stbuf.st_mode)) {
		inode->type = smashfs_inode_type_fifo;
	} else if (S_ISLNK(stbuf.st_mode)) {
		inode->type = smashfs_inode_type_symbolic_link;
	} else if (S_ISSOCK(stbuf.st_mode)) {
		inode->type = smashfs_inode_type_socket;
	} else {
		fprintf(stderr, "unknown mode: 0x%08x, for path: %s\n", stbuf.st_mode, path);
		free(inode);
		return NULL;
	}
	inode->owner_mode = 0;
	if (stbuf.st_mode & S_IRUSR) {
		inode->owner_mode |= smashfs_inode_mode_read;
	}
	if (stbuf.st_mode & S_IWUSR) {
		inode->owner_mode |= smashfs_inode_mode_write;
	}
	if (stbuf.st_mode & S_IXUSR) {
		inode->owner_mode |= smashfs_inode_mode_execute;
	}
	inode->group_mode = 0;
	if (stbuf.st_mode & S_IRGRP) {
		inode->group_mode |= smashfs_inode_mode_read;
	}
	if (stbuf.st_mode & S_IWGRP) {
		inode->group_mode |= smashfs_inode_mode_write;
	}
	if (stbuf.st_mode & S_IXGRP) {
		inode->group_mode |= smashfs_inode_mode_execute;
	}
	inode->other_mode = 0;
	if (stbuf.st_mode & S_IROTH) {
		inode->other_mode |= smashfs_inode_mode_read;
	}
	if (stbuf.st_mode & S_IWOTH) {
		inode->other_mode |= smashfs_inode_mode_write;
	}
	if (stbuf.st_mode & S_IXOTH) {
		inode->other_mode |= smashfs_inode_mode_execute;
	}
	inode->uid = (stbuf.st_uid & SMASHFS_INODE_UID_MASK);
	inode->gid = (stbuf.st_gid & SMASHFS_INODE_GID_MASK);
	return inode;
}

static void scan_source (struct source *source)
{
	struct smashfs_inode *inode;
	fprintf(stdout, "scanning source: %s\n", source->path);
	inode = get_inode(source->path);
}

static void scan_sources (void)
{
	unsigned int nsources;
	struct source *source;
	nsources = 0;
	LIST_FOREACH(source, &sources, head) {
		nsources += 1;
	}
	fprintf(stdout, "scanning sources: %d\n", nsources);
	LIST_FOREACH(source, &sources, head) {
		scan_source(source);
	}
}

static void print_help (const char *pname)
{
	fprintf(stdout, "%s usage;\n", pname);
	fprintf(stdout, "  -s, --source     : source directory/file\n");
	fprintf(stdout, "  -o, --output     : output file\n");
	fprintf(stdout, "  -b, --block_size : block size (default: %d)\n", block_size);
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
		{"help"      , no_argument      , 0, 'h' },
		{ 0          , 0                , 0,  0 }
	};
	option_index = 0;
	while ((c = getopt_long(argc, argv, "hs:o:", long_options, &option_index)) != -1) {
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
