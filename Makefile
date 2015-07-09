obj-m := mfs.o
ccflags-y := -DSIMPLEFS_DEBUG

all: ko mkfs.mfs

mkfs.mfs: mkmfs.c
	gcc -o mkfs.mfs mkmfs.c

ko:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f mkfs.mfs
