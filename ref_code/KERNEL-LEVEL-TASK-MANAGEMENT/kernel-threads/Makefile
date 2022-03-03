
obj-m += kernel_thread.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

stop:
	echo "1" > /sys/module/kernel_thread/parameters/shutdown_daemon
