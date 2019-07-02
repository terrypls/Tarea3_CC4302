ccflags-y := -Wall

obj-m := buffer.o
buffer-objs := kmutex.o buffer-impl.o

KDIR  := /lib/modules/$(shell uname -r)/build
PWD   := $(shell pwd)

#include $(KDIR)/.config

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

buffer-impl.o kmutex.o: kmutex.h

clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean
