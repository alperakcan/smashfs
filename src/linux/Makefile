
MOD_NAME    ?= smashfs

CHIP_ARCH   ?= x86
KERNEL_PATH ?= /usr/src/linux

obj-m := ${MOD_NAME}.o

${MOD_NAME}-objs  = fs.o
${MOD_NAME}-objs += super.o

cflags-y  = -I${SUBDIRS}/../include

EXTRA_CFLAGS  = ${cflags-y}

KDIR := ${KERNEL_PATH}

all:
	ARCH=${CHIP_ARCH} CROSS_COMPILE=${CROSS_COMPILE_FREFIX} ${MAKE} -C ${KDIR} SUBDIRS=${CURDIR} modules

clean:
	@rm -rf *.o .*.o.d *.ko *.mod *.mod.c *~ .*.cmd *.symvers modules.order Module.markers
	@rm -rf .tmp_versions
