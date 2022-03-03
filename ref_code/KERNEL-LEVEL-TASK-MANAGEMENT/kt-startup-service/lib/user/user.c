#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define VTPMO 134 //this depends on what the kernel tells you when mounting the vtpmo module

int vtpmo(unsigned long x){
	return syscall(VTPMO,x);
}


#define NUM_TARGET_PAGES 10

char buff[4096*NUM_TARGET_PAGES]__attribute__((aligned(4096)));

int main(int argc, char** argv){
        
       	int frame_num;
	int i;
	char c;
        
	if(argc == 2) buff[0] = 'q';

       	frame_num = vtpmo((unsigned long)buff);
	printf("address %p mapped to frame %d - errno is %d\n",buff,frame_num,errno);

       	frame_num = vtpmo((unsigned long)(buff+1));
	printf("address %p mapped to frame %d - errno is %d\n",buff+1,frame_num,errno);

	if(argc == 2){ 
		if(!strcmp(argv[1],"read")){ 
			printf("read path\n");
			for(i=0;i<NUM_TARGET_PAGES;i++){
				c = buff[i*4096] ;
				frame_num = vtpmo((unsigned long)(buff+(i*4096)));
        			printf("address %p mapped to frame %d - errno is %d\n",(buff+(i*4096)),frame_num,errno);
			}
		}

		if(!strcmp(argv[1],"write")){ 
			printf("write path\n");
			for(i=0;i<NUM_TARGET_PAGES;i++){
				buff[i*4096] = 'q' ;
				frame_num = vtpmo((unsigned long)(buff+(i*4096)));
        			printf("address %p mapped to frame %d - errno is %d\n",(buff+(i*4096)),frame_num,errno);
			}
		}
		if(!strcmp(argv[1],"vtpmo")){ 
			printf("vtpmo path\n");
			for(i=0;i<NUM_TARGET_PAGES;i++){
				frame_num = vtpmo((unsigned long)(buff+(i*4096)));
        			printf("address %p mapped to frame %d - errno is %d\n",(buff+(i*4096)),frame_num,errno);
			}
		}
	}

	printf("\nnow look at the stack and NULL\n\n");


       	frame_num = vtpmo((unsigned long)&frame_num);
	printf("address %p mapped to frame %d - errno is %d\n",&frame_num,frame_num,errno);

       	frame_num = vtpmo((unsigned long)NULL);
	printf("address %p mapped to frame %d - errno is %d\n",NULL,frame_num,errno);
 
        return frame_num;
}

