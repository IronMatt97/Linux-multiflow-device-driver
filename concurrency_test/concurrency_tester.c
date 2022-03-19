#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>

int i;
int action;
int minors;
unsigned long timeout;
char **minors_list;
pthread_t tid1, tid2, tid3, tid4;
char buff[50];


#define LOW_PRIORITY_DATA "LO_DATA"
#define LOW_PRIORITY_DATA_LENGTH strlen(LOW_PRIORITY_DATA)
#define HIGH_PRIORITY_DATA "HI_DATA"
#define HIGH_PRIORITY_DATA_LENGTH strlen(HIGH_PRIORITY_DATA)
#define BYTES_TO_READ 2

char r1[BYTES_TO_READ];
char r2[BYTES_TO_READ];
char r3[BYTES_TO_READ];
char r4[BYTES_TO_READ];

//A thread writing in the low priority stream in blocking mode
void * low_priority_thread_w_b(void* path)
{
	char* device = (char*)path;
	int fd = open(device,O_RDWR|O_APPEND);
	if(fd == -1)
	{
		printf("ERROR - something went wrong opening device %s. Returning...\n",device);
		return NULL;
	}
	ioctl(fd,0);
	ioctl(fd,3);
	write(fd,LOW_PRIORITY_DATA,LOW_PRIORITY_DATA_LENGTH);
	close(fd);
	return NULL;
}
//A thread writing in the low priority stream in non-blocking mode
void * low_priority_thread_w_nb(void* path)
{
	char* device = (char*)path;
	int fd = open(device,O_RDWR|O_APPEND);
	if(fd == -1)
	{
		printf("ERROR - something went wrong opening device %s. Returning...\n",device);
		return NULL;
	}
	ioctl(fd,0);
	ioctl(fd,2);
	write(fd,LOW_PRIORITY_DATA,LOW_PRIORITY_DATA_LENGTH);
	close(fd);
	return NULL;
}
//A thread writing in the high priority stream in blocking mode
void * high_priority_thread_w_b(void* path)
{
	char* device = (char*)path;
	int fd = open(device,O_RDWR|O_APPEND);
	if(fd == -1)
	{
		printf("ERROR - something went wrong opening device %s. Returning...\n",device);
		return NULL;
	}
	ioctl(fd,1);
	ioctl(fd,3);
	write(fd,HIGH_PRIORITY_DATA,HIGH_PRIORITY_DATA_LENGTH);
	close(fd);
	return NULL;
}
//A thread writing in the high priority stream in non-blocking mode
void * high_priority_thread_w_nb(void* path)
{
	char* device = (char*)path;
	int fd = open(device,O_RDWR|O_APPEND);
	if(fd == -1)
	{
		printf("ERROR - something went wrong opening device %s. Returning...\n",device);
		return NULL;
	}
	ioctl(fd,1);
	ioctl(fd,2);
	write(fd,HIGH_PRIORITY_DATA,HIGH_PRIORITY_DATA_LENGTH);
	close(fd);
	return NULL;
}
//A thread reading in the low priority stream in blocking mode
void * low_priority_thread_r_b(void* path)
{
	char* device = (char*)path;
	int fd = open(device,O_RDWR|O_APPEND);
	if(fd == -1)
	{
		printf("ERROR - something went wrong opening device %s. Returning...\n",device);
		return NULL;
	}
	ioctl(fd,0);
	ioctl(fd,3);
    read(fd, r1, BYTES_TO_READ);
	close(fd);
	return NULL;
}
//A thread reading in the low priority stream in non-blocking mode
void * low_priority_thread_r_nb(void* path)
{
	char* device = (char*)path;
	int fd = open(device,O_RDWR|O_APPEND);
	if(fd == -1)
	{
		printf("ERROR - something went wrong opening device %s. Returning...\n",device);
		return NULL;
	}
	ioctl(fd,0);
	ioctl(fd,2);
    read(fd, r2, BYTES_TO_READ);
	close(fd);
	return NULL;
}
//A thread reading in the high priority stream in blocking mode
void * high_priority_thread_r_b(void* path)
{
	char* device = (char*)path;
	int fd = open(device,O_RDWR|O_APPEND);
	if(fd == -1)
	{
		printf("ERROR - something went wrong opening device %s. Returning...\n",device);
		return NULL;
	}
	ioctl(fd,1);
	ioctl(fd,3);
    read(fd, r3, BYTES_TO_READ);
	close(fd);
	return NULL;
}
//A thread reading in the high priority stream in non-blocking mode
void * high_priority_thread_r_nb(void* path)
{
	char* device = (char*)path;
	int fd = open(device,O_RDWR|O_APPEND);
	if(fd == -1)
	{
		printf("ERROR - something went wrong opening device %s. Returning...\n",device);
		return NULL;
	}
	ioctl(fd,1);
	ioctl(fd,2);
    read(fd, r4, BYTES_TO_READ);
	close(fd);
	return NULL;
}
void * special(void* path)
{
	printf("sono il thread\n");
	char* device = (char*)path;
	int fd = open(device,O_RDWR|O_APPEND);
	if(fd == -1)
	{
		printf("ERROR - something went wrong opening device %s. Returning...\n",device);
		return NULL;
	}
	ioctl(fd,1);
	ioctl(fd,3);
	write(fd,HIGH_PRIORITY_DATA,HIGH_PRIORITY_DATA_LENGTH);

    read(fd, r4, BYTES_TO_READ);
	close(fd);
	return NULL;
}
int main(int argc, char** argv)
{
	int major = strtol(argv[2],NULL,10);
    minors = strtol(argv[3],NULL,10);
    char *path = argv[1];
	minors_list = malloc(minors*sizeof(char*));

    if (argc < 4)
    {
        printf("ERROR - WRONG PARAMETERS: usage -> prog pathname major minors\n");
        return -1;
    }
	printf("\n----------------------------------------------------------------\n");
    printf("Multi-flow device driver tester initialization started correctly.");
	printf("\n----------------------------------------------------------------\n\n");
    printf("\t...Creating %d minors for device %s (major %d)...\n", minors, path, major);
    for (i = 0; i < minors; i++)
    {
        sprintf(buff, "mknod %s%d c %d %i\n", path, i, major, i);
        system(buff);
        sprintf(buff, "%s%d", path, i);
        minors_list[i] = malloc(32);
        strcpy(minors_list[i], buff);
    }
    printf("\tSystem initialized. Minors list:\n");
    for (i = 0; i < minors; i++)
    {
        printf("\t\t%d) %s\n",i, minors_list[i]);
    }
	printf("\n\nThis is a testing program. Starting tests...\n");
	
	for(i=0;i<minors;i++)
	{/*
		printf("\n\t|--------------------------|");
		printf("\n\t Test subject: %s\n",minors_list[i]);
		printf("\t|--------------------------|\n");

		printf("\n\tTest 1 - concurrent writes...\n");
		pthread_create(&tid1, NULL, low_priority_thread_w_b, strdup(minors_list[i]));
		pthread_create(&tid2, NULL, low_priority_thread_w_nb, strdup(minors_list[i]));
		pthread_create(&tid3, NULL, low_priority_thread_w_b, strdup(minors_list[i]));
		pthread_create(&tid4, NULL, low_priority_thread_w_nb, strdup(minors_list[i]));
		pthread_join(tid1,NULL);
		pthread_join(tid2,NULL);
		pthread_join(tid3,NULL);
		pthread_join(tid4,NULL);	
		
		sleep(1);

		pthread_create(&tid1, NULL, high_priority_thread_w_b, strdup(minors_list[i]));
		pthread_create(&tid2, NULL, high_priority_thread_w_nb, strdup(minors_list[i]));
		pthread_create(&tid3, NULL, high_priority_thread_w_b, strdup(minors_list[i]));
		pthread_create(&tid4, NULL, high_priority_thread_w_nb, strdup(minors_list[i]));
		pthread_join(tid1,NULL);
		pthread_join(tid2,NULL);
		pthread_join(tid3,NULL);
		pthread_join(tid4,NULL);

		sleep(1);
		printf("\t\tdone.\n");

		printf("\n\tTest 2 - concurrent reads...\n");
		pthread_create(&tid1, NULL, low_priority_thread_r_b, strdup(minors_list[i]));
		pthread_create(&tid2, NULL, low_priority_thread_r_nb, strdup(minors_list[i]));
		pthread_create(&tid3, NULL, low_priority_thread_r_b, strdup(minors_list[i]));
		pthread_create(&tid4, NULL, low_priority_thread_r_nb, strdup(minors_list[i]));
		pthread_join(tid1,NULL);
		pthread_join(tid2,NULL);
		pthread_join(tid3,NULL);
		pthread_join(tid4,NULL);

		sleep(1);

		pthread_create(&tid1, NULL, high_priority_thread_r_b, strdup(minors_list[i]));
		pthread_create(&tid2, NULL, high_priority_thread_r_nb, strdup(minors_list[i]));
		pthread_create(&tid3, NULL, high_priority_thread_r_b, strdup(minors_list[i]));
		pthread_create(&tid4, NULL, high_priority_thread_r_nb, strdup(minors_list[i]));
		pthread_join(tid1,NULL);
		pthread_join(tid2,NULL);
		pthread_join(tid3,NULL);
		pthread_join(tid4,NULL);

		sleep(1);
		printf("\t\tdone.\n");

		printf("\n\tTest 3 - concurrent writes and reads...\n");
		pthread_create(&tid1, NULL, low_priority_thread_w_b, strdup(minors_list[i]));
		pthread_create(&tid2, NULL, low_priority_thread_w_nb, strdup(minors_list[i]));
		pthread_create(&tid3, NULL, low_priority_thread_r_b, strdup(minors_list[i]));
		pthread_create(&tid4, NULL, low_priority_thread_r_nb, strdup(minors_list[i]));
		pthread_join(tid1,NULL);
		pthread_join(tid2,NULL);
		pthread_join(tid3,NULL);
		pthread_join(tid4,NULL);

		sleep(1);

		pthread_create(&tid1, NULL, high_priority_thread_w_b, strdup(minors_list[i]));
		pthread_create(&tid2, NULL, high_priority_thread_w_nb, strdup(minors_list[i]));
		pthread_create(&tid3, NULL, high_priority_thread_r_b, strdup(minors_list[i]));
		pthread_create(&tid4, NULL, high_priority_thread_r_nb, strdup(minors_list[i]));
		pthread_join(tid1,NULL);
		pthread_join(tid2,NULL);
		pthread_join(tid3,NULL);
		pthread_join(tid4,NULL);

		sleep(1);
		printf("\t\tdone.\n");
		*/
		printf("Special test\n");
		pthread_create(&tid4, NULL, special, strdup(minors_list[i]));
		pthread_join(tid4,NULL);
		printf("thread launched\n");
	}


	printf("\n\nTesting complete. Use 'dmesg' to see the outcome.\n\n");

    return 0;
}
