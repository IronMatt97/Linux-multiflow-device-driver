#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#define STARTDAEMON 134 //this depends on what the kernel tells you when mounting the mm-inpector module

int start_daemon(void){
	return syscall(STARTDAEMON);
}



int main(int argc, char** argv){
       
	int ret; 

       	ret = start_daemon();
	printf("start daemon returned with code %d\n",ret);

}

