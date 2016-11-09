//////////////////////////////////////////////////////////////////////////////
//
// Description:		This file includes all functions for TDA layer.
//                      It's ported from Matlab project.
//
// Version:		V1.0
//
// Author:		Mian Tang
//
// Date:		05/10/2016
//
// Comment:
//
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "td_func.h"
#include "Common.h"




#define INVALID_ADDRESS			0x0






////////////////////////////////////////////////////////////////////////////
//
// Extern Global Variables
//
///////////////////////////////////////////////////////////////////////////




unsigned int tda_getLocalAddr(char xid, char* local_addr);









/////////////////////////////////////////////////////////////////////////
//
// Function Name:	tda_modemInit
//
// Description:		It's used to find and initialize modem to enter
//                      mmpd mode.
//
// Parameter:
//        Input:
//			NONE
//        Output:
//			NONE
//
// Return Value:
//			0:		OK
//			else:		Error
//
////////////////////////////////////////////////////////////////////////
unsigned int tda_modemInit(void)
{
    const char uart_dev[] = "/dev/ttyO2";
    char init_cmd[] = {0x2B, 0x2B, 0x2B, 0x0A};
    char mmpd_cmd[] = {0x6D, 0x6D, 0x70, 0x64, 0x0A};
    char mmpd_res[] = {0x40, 0x00, 0x6E};
    char rbuffer[128];
    int num, loop, detected;
    unsigned int ui_err = 0;


    memset(rbuffer, 0, sizeof(rbuffer));
    detected = loop = 0;
    // Uart 2
    if (Com_Init(uart_dev, 115200, 8, 1, 'N') == 0) {
        Com_ConfigureTimeout(0);

            // Read log data if it outputs...
            do {
                Com_uSleep(200 * 1000);
                num = Com_Receive(rbuffer, sizeof(rbuffer));
                printf("Read Log: %d, %s.\r\n", num, rbuffer);
            } while (num > 0);
            // Send "+++" command and get response
            do {
                Com_Send(init_cmd, sizeof(init_cmd));
                Com_uSleep(200 * 1000);
                num = Com_Receive(rbuffer, sizeof(rbuffer));
                if (strstr(rbuffer, "user:") != NULL) {
                    detected = 1;
                    printf("Modem has been detected.\r\n");
                }
                else {
                    Com_ClearRbuffer();
                    if (tda_getLocalAddr(10, rbuffer) == 0) {
                        detected = 1;
                        printf("Modem has been  detected.\r\n");
                    }
                    else {
                        printf("No Response found from Modem: %d, %s.\r\n", num, rbuffer);
                        Com_uSleep(500 * 1000);
                    }
                }
            } while ((++ loop <= 3) && (detected == 0));
    }
    else {
        printf("Cannot configure Uart correctly.\r\n");
        ui_err = 1;
    }

    if (detected == 1) {
        // Enter mmpd mode
        memset(rbuffer, 0, sizeof(rbuffer));
        Com_Send(mmpd_cmd, sizeof(mmpd_cmd));
        Com_uSleep(200 * 1000);
        num = Com_Receive(rbuffer, sizeof(rbuffer));
        if ((rbuffer[7] == mmpd_res[0]) && (rbuffer[8] == mmpd_res[1]) && (rbuffer[9] == mmpd_res[2])) {
            printf("Entered MMPD mode.....\r\n");
        }
        else {
            printf("Cannot enter MMPD mode, or has been in MMPD mode already:\r\n");
            for (loop = 0; loop < num; loop ++) {
                printf("0x%x, ", rbuffer[loop]);
            }
            printf("\r\n");
            ui_err = 3;
        }
    }
    else {
        printf("No response from Modem.\r\n");
        ui_err = 2;
    }

    return ui_err;
}



/////////////////////////////////////////////////////////////////////////
//
// Function Name:	tda_getLocalAddr
//
// Description:		It's used to get the local address from Modem
//
// Parameter:
//        Input:
//			xid:		XID field for Modem
//        Output:
//			local_addr:	the local address received from
//                                      the modem
//
// Return Value:
//			0:		OK
//			else:		Error
//
////////////////////////////////////////////////////////////////////////
unsigned int tda_getLocalAddr(char xid, char* local_addr)
{
    unsigned int i, ui_err = 0;
    struct modemPacket mpkt;


    *local_addr = INVALID_ADDRESS;
    // Clear Uart input buffer
    //Com_ClearRbuffer();
    if (td_get("localAddr", xid) == 0) {
        Com_uSleep(50 * 1000);
        if (td_notify(&mpkt, 5, 100) == 0) {
            for (i = 0; i < mpkt.numParam; i ++) {
                if ((mpkt.subsys[i] == 1) && (mpkt.field[i] == 18)) {
                    *local_addr = mpkt.message[i][1];
                }
            }
            if (*local_addr == INVALID_ADDRESS) {
                printf("Cannot find Local address from the message sent by the Modem.\r\n");
                ui_err = 3;
            }
        }
        else {
            printf("Cannot get notify from Modem after sending localAddr Command.\r\n");
            ui_err = 2;
        }
    }
    else {
        printf("Cannot send localAddr command to the Modem.\r\n");
        ui_err = 1;
    }


    return ui_err;
}
