
target.host-y = \
	mkfs.smashfs

mkfs.smashfs_files-y = \
	mkfs.c \
	buffer.c \
	bitbuffer.c

mkfs.smashfs_cflags-y = \
	-U_FILE_OFFSET_BITS

mkfs.smashfs_ldflags-y = \
	-lz

include ../../Makefile.lib
