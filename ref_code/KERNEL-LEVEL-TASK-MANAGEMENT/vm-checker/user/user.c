#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#define MMINSP 134 //this depends on what the kernel tells you when mounting the mm-inpector module

int mm_inspector(unsigned long x, int y){
	return syscall(MMINSP,x,y);
}


#define NUM_TARGET_PAGES 10

char buff[4096*NUM_TARGET_PAGES] __attribute__ ((aligned(4096)));

int main(int argc, char** argv){
        
       	int ret;

	buff[0] = 'f';

       	ret = mm_inspector((unsigned long)buff,0);
	printf("address %p mapped: %d - read only query\n",buff,ret);

       	ret = mm_inspector((unsigned long)buff,1);
	printf("address %p mapped: %d - read/write query\n",buff,ret);

       	ret = mm_inspector((unsigned long)buff,2);
	printf("address %p mapped: %d - exec query\n",buff,ret);


}

