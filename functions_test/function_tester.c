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
int timeout;
char **minors_list;
pthread_t tid;
char buff[4096];

#define DATA "Test"
#define REPEAT while(1)
#define SIZE strlen(DATA)

void default_prompt(void)
{
    printf("--------------------\n");
    printf("Choose an action:\n");
    printf("1) Write on a device flow\n");
    printf("2) Read from a device flow\n");
    printf("3) Send an IOCTL to a device\n");
    printf("\n\tChosen action: ");
    return;
}

void * thread_write(void* path)
{
    char* device = (char*)path;
	int fd = open(device,O_RDWR|O_APPEND);
	if(fd == -1)
	{
		printf("\t\t\tTHREAD ERROR: open error on device %s\n",device);
		return NULL;
	}
	write(fd,DATA,strlen(DATA));
	close(fd);
	printf("\t\t\tTHREAD WRITE COMPLETE.\n");
	return NULL;
}
void * thread_read(void* path)
{
    char* device = (char*)path;
	int fd = open(device,O_RDWR);
	if(fd == -1)
	{
		printf("\t\t\tTHREAD ERROR: open error on device %s\n",device);
		return NULL;
	}
	char *message = malloc(2);
	read(fd,message,2);
	close(fd);
	printf("\t\t\tTHREAD READ COMPLETE - read data: '%s'.\n",message);
	return NULL;
}
void * thread_ioctl_set_low(void* path)
{
    char* device = (char*)path;
	int fd = open(device,O_RDWR);
	if(fd == -1)
	{
		printf("\t\t\tTHREAD ERROR: open error on device %s\n",device);
		return NULL;
	}
	ioctl(fd,0);
    close(fd);
	printf("\t\t\tTHREAD IOCTL COMPLETE.\n");
	return NULL;
}
void * thread_ioctl_set_high(void* path)
{
    char* device = (char*)path;
	int fd = open(device,O_RDWR);
	if(fd == -1)
	{
		printf("\t\t\tTHREAD ERROR: open error on device %s\n",device);
		return NULL;
	}
	ioctl(fd,1);
    close(fd);
	printf("\t\t\tTHREAD IOCTL COMPLETE.\n");
	return NULL;
}
void * thread_ioctl_set_non_blocking(void* path)
{
    char* device = (char*)path;
	int fd = open(device,O_RDWR);
	if(fd == -1)
	{
		printf("\t\t\tTHREAD ERROR: open error on device %s\n",device);
		return NULL;
	}
	ioctl(fd,2);
    close(fd);
	printf("\t\t\tTHREAD IOCTL COMPLETE.\n");
	return NULL;
}
void * thread_ioctl_set_blocking(void* path)
{
    char* device = (char*)path;
	int fd = open(device,O_RDWR);
	if(fd == -1)
	{
		printf("\t\t\tTHREAD ERROR: open error on device %s\n",device);
		return NULL;
	}
	ioctl(fd,3);
    close(fd);
	printf("\t\t\tTHREAD IOCTL COMPLETE.\n");
	return NULL;
}
void * thread_ioctl_set_timeout(void* path)
{
    char* device = (char*)path;
	int fd = open(device,O_RDWR);
	if(fd == -1)
	{
		printf("\t\t\tTHREAD ERROR: open error on device %s\n",device);
		return NULL;
	}
	ioctl(fd,4,timeout);
    close(fd);
	printf("\t\t\tTHREAD IOCTL COMPLETE.\n");
	return NULL;
}
void * thread_ioctl_enable_disable(void* path)
{
    char* device = (char*)path;
	int fd = open(device,O_RDWR);
	if(fd == -1)
	{
		printf("\t\t\tTHREAD ERROR: open error on device %s\n",device);
		return NULL;
	}
	ioctl(fd,5);
    close(fd);
	printf("\t\t\tTHREAD IOCTL COMPLETE.\n");
	return NULL;
}
void do_write()
{
    int minor;
    printf("Insert the minor number you want to write to: ");
    scanf("%d",&minor);
    if( minor < 0 || minor > minors)
    {
        printf("\n\tInvalid minor chosen. Restarting...\n");
        return;
    }
    
    pthread_create(&tid,NULL,thread_write,strdup(minors_list[minor]));
    
    printf("A thread has been spawned to perform the request.\n");
    return;
}
void do_read()
{
    int minor;
    printf("Insert the minor number you want to read from: ");
    scanf("%d",&minor);
    if( minor < 0 || minor > minors)
    {
        printf("\n\tInvalid minor chosen. Restarting...\n");
        return;
    }
    pthread_create(&tid,NULL,thread_read,strdup(minors_list[minor]));
    printf("A thread has been spawned to perform the request.\n");
    return;
}
void do_ioctl()
{
    int minor;
    printf("Insert the minor number you want to send an ioctl to: ");
    scanf("%d",&minor);
    if( minor < 0 || minor > minors)
    {
        printf("\n\tInvalid minor chosen. Restarting...\n");
        return;
    }
    printf("Insert the ioctl code you want to send to device with minor %d:\n",minor);
    printf("0 - set low priority mode for RW operations for the device\n");
    printf("1 - set high priority mode for RW operations for the device\n");
    printf("2 - set the execution to non-blocking mode\n");
    printf("3 - set the execution to blocking mode\n");
    printf("4 - set the blocking operations timeout for the device\n");
    printf("5 - enable/disable the device\n");
    scanf("%d",&action);
    switch (action)
    {
        case 0:
            pthread_create(&tid,NULL,thread_ioctl_set_low,strdup(minors_list[minor]));
            break;
        case 1:
            pthread_create(&tid,NULL,thread_ioctl_set_high,strdup(minors_list[minor]));
            break;
        case 2:
            pthread_create(&tid,NULL,thread_ioctl_set_non_blocking,strdup(minors_list[minor]));
            break;
        case 3:
            pthread_create(&tid,NULL,thread_ioctl_set_blocking,strdup(minors_list[minor]));
            break;
        case 4:
            printf("Declare new timeout: ");
            scanf("%d",&timeout);
            pthread_create(&tid,NULL,thread_ioctl_set_timeout,strdup(minors_list[minor]));
            break;
        case 5:
            pthread_create(&tid,NULL,thread_ioctl_enable_disable,strdup(minors_list[minor]));
            break;
        default:
            printf("\n\tInvalid input. Restarting...\n");
            return;
    }
    printf("A thread has been spawned to perform the request.\n");
    return;
}
int main(int argc, char** argv)
{
	int major = strtol(argv[2],NULL,10);
    minors = strtol(argv[3],NULL,10);
    char *path = argv[1];
	minors_list = malloc(minors*sizeof(char*));

    if(argc<4)
	{
		printf("ERROR - WRONG PARAMETERS: usage -> prog pathname major minors\n");
		return -1;
    }
    printf("\n----------Multi-flow device driver tester correctly initialized.\n\n");
	printf("\t...Creating %d minors for device %s (major %d)\n",minors,path,major);
	for(i=0;i<minors;i++)
	{
		sprintf(buff,"mknod %s%d c %d %i\n",path,i,major,i);
		system(buff);
		sprintf(buff,"%s%d",path,i);
		minors_list[i] = malloc(32);
		strcpy(minors_list[i],buff);
    }
    printf("\tSystem initialized. Minors list:\n");
	for(i=0;i<minors;i++)
	{
		printf("\t\t%s\n",minors_list[i]);
    }
    printf("\n\nHow to use: select an action in the next prompt and a thread will be spawned to perform the request.\n");

    //User routine
    REPEAT
    {
        default_prompt();
        scanf("%d",&action);
        switch (action)
        {
            case 1:
                do_write();
                break;
            case 2:
                do_read();
                break;
            case 3:
                do_ioctl();
                break;
            default:
                printf("\n\tWrong input, restarting...\n");
        }
    }
    return 0;
}