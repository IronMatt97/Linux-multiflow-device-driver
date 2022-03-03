obj-m += the_queuing-service.o
the_queuing-service-objs += queuing-service.o lib/scth.o

A = $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

mount:
	insmod the_queuing-service.ko the_syscall_table=$(A)

enable:
	echo "1" > /sys/module/the_queuing_service/parameters/enable_sleep 

disable:
	echo "0" > /sys/module/the_queuing_service/parameters/enable_sleep 

check:
	cat /sys/module/the_queuing_service/parameters/count

