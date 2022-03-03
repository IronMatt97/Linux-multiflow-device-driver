#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>


long val = 0;

int main(int argc, char** argv){
	
	char *p;

	p = (char*)mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,0,0);

	printf("I'm process %d - target addr is: %lu - mmap address is %lu\n",getpid(),&val,p);


	while(1){
		sleep(3);
		printf("value is: %ld\n",val);
		if (val == 1024) break;

	}

	printf("mmap value is: %ld\n",((unsigned long*)p)[0]);//did somebody already materialize memory with non-null value?

}
