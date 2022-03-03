

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../include/monitor.h"


#define AUDIT if(1)

int monitor_reset(long int x){
	return syscall(x);
}

int monitor_get(long int x, monitor * p){
	return syscall(x,p);
}


int main(int argc, char** argv){
	
	long int monitor_reset_code;	
	long int monitor_get_code;
	monitor info;
	int i;
	int ret;

	if(argc < 3){
		printf("need argv[1] and argv[2] (the numbers corresponding to the monitoring service system-calls (please check with dmesg)\n");
		return -1;
	}

	monitor_reset_code = strtol(argv[1],NULL,10);
	monitor_get_code = strtol(argv[2],NULL,10);

	ret = monitor_reset(monitor_reset_code);
	printf("monitor reset service returned %d\n",ret);

retry:
	sleep(1);

for(i=0;i<100;i++){
	ret = monitor_get(monitor_get_code, &info);
	printf("monitor get service returned %d\n",ret);

	printf("running on CPU %d - CPU switches are %d\n",info.cpu_id,info.switches);
	goto retry;
	break;
}

}
