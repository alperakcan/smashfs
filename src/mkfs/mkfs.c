
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../include/list.h"
#include "../include/smashfs.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

struct item {
	char *path;
};

struct source {
	struct smashfs_list head;
	char *path;
};

static char *output = NULL;
static unsigned int block_size = 4096;
static struct smashfs_list sources = SMASHFS_LIST_HEAD_INIT(sources);

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
	struct smashfs_super super;
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
	rc = write(fd, &super, sizeof(struct smashfs_super));
	if (rc != sizeof(struct smashfs_super)) {
		fprintf(stderr, "write failed for super\n");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

static void scan_source (struct source *source)
{
	fprintf(stdout, "scanning source: %s\n", source->path);
}

static void scan_sources (void)
{
	struct source *source;
	fprintf(stdout, "scanning sources: %d\n", smashfs_list_count(&sources));
	smashfs_list_for_each_entry(source, &sources, head) {
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
	struct source *source;
	struct source *nsource;
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
				smashfs_list_add_tail(&source->head, &sources);
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
	if (smashfs_list_count(&sources) == 0) {
		fprintf(stderr, "no source directory/file specified, quiting.\n");
		goto bail;
	}
	if (output == NULL) {
		fprintf(stderr, "no output file specified, quiting.\n");
		goto bail;
	}
	scan_sources();
	write_output();
	smashfs_list_for_each_entry_safe(source, nsource, &sources, head) {
		smashfs_list_del(&source->head);
		free(source->path);
		free(source);
	}
	free(output);
	return 0;
bail:
	smashfs_list_for_each_entry_safe(source, nsource, &sources, head) {
		smashfs_list_del(&source->head);
		free(source->path);
		free(source);
	}
	free(output);
	return -1;
}
