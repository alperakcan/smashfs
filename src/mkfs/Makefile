
SMASHFS_ENABLE_GZIP ?= y
SMASHFS_ENABLE_LZMA ?= y
SMASHFS_ENABLE_LZO  ?= y
SMASHFS_ENABLE_XZ   ?= y

target.host-y = \
	mkfs.smashfs \
	unfs.smashfs

mkfs.smashfs_files-y = \
	mkfs.c \
	buffer.c \
	bitbuffer.c \
	compressor.c \
	compressor-none.c

mkfs.smashfs_cflags-y += \
	-U_FILE_OFFSET_BITS

mkfs.smashfs_cflags-${SMASHFS_ENABLE_GZIP} += \
	-DSMASHFS_ENABLE_GZIP=1

mkfs.smashfs_files-${SMASHFS_ENABLE_GZIP} += \
	compressor-gzip.c

mkfs.smashfs_cflags-${SMASHFS_ENABLE_LZMA} += \
	-DSMASHFS_ENABLE_LZMA=1

mkfs.smashfs_files-${SMASHFS_ENABLE_LZMA} += \
	compressor-lzma.c

mkfs.smashfs_cflags-${SMASHFS_ENABLE_LZO} += \
	-DSMASHFS_ENABLE_LZO=1

mkfs.smashfs_files-${SMASHFS_ENABLE_LZO} += \
	compressor-lzo.c

mkfs.smashfs_cflags-${SMASHFS_ENABLE_XZ} += \
	-DSMASHFS_ENABLE_XZ=1

mkfs.smashfs_files-${SMASHFS_ENABLE_XZ} += \
	compressor-xz.c

mkfs.smashfs_includes-y = \
	../include

mkfs.smashfs_libraries-y = \
	/opt/local/lib

mkfs.smashfs_ldflags-y = \
	-lpthread \
	-lz \
	-llzma \
	-llzo2

unfs.smashfs_files-y = \
	unfs.c \
	buffer.c \
	bitbuffer.c \
	compressor.c \
	compressor-none.c

unfs.smashfs_cflags-y += \
	-U_FILE_OFFSET_BITS

unfs.smashfs_cflags-${SMASHFS_ENABLE_GZIP} += \
	-DSMASHFS_ENABLE_GZIP=1

unfs.smashfs_files-${SMASHFS_ENABLE_GZIP} += \
	compressor-gzip.c

unfs.smashfs_cflags-${SMASHFS_ENABLE_LZMA} += \
	-DSMASHFS_ENABLE_LZMA=1

unfs.smashfs_files-${SMASHFS_ENABLE_LZMA} += \
	compressor-lzma.c

unfs.smashfs_cflags-${SMASHFS_ENABLE_LZO} += \
	-DSMASHFS_ENABLE_LZO=1

unfs.smashfs_files-${SMASHFS_ENABLE_LZO} += \
	compressor-lzo.c

unfs.smashfs_cflags-${SMASHFS_ENABLE_XZ} += \
	-DSMASHFS_ENABLE_XZ=1

unfs.smashfs_files-${SMASHFS_ENABLE_XZ} += \
	compressor-xz.c

unfs.smashfs_includes-y = \
	../include

unfs.smashfs_libraries-y = \
	/opt/local/lib

unfs.smashfs_ldflags-y = \
	-lz \
	-llzma \
	-llzo2

include ../../Makefile.lib
