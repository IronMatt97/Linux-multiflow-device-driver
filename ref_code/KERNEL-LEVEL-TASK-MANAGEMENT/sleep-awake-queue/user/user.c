

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>



int sleep_or_awake(long int x){
	return syscall(x);
}

void* do_job(void * arg){
	int res;
	res = sleep_or_awake((long int)arg);
	printf("sys call %ld returned value %d\n",(long int)arg,res);
	return NULL;
}

int main(int argc, char** argv){
	
	long int num_threads, arg;	
	pthread_t tid;
	int i;

	if(argc < 3){
		printf("usage: prog num-spawns sycall-num\n");
		return EXIT_FAILURE;
	}
	
	
	num_threads = strtol(argv[1],NULL,10);
	arg = strtol(argv[2],NULL,10);
	

	for (i=0; i<num_threads; i++){
		pthread_create(&tid,NULL,do_job,(void*)arg);
	}

	pause();

}
