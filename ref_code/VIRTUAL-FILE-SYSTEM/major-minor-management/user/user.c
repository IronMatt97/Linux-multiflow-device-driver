#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>


int main(int argc, char** argv){

     int ret;

     if(argc<2){
	printf("useg: prog pathname");
	return -1;
     }

      printf("trying to open %s\n",argv[1]);

      ret = open(argv[1],O_RDWR);
      printf("open returned %d\n",ret);
      
      ret = write(ret,"ab",2);
      printf("write returned %d\n",ret);
	
      pause();
}
