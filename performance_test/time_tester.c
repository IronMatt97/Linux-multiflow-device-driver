/*
* @file time-tester.c 
* @brief A testing utility to measure system's performances
*
* @author Matteo Ferretti
*
* @date March 1, 2022
*/
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <time.h>

#define NUM_THREADS 100
#define NUM_TESTS 10
#define HIGH_PRIORITY_DATA "|TEST_DATA"
#define HIGH_PRIORITY_DATA_LENGTH strlen(HIGH_PRIORITY_DATA)
#define BYTES_TO_READ 2

char readbuff[BYTES_TO_READ];
char minor[50];
int i,k;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
FILE *fptr;

void * blocking_write_hi(void* path)
{
    double time_spent=0.0;
	int fd = open(minor,O_RDWR|O_APPEND);
	if(fd == -1)
	{
		printf("ERROR - something went wrong opening device %s. Returning...\n",minor);
		return NULL;
	}
	ioctl(fd,1);
	ioctl(fd,3);
    clock_t begin = clock();
	write(fd,HIGH_PRIORITY_DATA,HIGH_PRIORITY_DATA_LENGTH);
    clock_t end = clock();
	close(fd);

    time_spent += (double)(end - begin) / CLOCKS_PER_SEC;
    pthread_mutex_lock(&mutex);
    fptr = fopen((char*)path, "a");
    if (fptr == NULL) 
    {
        printf("Open error!\n");
        pthread_mutex_unlock(&mutex);
        return NULL;
    }
    fprintf(fptr, "%f;\n", time_spent);
    fclose(fptr);
    pthread_mutex_unlock(&mutex);
	return NULL;
}
void * blocking_read_hi(void* path)
{
    double time_spent=0.0;
	int fd = open(minor,O_RDWR|O_APPEND);
	if(fd == -1)
	{
		printf("ERROR - something went wrong opening device %s. Returning...\n",minor);
		return NULL;
	}
	ioctl(fd,1);
	ioctl(fd,3);
    clock_t begin = clock();
    read(fd, readbuff, BYTES_TO_READ);
    clock_t end = clock();
	close(fd);

    time_spent += (double)(end - begin) / CLOCKS_PER_SEC;
    pthread_mutex_lock(&mutex);
    fptr = fopen((char*)path, "a");
    if (fptr == NULL) 
    {
        printf("Open error!\n");
        pthread_mutex_unlock(&mutex);
        return NULL;
    }
    fprintf(fptr, "%f;\n", time_spent);
    fclose(fptr);
    pthread_mutex_unlock(&mutex);
	return NULL;
}

void main(int argc, char **argv)
{
    int major = strtol(argv[2],NULL,10);
    char *path = argv[1];
    pthread_t threads[NUM_THREADS];

    if (argc < 3)
    {
        printf("ERROR - WRONG PARAMETERS: usage -> prog pathname major\n");
        return;
    }
	printf("\n----------------------------------------------------------------\n");
    printf("Multi-flow device driver time tester initialization started correctly.");
	printf("\n----------------------------------------------------------------\n\n");
    printf("\t...Creating minor 0 for device %s (major %d)...\n", path, major);
    
    sprintf(minor, "mknod %s0 c %d 0\n", path, major);
    system(minor);
    sprintf(minor, "%s0", path);
    printf("\tSystem initialized. Minor to work on:\n");
    printf("\t\t%d) %s\n",i, minor);
	printf("\n\nThis is a testing program. Starting tests...\n");

    //Time test for writes
    for(k=0;k<NUM_TESTS;k++)
    {
        pthread_mutex_lock(&mutex);
        fptr = fopen("times_log_w", "a");
        if (fptr == NULL) 
        {
            printf("Open error!\n");
            pthread_mutex_unlock(&mutex);
            return;
        }
        fprintf(fptr, "\nWRITE TIMES;\n");
        fclose(fptr);
        pthread_mutex_unlock(&mutex);
        
        for (i = 0; i < NUM_THREADS; ++i)
            pthread_create(&threads[i], NULL, blocking_write_hi, "times_log_w");
        for (i = 0; i < NUM_THREADS; ++i)
            pthread_join(threads[i], NULL);
    }
    sleep(1);
    for(k=0;k<NUM_TESTS;k++)
    {
        pthread_mutex_lock(&mutex);
        fptr = fopen("times_log_w_single", "a");
        if (fptr == NULL) 
        {
            printf("Open error!\n");
            pthread_mutex_unlock(&mutex);
            return;
        }
        fprintf(fptr, "\nWRITE TIMES;\n");
        fclose(fptr);
        pthread_mutex_unlock(&mutex);
        
        for (i = 0; i < NUM_THREADS; ++i)
        {
            pthread_create(&threads[i], NULL, blocking_write_hi, "times_log_w_single");
            pthread_join(threads[i], NULL);
        }
    }
    printf("\t\tWrite test completed.\n");

    //Time test for reads

    for(k=0;k<NUM_TESTS;k++)
    {
        pthread_mutex_lock(&mutex);
        fptr = fopen("times_log_r", "a");
        if (fptr == NULL) 
        {
            printf("Open error!\n");
            pthread_mutex_unlock(&mutex);
            return;
        }
        fprintf(fptr, "\nREAD TIMES;\n");
        fclose(fptr);
        pthread_mutex_unlock(&mutex);
        
        for (i = 0; i < NUM_THREADS; ++i)
            pthread_create(&threads[i], NULL, blocking_read_hi, "times_log_r");
        for (i = 0; i < NUM_THREADS; ++i)
            pthread_join(threads[i], NULL);
    }
    sleep(1);
    for(k=0;k<NUM_TESTS;k++)
    {
        pthread_mutex_lock(&mutex);
        fptr = fopen("times_log_r_single", "a");
        if (fptr == NULL) 
        {
            printf("Open error!\n");
            pthread_mutex_unlock(&mutex);
            return;
        }
        fprintf(fptr, "\nREAD TIMES;\n");
        fclose(fptr);
        pthread_mutex_unlock(&mutex);
        
        for (i = 0; i < NUM_THREADS; ++i)
        {
            pthread_create(&threads[i], NULL, blocking_read_hi, "times_log_r_single");
            pthread_join(threads[i], NULL);
        }
    }
    printf("\t\tRead test completed.\n");    

}