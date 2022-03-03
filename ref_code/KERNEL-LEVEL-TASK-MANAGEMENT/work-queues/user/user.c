#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define TASK_EXE_REQUESTS 10

int main(int argc, char** argv){
	
	int i;
	int sys_call_num;
	int core_num;

	if(argc < 3){
                printf("usage: prog syscall-num core-num\n");
                return 1;
        }

	sys_call_num = strtol(argv[1],NULL,10);
	core_num = strtol(argv[2],NULL,10);
	

	for (i=0;i<TASK_EXE_REQUESTS; i++){
		syscall(sys_call_num,core_num,i);
	}

	 return 0;
}
