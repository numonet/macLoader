///////////////////////////////////////////////////
// File name:	Common.c
//
//
//  Created on: 05/02/2016
//      Author: Mian Tang
//
//
///////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "termios.h"






/////////////////////////////////////////////////////////////////////////////////////
// the extern variable
/////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////
// Local variable
/////////////////////////////////////////////////////////////////////////////////////

static int 	uart_fd;
static FILE*	logf;

pthread_mutex_t uart_lock;
pthread_mutex_t thread_lock;



 
/////////////////////////////////////////////////////////////////////////////////////
// Function Name:               Com_Init
// Function Description:        Initialize Uart port for transmission
// Function Parameters:
//                      INPUT:                  None
//                      OUTPUT:                 None
// Return Value:
//                                                      0: no error
//                                                      1: an error occur
/////////////////////////////////////////////////////////////////////////////////////
int Com_Init(const char* dev_path, int baudrate, int data_bits, int stop_bits, char parity)
{
    const int speed_arr[] = {B115200, B57600, B38400, B19200, B9600, B4800};
    const int name_arr[6] = {115200, 57600, 38400, 19200, 9600, 4800};
    struct termios opt;
    int i ,err = 0;


    if (pthread_mutex_init(&uart_lock, NULL) != 0) {
        printf("Uart Mutex Initialized failed.\r\n");
    }
    if (pthread_mutex_init(&thread_lock, NULL) != 0) {
        printf("Multi-thread Mutex Initialized failed.\r\n");
    }
    uart_fd = open(dev_path, O_RDWR);
    if (uart_fd >= 0) {
        if(tcgetattr(uart_fd, &opt) == -1) {
            printf("Get Uart Attribute Error.\r\n");
            err = -2;
        } else {
            // Configure Baud rate
            for (i = 0; i < 6; i ++) {
                if (baudrate == name_arr[i]) {
                    tcflush(uart_fd, TCIOFLUSH);
                    cfsetispeed(&opt, speed_arr[i]);
                    cfsetospeed(&opt, speed_arr[i]);
                    break;
                }
            }
            // Configure Parity, Stop bit, Data bit
            opt.c_cflag &= ~CSIZE;
            switch (data_bits) {
                case 5:
                    opt.c_cflag |= CS5;
                    break;
                case 6:
                    opt.c_cflag |= CS6;
                    break;
                case 7:
                    opt.c_cflag |= CS7;
                    break;
                case 8:
                    opt.c_cflag |= CS8;
                    break;
                default:
                    printf("Unsupported data bits\n");
                    err = -4;
            }
            switch (parity) {
                case 'n':
                case 'N':
                    opt.c_cflag &= ~PARENB;
                    opt.c_iflag &= ~INPCK;
                    break;
                case 'o':
                case 'O':
                    opt.c_cflag |= PARODD | PARENB;
                    opt.c_iflag |= INPCK;
                    break;
                case 'e':
                case 'E':
                    opt.c_cflag |= (PARENB & ~PARODD);
                    opt.c_iflag |= INPCK;
                    break;
                default:
                    printf("Unsupported parity\n");
                    err = -5;
            }
            switch (stop_bits) {
                case 1:
                    opt.c_cflag &= ~CSTOPB;
                    break;
                case 2:
                    opt.c_cflag &= CSTOPB;
                    break;
                default:
                    printf("Unsupporter stop bits\n");
                    err = -6;
            }
            if (tcsetattr(uart_fd, TCSANOW, &opt) == -1 ) {
                printf("Configure baudrate error.\r\n");
                err = -3;
            }
        }
    }
    else {
        printf("Open Uart port error.\r\n");
        err = -1;
    }
    


    return err;
}


unsigned int Com_ConfigureTimeout(unsigned char uc_time)
{
    unsigned int ui_err;
    struct termios opt;


    ui_err = 0;
    if (tcgetattr(uart_fd, &opt) == -1) {
        printf("read_Timeout/tcgetattr");
        ui_err = 1;
    } else {
        //RAW mode
        opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        opt.c_oflag &= ~OPOST;
        //Close IXON,IXOFF  0x11 0x13
        opt.c_iflag &= ~(IXANY | IXON | IXOFF);
        opt.c_iflag &= ~ (INLCR | ICRNL | IGNCR);
        //set read timeout, 1 means 100ms
        opt.c_cc[VTIME] = uc_time;
        opt.c_cc[VMIN] = 0;
        if(tcsetattr(uart_fd, TCSANOW, &opt) == -1) {
            printf("read_Timeout/tcsetattr");
            ui_err = 1;
        }
    }

    return ui_err;
}


unsigned int Com_Send(char* puc_ptr, int ui_len)
{
    unsigned int num;

    pthread_mutex_lock(&uart_lock);
    num = write(uart_fd, puc_ptr, ui_len);
    pthread_mutex_unlock(&uart_lock);

    return num;
}


unsigned int Com_Receive(char* puc_ptr, int ui_len)
{
    int num;

    pthread_mutex_lock(&uart_lock);
    num = read(uart_fd, puc_ptr, ui_len);
    pthread_mutex_unlock(&uart_lock);

    return num;
}


unsigned int Com_ClearRbuffer(void)
{
    pthread_mutex_lock(&uart_lock);
    tcflush(uart_fd, TCIFLUSH);
    pthread_mutex_unlock(&uart_lock);

    return 0;
}


unsigned int Com_Close()
{
    close(uart_fd);

    return 0;
}


unsigned int Com_uSleep(int num_of_us)
{
    usleep(num_of_us);

    return 0;
}


unsigned int Com_Sleep(int num_of_s)
{
    sleep(num_of_s);

    return 0;
}


unsigned int Com_Printf(char* msg, int mask)
{
    
    printf("%d, %s.\r\n", mask, msg);

    return 0;
}


unsigned int Com_Clock(char* ptime, int* length)
{
    struct timespec systime;
    struct tm* time_vec;


    clock_gettime(CLOCK_REALTIME, &systime);
    time_vec = localtime(&systime.tv_sec);
    *length = sprintf(ptime, "%d,%d,%d, %d:%d:%d:%dms\n",
            time_vec->tm_year + 1900, time_vec->tm_mon +1, time_vec->tm_mday,
            time_vec->tm_hour, time_vec->tm_min, time_vec->tm_sec, (int)(systime.tv_nsec / 1000000));


    return 0;
}


unsigned int Com_fileInit(char* pathname)
{
    unsigned int ui_err = 1;


    logf = fopen(pathname, "a+");
    if (logf != NULL) {
        ui_err = 0;
    }
    
    return ui_err;
}


unsigned int Com_fileWrite(char* buf, int length)
{
    fwrite(buf, sizeof(char), length, logf);
    fflush(logf);

    return 0;
}


unsigned int Com_MutexLock(void)
{
    pthread_mutex_lock(&thread_lock);

    return 0;
}


unsigned int Com_MutexUnlock(void)
{
    pthread_mutex_unlock(&thread_lock);

    return 0;
}







