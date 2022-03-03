#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#define GETPID 134 //this depends on what the kernel tells you when mounting the mm-inpector module

int get_pid(unsigned long x){
	return syscall(GETPID,x);
}


int main(int argc, char** argv){
        
       	int ret;

	printf("getpid() is %d\n",getpid());

       	ret = get_pid(0);
	printf("real pid is: %d\n",ret);

       	ret = get_pid(1);
	printf("virtual pid is: %d\n",ret);


}

