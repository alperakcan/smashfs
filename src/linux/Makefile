
MOD_NAME    ?= smashfs

CHIP_ARCH   ?= x86
KERNEL_PATH ?= /usr/src/linux-headers-$(shell uname -r)

SMASHFS_ENABLE_GZIP ?= y
SMASHFS_ENABLE_LZMA ?= y
SMASHFS_ENABLE_LZO  ?= y
SMASHFS_ENABLE_XZ   ?= y

obj-m := ${MOD_NAME}.o

${MOD_NAME}-objs  = super.o
${MOD_NAME}-objs += bitbuffer.o
${MOD_NAME}-objs += compressor.o
${MOD_NAME}-objs += compressor-none.o
ifeq (${SMASHFS_ENABLE_GZIP}, y)
${MOD_NAME}-objs += compressor-gzip.o
endif
ifeq (${SMASHFS_ENABLE_LZMA}, y)
${MOD_NAME}-objs += compressor-lzma.o
endif
ifeq (${SMASHFS_ENABLE_LZO}, y)
${MOD_NAME}-objs += compressor-lzo.o
endif
ifeq (${SMASHFS_ENABLE_XZ}, y)
${MOD_NAME}-objs += compressor-xz.o
endif

cflags-y  = -I${SUBDIRS}/../include

cflags-${SMASHFS_ENABLE_GZIP} += -DSMASHFS_ENABLE_GZIP=1
cflags-${SMASHFS_ENABLE_LZMA} += -DSMASHFS_ENABLE_LZMA=1
cflags-${SMASHFS_ENABLE_LZO}  += -DSMASHFS_ENABLE_LZO=1
cflags-${SMASHFS_ENABLE_XZ}   += -DSMASHFS_ENABLE_XZ=1

EXTRA_CFLAGS  = ${cflags-y}

KDIR := ${KERNEL_PATH}

all:
	ARCH=${CHIP_ARCH} CROSS_COMPILE=${CROSS_COMPILE_PREFIX} ${MAKE} -C ${KDIR} SUBDIRS=${CURDIR} modules

clean:
	@rm -rf *.o .*.o.d *.ko *.mod *.mod.c *~ .*.cmd *.symvers modules.order Module.markers
	@rm -rf .tmp_versions
