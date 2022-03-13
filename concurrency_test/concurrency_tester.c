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
char buff[4096];

#define LOW_PRIORITY_DATA "Low_DATA"
#define LOW_PRIORITY_DATA_LENGTH strlen(LOW_PRIORITY_DATA)
#define HIGH_PRIORITY_DATA "High_DATA"
#define HIGH_PRIORITY_DATA_LENGTH strlen(HIGH_PRIORITY_DATA)
#define BYTES_TO_READ 2

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
	char *read_data = malloc(BYTES_TO_READ);
    read(fd, read_data, 2);
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
	char *read_data = malloc(BYTES_TO_READ);
    read(fd, read_data, 2);
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
	char *read_data = malloc(BYTES_TO_READ);
    read(fd, read_data, 2);
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
	char *read_data = malloc(BYTES_TO_READ);
    read(fd, read_data, 2);
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
    printf("\n----------Multi-flow device driver tester initialization started correctly.\n\n");
    printf("\t...Creating %d minors for device %s (major %d)\n", minors, path, major);
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
        printf("\t\t%s\n", minors_list[i]);
    }
	printf("\n\nThis is a testing program. Starting tests...\n");
	printf("\n\tTest 1 - concurrent writes...\n");
	for(i=0;i<minors;i++)
	{
		pthread_create(&tid1, NULL, low_priority_thread_w_b, strdup(minors_list[i]));
		pthread_create(&tid2, NULL, low_priority_thread_w_b, strdup(minors_list[i]));
		pthread_create(&tid3, NULL, low_priority_thread_w_nb, strdup(minors_list[i]));
		pthread_create(&tid4, NULL, low_priority_thread_w_nb, strdup(minors_list[i]));
		pthread_join(tid1,NULL);
		pthread_join(tid2,NULL);
		pthread_join(tid3,NULL);
		pthread_join(tid4,NULL);

		sleep(1);

		pthread_create(&tid1, NULL, high_priority_thread_w_b, strdup(minors_list[i]));
		pthread_create(&tid2, NULL, high_priority_thread_w_b, strdup(minors_list[i]));
		pthread_create(&tid3, NULL, high_priority_thread_w_nb, strdup(minors_list[i]));
		pthread_create(&tid4, NULL, high_priority_thread_w_nb, strdup(minors_list[i]));
		pthread_join(tid1,NULL);
		pthread_join(tid2,NULL);
		pthread_join(tid3,NULL);
		pthread_join(tid4,NULL);

		sleep(1);
	}
	printf("\t\tdone.\n");

	printf("\n\tTest 2 - concurrent reads...\n");
	for(i=0;i<minors;i++)
	{
		pthread_create(&tid1, NULL, low_priority_thread_r_b, strdup(minors_list[i]));
		pthread_create(&tid2, NULL, low_priority_thread_r_b, strdup(minors_list[i]));
		pthread_create(&tid3, NULL, low_priority_thread_r_nb, strdup(minors_list[i]));
		pthread_create(&tid4, NULL, low_priority_thread_r_nb, strdup(minors_list[i]));
		pthread_join(tid1,NULL);
		pthread_join(tid2,NULL);
		pthread_join(tid3,NULL);
		pthread_join(tid4,NULL);

		sleep(1);

		pthread_create(&tid1, NULL, high_priority_thread_r_b, strdup(minors_list[i]));
		pthread_create(&tid2, NULL, high_priority_thread_r_b, strdup(minors_list[i]));
		pthread_create(&tid3, NULL, high_priority_thread_r_nb, strdup(minors_list[i]));
		pthread_create(&tid4, NULL, high_priority_thread_r_nb, strdup(minors_list[i]));
		pthread_join(tid1,NULL);
		pthread_join(tid2,NULL);
		pthread_join(tid3,NULL);
		pthread_join(tid4,NULL);

		sleep(1);
	}
	printf("\t\tdone.\n");

	printf("\n\tTest 3 - concurrent writes and reads...\n");
	for(i=0;i<minors;i++)
	{
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
	}
	printf("\t\tdone.\n");

	printf("/n/nTesting complete.");

    return 0;
}
