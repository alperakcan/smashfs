
target.host-y = \
	mkfs.smashfs \
	unfs.smashfs

mkfs.smashfs_files-y = \
	mkfs.c \
	buffer.c \
	bitbuffer.c \
	compressor.c \
	compressor-none.c \

mkfs.smashfs_cflags-y = \
	-U_FILE_OFFSET_BITS

unfs.smashfs_files-y = \
	unfs.c \
	buffer.c \
	bitbuffer.c \
	compressor.c \
	compressor-none.c \

unfs.smashfs_cflags-y = \
	-U_FILE_OFFSET_BITS

include ../../Makefile.lib
