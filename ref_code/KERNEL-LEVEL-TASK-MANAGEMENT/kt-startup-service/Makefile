obj-m += the_kt-startup-service.o
the_kt-startup-service-objs += kt-startup-service.o lib/scth.o

A = $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

mount:
	insmod the_kt-startup-service.ko the_syscall_table=$(A)

shutdown:
	echo "1" > /sys/module/the_kt_startup_service/parameters/shutdown_daemon 



