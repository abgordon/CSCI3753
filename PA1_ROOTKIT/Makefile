obj-m += rkit.o

KDIR = /usr/src/linux-headers-3.13.0-35-generic

all:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	rm -rf *.o *.ko *.mod *.sysmvers *.order

all: $(SOURCES)