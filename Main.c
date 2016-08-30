///////////////////////////////////////////////////
// File name:	main.c
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
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "Common.h"
#include "tda_func.h"



#define CMDHEADER_LEN		8
#define CMD_HEADER		0xFFFF0000
#define CRC_LEN			4
#define MAX_PAYLOAD		256


#define COMMON_CMD		0x01
#define DOWNLOAD_CMD		0x02
#define UPLOAD_CMD		0x03
#define ACK_CMD			0x04
#define MONITOR_CMD		0x05


#define CRC_ERROR		0x02
#define FILE_ERROR		0x03


#define TIMESYNC_CMD		0x11
#define RUNSTOP_CMD		0x12
#define MODEMADDR_CMD		0x13


#define EXECUTIVE_FL		0x01
#define MODEM_FL		0x02


#define PACKETSIZE		116
#define MAXFILESIZE_1024K	1024 * 1024

#define ELFHEADER_LEN		24
#define MONITOR_LENGTH		100


#define EXEC_ORIGINAL		"/root/macComm"
#define EXEC_FILE		"/root/macComm_bak"
#define MODEM_ORIGINAL		"/root/modem"
#define MODEM_FILE		"/root/modem_bak"

#define PIPE_FILE		"/tmp/macFifo"



/////////////////////////////////////////////////////////////////////////////////////
// the local global variable
/////////////////////////////////////////////////////////////////////////////////////
static pid_t pid, wait_pid = -1;

static char modem_addr = 0x00;

static int fifo_fd;








/////////////////////////////////////////////////////////////////////////////////////
// the extern variable
/////////////////////////////////////////////////////////////////////////////////////




unsigned long CRC32(unsigned char *uc_base, unsigned long ul_size);



static unsigned int Loader_CheckELF(char* filepath)
{
    unsigned int err;
    char buffer[ELFHEADER_LEN];
    int fd;

    err = 1;
    fd =open(filepath, O_RDONLY);
    if (fd >= 0) {
        if (read(fd, buffer, ELFHEADER_LEN) == ELFHEADER_LEN) {
            // Check Header
            if ((buffer[0] == 0x7F) && (buffer[1] == 'E') && (buffer[2] == 'L') && (buffer[3] == 'F')) {
                // Check ARM processor
                if (buffer[18] == 0x28) {
                    err = 0;
                }
            }
        }
    }
    close(fd);

    return err;
}




static unsigned int Loader_TimeSync(char* time_payload)
{
    int err;
    time_t ptime;
    struct tm time;
    struct timeval stime;
    struct timezone tz;


    printf("In Loader_timesync function.....\r\n");
    time.tm_mon = (int)time_payload[0] - 1;
    time.tm_mday = (int)time_payload[1];
    time.tm_year = ((int)time_payload[2] << 8) + (int)time_payload[3] - 1900;
    time.tm_hour = (int)time_payload[4];
    time.tm_min = (int)time_payload[5];
    time.tm_sec = (int)time_payload[6];
    ptime = mktime(&time);
    gettimeofday(&stime, &tz);
    stime.tv_sec = ptime;
    stime.tv_usec = (((int)time_payload[8] << 8) + (int)time_payload[9]) * 1000;
    err = settimeofday(&stime, &tz);


    return err;
}


static unsigned int Loader_modemAddr(void)
{
    // Do nothing...
    printf("In Loader_modemAddr function.....\r\n");


    return 0;
}


static unsigned int Loader_Monitor(char* monitor_payload, char* monitorData)
{
    int crc_Cal;
    char* ptr;
    char wbuffer[8 + MONITOR_LENGTH + 4];

    printf("In Loader_Monitor function...\r\n");
    ptr = monitor_payload;
    if (ptr[0] == 0x00) {
        memset(wbuffer, 0, sizeof(wbuffer));
        wbuffer[0] = wbuffer[1] = 0xFF;
        wbuffer[2] = wbuffer[3] = 0x00;
        wbuffer[4] = 0x00;
        wbuffer[5] = ACK_CMD;
        wbuffer[6] = (char)(MONITOR_LENGTH >> 8);
        wbuffer[7] = (char)MONITOR_LENGTH;
        memcpy((wbuffer + 8), monitorData, MONITOR_LENGTH);
        crc_Cal = CRC32((unsigned char*)(wbuffer + 8), MONITOR_LENGTH);
        wbuffer[8 + MONITOR_LENGTH] = (char)(crc_Cal >> 24);
        wbuffer[8 + MONITOR_LENGTH + 1] = (char)(crc_Cal >> 16);
        wbuffer[8 + MONITOR_LENGTH + 2] = (char)(crc_Cal >> 8);
        wbuffer[8 + MONITOR_LENGTH + 3] = (char)crc_Cal;
        // Send monitor data back...
        Com_Send(wbuffer, (8 + MONITOR_LENGTH + 4));
    }


    return 0;
}


