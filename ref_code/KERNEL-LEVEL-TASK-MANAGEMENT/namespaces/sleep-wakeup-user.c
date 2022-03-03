#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


int sleep_or_awake(long int x){
	return syscall(x);
}

void* do_job(void * arg){
	sleep_or_awake((long int)arg);
	return NULL;
}

int main(int argc, char** argv){
	
	long int num_threads, arg;	
	pthread_t tid;
	int i;

	if(argc < 2){
		printf("usage: prog  sycall-num\n");
		return EXIT_FAILURE;
	}
	
	
	arg = strtol(argv[1],NULL,10);
	
	printf("process %d calling sleep/wakeup service\n",getpid());

	sleep_or_awake(arg);

}
