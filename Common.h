///////////////////////////////////////////////////
// File name:	Common.h
//
//
//  Created on: 05/02/2016
//      Author: Mian Tang
//
//
///////////////////////////////////////////////////






unsigned int Com_Init(const char* dev_path, int baudrate, int data_bits, int stop_bits, char parity);
unsigned int Com_ConfigureTimeout(unsigned char uc_time);
unsigned int Com_Send(char* puc_ptr, int ui_len);
unsigned int Com_Receive(char* puc_ptr, int ui_len);
unsigned int Com_ClearRbuffer(void);
unsigned int Com_Close();

unsigned int Com_Sleep(int num_of_s);
unsigned int Com_uSleep(int num_of_us);
unsigned int Com_Printf(char* msg, int mask);
unsigned int Com_Clock(char* ptime, int* length);

unsigned int Com_fileInit(char* pathname);
unsigned int Com_fileWrite(char* buf, int length);

unsigned int Com_MutexLock(void);
unsigned int Com_MutexUnlock(void);