static unsigned int Loader_Exec(char* exec_payload)
{
    int flag, temp, status, err;
    float pktrate;
    char* ptr;
    char macProtocol[2], xid[4], masterAddr[4], packetrate[8];


    err = 0;
    ptr = exec_payload;
    flag = (int)ptr[0];
    if (flag == 1) {
        // Run the application
        memset(xid, 0, sizeof(xid));
        memset(masterAddr, 0, sizeof(masterAddr));
        memset(packetrate, 0, sizeof(packetrate));
        memset(macProtocol, 0, sizeof(macProtocol));
        macProtocol[0] = ptr[1] + 0x30;
        sprintf(xid, "%d", ptr[2]);
        sprintf(masterAddr, "%d", ptr[3]);
        temp = ((int)ptr[4] << 24) + ((int)ptr[5] << 16) + ((int)ptr[6] << 8) + (int)ptr[7];
        pktrate = ((float)temp / 10000);
        sprintf(packetrate, "%.4f", pktrate);
        
        // Check the executive file
        if (Loader_CheckELF(EXEC_ORIGINAL) == 0) {
            if (wait_pid == -1) {
                printf("Run...Parameters: %s, %s, %s, %s.\r\n", macProtocol, xid, masterAddr, packetrate);
                if ((pid = fork()) < 0) {
                    err = 1;
                    printf("Create new progress failed.\r\n");
                }
                else if (pid == 0) {
                    // Child progress, run as default when xid = 0x0 and mastAddr = 0x0
                    if ((ptr[2] == 0x0) && (ptr[3] == 0x0)) {
                        execl("/root/macComm", "macComm", NULL);
                    }
                    else {
                        execl("/root/macComm", " ", "-protocol", macProtocol, "-xid", xid, "-master", masterAddr, "-packetrate", packetrate, NULL);
                    }
                }
            }   
            else {
                printf("The program has been run......\r\n");
            }
        }
        else {
            printf("Not executive File.\r\n");
            err = 1;
        }
    }
    else {
        // Stop the application
        printf("Stop....\r\n");
        if (pid >= 0) {
            kill(pid, SIGKILL);
            wait_pid = waitpid(pid, &status, 0);
            if (wait_pid >= 0) {
                printf("The application quit correctly.\r\n");
                wait_pid = -1;
            }
        }
    }



    return err;
}


static unsigned int Loader_Run(char* payload, int payload_len)
{
    int subCmd;
    char* ptr;
    unsigned int err_code;

    printf("In Loader_Run function.....\r\n");
    ptr = payload;
    subCmd = ((int)ptr[0] << 8) + (int)ptr[1];
    switch (subCmd) {
        case TIMESYNC_CMD:
            err_code = Loader_TimeSync(payload + 4);
            break;
        case RUNSTOP_CMD:
            err_code = Loader_Exec(payload + 4);
            break;
        case MODEMADDR_CMD:
            err_code = Loader_modemAddr();
            break;
        default:
            err_code = 1;
            break;
    }


    return err_code;
}




static unsigned int Loader_Reply(char* Msg, int subCmd, int error_code)
{
    char* pMsg;
    unsigned int crc_Cal;

    pMsg = Msg;
    // Reply ACK with CRC error
    pMsg[0] = pMsg[1] = 0xFF;
    pMsg[2] = pMsg[3] = 0x00;
    pMsg[4] = 0x00;
    pMsg[5] = ACK_CMD;
    pMsg[6] = 0x00;
    pMsg[7] = 0x08;
    pMsg[8] = 0X00;
    pMsg[9] = 0X00;
    pMsg[10] = (char)(subCmd >> 8);
    pMsg[11] = (char)subCmd;
    switch (subCmd) {
        case MODEMADDR_CMD:
            pMsg[12] = modem_addr;
            pMsg[13] = 0x0;
            break;
        default:
            pMsg[12] = 0x0;
            pMsg[13] = 0x0;
            break;
    }
    pMsg[14] = (char)(error_code >> 8);
    pMsg[15] = (char)error_code;
    crc_Cal = CRC32((unsigned char*)(pMsg + 8), 8);
    pMsg[16] = (char)(crc_Cal >> 24);
    pMsg[17] = (char)(crc_Cal >> 16);
    pMsg[18] = (char)(crc_Cal >> 8);
    pMsg[19] = (char)crc_Cal;
 

    return 0;
}


