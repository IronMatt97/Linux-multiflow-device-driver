

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>

#ifdef _MSC_VER
#include <intrin.h> /* for rdtscp and clflush */
#pragma optimize("gt",on)
#else
#include <x86intrin.h> /* for rdtscp and clflush */
#endif


#define NUM_TRIES 10

float scale = 2800; //number of clock cycles per microsecond (2800 is for a 2.8GHz processor)

int my_sleep(long int x, unsigned long y){
	return syscall(x,y);
}


int main(int argc, char** argv){
	
	long int arg;	
	unsigned long timeout;
	int i;
	unsigned long cumulative, time1, time2;
	int junk = 0;


	if(argc < 3){
		printf("usage: prog sys_call-num sleep_timeout\n");
		return EXIT_FAILURE;
	}	
	
	
	timeout = strtol(argv[2],NULL,10);
	arg = strtol(argv[1],NULL,10);
	
	printf("expected number of clock cycles per %lu microsecods timeout is %lu\n",timeout,(unsigned long)(scale*timeout));

	cumulative = 0;
	for (i=0; i<NUM_TRIES; i++){
		time1 = __rdtscp( & junk);
		my_sleep(arg,timeout);
		time2 = __rdtscp( & junk);
		cumulative += time2 - time1;
	}
	printf("my_sleep average sleep cycles: %lu\n",cumulative/NUM_TRIES);

	cumulative = 0;
	for (i=0; i<NUM_TRIES; i++){
		time1 = __rdtscp( & junk);
		usleep(timeout);
		time2 = __rdtscp( & junk);
		cumulative += time2 - time1;
	}
	printf("usleep average sleep cycles: %lu\n",cumulative/NUM_TRIES);
	fflush(stdout);

	return 0;
}
