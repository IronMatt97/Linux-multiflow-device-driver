obj-m += the_memory-map-inspector.o
the_memory-map-inspector-objs += memory-map-inspector.o lib/scth.o

A = $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

mount:
	insmod the_memory-map-inspector.ko the_syscall_table=$(A) 

