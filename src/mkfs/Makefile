
target.host-y = \
	mkfs.smashfs \
	unfs.smashfs

mkfs.smashfs_files-y = \
	mkfs.c \
	buffer.c \
	bitbuffer.c \
	compressor.c \
	compressor-none.c \
	compressor-gzip.c \
	compressor-lzma.c

mkfs.smashfs_libraries-y = \
	/opt/local/lib

mkfs.smashfs_ldflags-y = \
	-lz \
	-llzma

mkfs.smashfs_cflags-y = \
	-U_FILE_OFFSET_BITS

unfs.smashfs_files-y = \
	unfs.c \
	buffer.c \
	bitbuffer.c \
	compressor.c \
	compressor-none.c \
	compressor-gzip.c \
	compressor-lzma.c

unfs.smashfs_cflags-y = \
	-U_FILE_OFFSET_BITS

unfs.smashfs_libraries-y = \
	/opt/local/lib

unfs.smashfs_ldflags-y = \
	-lz \
	-llzma

include ../../Makefile.lib
