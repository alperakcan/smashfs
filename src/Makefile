
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

subdir-y = \
	mkfs

ifeq ($(uname_S), Linux)
subdir-y += \
	linux
endif

include ../Makefile.lib
