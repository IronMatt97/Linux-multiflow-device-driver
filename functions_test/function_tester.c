/*
* @file function_tester.c 
* @brief An utility program to test the kernel module's basic functions
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
#include <sys/ioctl.h>

int minors;
int i;
int action;
char **minors_list;
char buff[4096];

#define DATA "Test"
#define REPEAT while (1)
#define SIZE strlen(DATA)
#define BYTES_TO_READ 3

char *choose_device()
{
    int minor;
    printf("Select a minor number from available minors: ");
    scanf("%d", &minor);
    if (minor < 0 || minor > minors)
    {
        printf("\n\tInvalid minor chosen. Closing the program...\n");
        exit(-1);
    }
    return minors_list[minor];
}
int choose_action()
{
    int action;
    printf("Choose an action:\n");
    printf("1) Write on a device flow\n");
    printf("2) Read from a device flow\n");
    printf("3) Send an IOCTL to a device\n");
    printf("4) Close the session\n");
    printf("\n\tChosen action: ");
    scanf("%d", &action);
    if (action < 0 || action > 4)
        return -1;

    return action;
}
void do_write(int fd)
{
    write(fd, DATA, SIZE);
    printf("\t\t\tTHREAD WRITE COMPLETE.\n");
    return;
}
void do_read(int fd)
{
    char message[BYTES_TO_READ];
    read(fd, message, BYTES_TO_READ);
    printf("\t\t\tTHREAD READ COMPLETE - read data: '%s'.\n", message);
    return;
}
void do_ioctl(int fd)
{
    unsigned long timeout;
    printf("\nInsert the ioctl code you want to send to the device:\n");
    printf("0 - set low priority mode for RW operations for the device\n");
    printf("1 - set high priority mode for RW operations for the device\n");
    printf("2 - set the execution to non-blocking mode\n");
    printf("3 - set the execution to blocking mode\n");
    printf("4 - set the blocking operations timeout for the device (microseconds)\n");
    printf("5 - enable/disable the device\n");
    scanf("%d", &action);
    switch (action)
    {
    case 0:
        ioctl(fd, 0);
        break;
    case 1:
        ioctl(fd, 1);
        break;
    case 2:
        ioctl(fd, 6);
        break;
    case 3:
        ioctl(fd, 3);
        break;
    case 4:
        printf("Declare new timeout: ");
        scanf("%ld", &timeout);
        ioctl(fd, 4, timeout);
        break;
    case 5:
        ioctl(fd,5);
        break;
    default:
        printf("\n\tInvalid input. Restarting...\n");
        return;
    }
    printf("\t\t\tTHREAD IOCTL COMPLETE.\n");
    return;
}

void control_device(char *device)
{
    int fd = open(device, O_RDWR);
    if(fd == -1)
    {
        printf("OPEN ERROR - There was an error opening the device.\n");
        return;
    }
    REPEAT
    {
        action = choose_action();
        switch (action)
        {
            case 1:
                do_write(fd);
                break;
            case 2:
                do_read(fd);
                break;
            case 3:
                do_ioctl(fd);
                break;
            case 4:
                close(fd);
                return;
            default:
                printf("\n\tWrong input, restarting...\n");
        }
    }
}
int main(int argc, char **argv)
{
    int major = strtol(argv[2], NULL, 10);
    minors = strtol(argv[3], NULL, 10);
    char *path = argv[1];
    minors_list = malloc(minors * sizeof(char *));

    if (argc < 4)
    {
        printf("ERROR - WRONG PARAMETERS: usage -> prog pathname major minors\n");
        return -1;
    }
    printf("\n---------Multi-flow device driver tester initialization started correctly.\n\n");
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
        printf("\t\t%d) %s\n", i, minors_list[i]);
    }
    printf("\n\nHow to use: select an action to perform in the next prompt.\n");

    char *device;

    // User routine
    REPEAT
    {
        printf("--------------------------------------------------------------\n");
        printf("Welcome %s. This is a testing program.\n",getlogin());
        printf("Choose an action:\n");
        printf("1) Open a session to a device\n");
        printf("2) Leave\n");
        printf("\n\n\tChosen action: ");
        scanf("%d",&action);
        switch (action)
        {
            case 1:
                device = choose_device();
                control_device(device);
                break;
            case 2:
                exit(0);
            default:
                printf("\n\tWrong input, restarting...\n");
        }
    }
    return 0;
}