static unsigned int Loader_Download(int filetype, int filesize)
{
    int fd, size_cnt, rnum, flags, i_err;
    int loop_cnt, sub_cnt, pkt_len;
    unsigned int crc_Rcv, crc_Cal;
    char rbuffer[128], wbuffer[20];

    fd = i_err = -1;
    printf("The filetype is %d, and file size is %dBytes.\r\n", filetype, filesize);
    if ((filetype == EXECUTIVE_FL) && (filesize < MAXFILESIZE_1024K)) {
        // Check the existence of the file and delete it before downloading
        if (access(EXEC_FILE, F_OK) != -1) {
            i_err = remove(EXEC_FILE);
            if (i_err == 0) {
                fd = open(EXEC_FILE, (O_CREAT | O_RDWR | O_TRUNC), 00775);
            }
        }
        else {
            fd = open(EXEC_FILE, (O_CREAT | O_RDWR | O_TRUNC), 00775);
        }
    }
    else if ((filetype == MODEM_FL) && (filesize < MAXFILESIZE_1024K)) {
        // Check the existence of the file and delete it before downloading
        if (access(MODEM_FILE, F_OK) != -1) {
            i_err = remove(MODEM_FILE);
            if (i_err == 0) {
                fd = open(MODEM_FILE, (O_CREAT | O_RDWR | O_TRUNC), 00775);
            }
        }
        else {
            fd = open(MODEM_FILE, (O_CREAT | O_RDWR | O_TRUNC), 00775);
        }
    }

    if (fd >= 0) {
        size_cnt = filesize / PACKETSIZE;
        memset(wbuffer, 0, sizeof(wbuffer));
        Loader_Reply(wbuffer, 0xFFFF, 0);
        Com_Send(wbuffer, 20);
        flags = 1;
        loop_cnt = 0;
        // Receive data and write to file
        do {
            memset(rbuffer, 0, sizeof(rbuffer));
            // First of all ,try to read the first byte
            sub_cnt = rnum = 0;
            do {
                Com_uSleep(5 * 1000);
                rnum = Com_Receive(rbuffer, 1);
            } while ((rnum == 0) && (++ sub_cnt < 200));
            // Read the header and payload
            if (rnum != 0) {
                Com_uSleep(5 * 1000);
                Com_Receive((rbuffer + 1), 7);
                pkt_len = ((int)rbuffer[6] << 8) + (int)rbuffer[7];
                Com_uSleep(30 * 1000);
                rnum = Com_Receive((rbuffer + 8), pkt_len + 4);
                printf("Packet data received: %dBytes.\r\n", rnum);
                crc_Rcv = ((int)rbuffer[8 + pkt_len] << 24) + ((int)rbuffer[8 + pkt_len + 1] << 16) + ((int)rbuffer[8 + pkt_len + 2] << 8) + (int)rbuffer[8 + pkt_len + 3];
                crc_Cal = CRC32((unsigned char*)(rbuffer + 8), pkt_len);
                printf("The packet length is %d, CRC received is 0x%x, CRC calculation is 0x%x.\r\n", pkt_len, crc_Rcv, crc_Cal);
                // Check CRC
                if (crc_Cal == crc_Rcv) {
                    // Write data to file
                    if (write(fd, (rbuffer + 8), pkt_len) == pkt_len) {
                        // Replay ACK...
                        if (loop_cnt < size_cnt) {
                            Loader_Reply(wbuffer, 0xFFFF, 0);
                            printf("Reply ACK for data transmission.\r\n");
                        }
                        else {
                            printf("The last packet received: %dBytes.\r\n", pkt_len);
                            close(fd);
                            remove(EXEC_ORIGINAL);
                            if (rename(EXEC_FILE, EXEC_ORIGINAL) == 0) {
                                Loader_Reply(wbuffer, 0xFFF0, 0);
                                printf("Data downloaded successfully.\r\n");
                            }
                            else {
                                Loader_Reply(wbuffer, 0xFFF0, FILE_ERROR);
                                flags = 0;
                            }
                        }
                        Com_Send(wbuffer, 20);
                    }
                    else {
                        flags = 0;
                        Loader_Reply(wbuffer, 0xFFFF, FILE_ERROR);
                        Com_Send(wbuffer, 20);
                    }   
                }
                else {
                    flags = 0;
                    Loader_Reply(wbuffer, 0xFFFF, CRC_ERROR);
                    Com_Send(wbuffer, 20);
                }
            }
            else {
                flags = 0;
            }
        } while ((flags == 1) && (++loop_cnt < (size_cnt + 1)));
    }
    else {
        Loader_Reply(wbuffer, 0xFFFF, FILE_ERROR);
        Com_Send(wbuffer, 20);
    }
    i_err = (flags == 1) ? 0 : 1;


    return i_err;
}




