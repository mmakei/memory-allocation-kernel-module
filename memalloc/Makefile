obj-m += memalloc.o
memalloc-y += memalloc-main.o memalloc-helper.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	insmod memalloc.ko

remove:
	rmmod memalloc
