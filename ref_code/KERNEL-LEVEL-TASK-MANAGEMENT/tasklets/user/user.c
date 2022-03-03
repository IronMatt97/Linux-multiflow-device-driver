#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define TASK_EXE_REQUESTS 10

int main(int argc, char** argv){
	
	int i;
	int sys_call_num;

	if(argc < 2){
                printf("usage: prog syscall-num\n");
                return EXIT_FAILURE;
        }

	sys_call_num = strtol(argv[1],NULL,10);
	

	for (i=0;i<TASK_EXE_REQUESTS; i++){
		syscall(sys_call_num,i);
	}
}