/////////////////////////////////////////////////////////////////////////////////////
// Function Name:			main
// Function Description:	main function
// Function Parameters:
//			INPUT:			None
//			OUTPUT:			None
// Return Value:
//						None
/////////////////////////////////////////////////////////////////////////////////////
int main(void)
{
    int header, type, len, subCmd, exe_err;
    int ftype, fsize, readCnt;
    unsigned int crc_Rcv, crc_Cal;
    char rbuffer[128], wbuffer[20], pipeBuf[MONITOR_LENGTH];
    char payload[MAX_PAYLOAD + CRC_LEN];
    char* uart_dev = "/dev/ttyO1";



    // Modem Diagnostics
    modem_addr = 0x0;
    tda_modemInit();
    tda_getLocalAddr(0x31, &modem_addr);
    Com_Close();
    printf("The modem addr is 0x%x.\r\n", modem_addr);


    // Create FIFO special file
    readCnt = 0;
    memset(pipeBuf, 0, sizeof(pipeBuf));
    unlink(PIPE_FILE);
    if (mkfifo(PIPE_FILE, 0777) == 0) {
        fifo_fd = open(PIPE_FILE, O_RDONLY | O_NONBLOCK);
        if (fifo_fd > 0) {
            printf("Pipe created OK.\r\n");
        }
    }


    if (Com_Init(uart_dev, 115200, 8, 1, 'N') == 0) {
        Com_ConfigureTimeout(0);
        wait_pid = -1;
        do {
            memset(rbuffer, 0, sizeof(rbuffer));
            if (Com_Receive(rbuffer, 1) == 1) {
                Com_uSleep(5 * 1000);
                Com_Receive((rbuffer + 1), 7);
                // Check header
                header = ((int)rbuffer[0] << 24) + ((int)rbuffer[1] << 16) + ((int)rbuffer[2] << 8) + (int)rbuffer[3];
                // For XBee data received, we just check the header of frame that we defined
                if (header == (int)CMD_HEADER) {
                    type = ((int)rbuffer[4] << 8) + (int)rbuffer[5];
                    len = ((int)rbuffer[6] << 8) + (int)rbuffer[7];
                    if (Com_Receive(payload, len + CRC_LEN) == (unsigned int)(len + CRC_LEN)) {
                        crc_Rcv = ((int)payload[len] << 24) + ((int)payload[len + 1] << 16) + ((int)payload[len + 2] << 8) + (int)payload[len + 3];
                        crc_Cal = CRC32((unsigned char*)payload, len);
                        if (crc_Cal == crc_Rcv) {
                            switch (type) {
                                case COMMON_CMD:
                                    printf("Received command: 0x%x, 0x%x, 0x%x, 0x%x.\r\n", type, payload[0], payload[1], payload[2]);
                                    exe_err = Loader_Run(payload, len);
                                    // Reply ACK...
                                    subCmd = ((int)payload[0] << 8) + (int)payload[1];
                                    Loader_Reply(wbuffer, subCmd, exe_err);
                                    Com_Send(wbuffer, 20);
                                    break;
                                case DOWNLOAD_CMD:
                                    printf("Download command received.\r\n");
                                    ftype = ((int)payload[0] << 8) + (int)payload[1];
                                    fsize = ((int)payload[4] << 24) + ((int)payload[5] << 16) + ((int)payload[6] << 8) + (int)payload[7];
                                    exe_err = Loader_Download(ftype, fsize);
                                    break;
                                case UPLOAD_CMD:
                                    break;
                                case MONITOR_CMD:
                                    printf("Monitor command received.\r\n");
                                    exe_err = Loader_Monitor(payload, pipeBuf);
                                    break;
                                default:
                                    break;
                            }
                        }
                        else {
                            // Reply ACK with CRC error
                            Loader_Reply(wbuffer, 0, CRC_ERROR);
                            Com_Send(wbuffer, 20);
                        }
                    }
                    else {
                        Com_uSleep(500 * 1000);
                        Com_ClearRbuffer();
                    }
                }
            }

            // Read data through Pipe every 1 second
            ++ readCnt;
            if (readCnt == 100) {
                read(fifo_fd, pipeBuf, MONITOR_LENGTH);
                //printf("The read number is %dBytes. Data is 0x%x, 0x%x, 0x%x, 0x%x...\r\n", readNum, pipeBuf[0], pipeBuf[1], pipeBuf[10], pipeBuf[11]);
                readCnt = 0;
            }

            // Sleep 10ms
            Com_uSleep(10 * 1000);
        } while (1);
    }
    else {
        printf("Open Uart device error.\r\n");
    }


    return 0;
}
