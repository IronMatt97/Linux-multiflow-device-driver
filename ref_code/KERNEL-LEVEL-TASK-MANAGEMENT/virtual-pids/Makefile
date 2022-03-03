obj-m += the_virtual-pids.o
the_virtual-pids-objs += virtual-pids.o lib/scth.o

A = $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

mount:
	insmod the_virtual-pids.ko the_syscall_table=$(A)

