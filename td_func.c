//////////////////////////////////////////////////////////////////////////////
//
// Description:		This is used to send data to the modem. The function is
//                      same as td_send.m file which is in Matlab project.
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



////////////////////////////////////////////////////////////////////////////
//
// Extern Global Variables
//
///////////////////////////////////////////////////////////////////////////







static int total_received = 0;






///////////////////////////////////////////////////////////////////////////
//
// Function Name:	td_send
//
// Description:		It's used to send data to the modem
//               
// Parameter:
//        INPUT:
//                      tx_ptr:		Buffer data for sending
//       OUTPUT:
//                      None
//
// Return Value:
//                      0:		OK
//			else:		Error
//
//////////////////////////////////////////////////////////////////////////
unsigned int td_send(struct modemPacket* tx_ptr)
{
    unsigned int ui_err = 0;
    unsigned int i, total_len;
    char tx_bytes[128];
    char* cur_ptr;


    memset(tx_bytes, 0, sizeof(tx_bytes));
    tx_bytes[0] = '@';
    tx_bytes[1] = tx_ptr->xid;
    tx_bytes[2] = tx_ptr->type;
    tx_bytes[3] = tx_ptr->numParam;
    cur_ptr = &tx_bytes[4];
    for (i = 0; i < tx_ptr->numParam; i ++) {
        *cur_ptr ++ = tx_ptr->subsys[i];
        *cur_ptr ++ = tx_ptr->field[i];
        if (tx_ptr->type != 'g') {
            *cur_ptr ++ = (char)((tx_ptr->len[i]) >> 8);
            *cur_ptr ++ = (char)(tx_ptr->len[i]);
            memcpy(cur_ptr, tx_ptr->message[i], tx_ptr->len[i]);
            cur_ptr += tx_ptr->len[i];
        }
        else {
            cur_ptr += 2;
        }
    }
    total_len = cur_ptr - tx_bytes;
    //For debug
    printf("Send data to Modem: ");
    for (i = 0; i < total_len; i ++) {
        printf("0x%x, ", tx_bytes[i]);
    }
    printf("\r\n");

    // Send command over Uart
    Com_Send(tx_bytes, total_len);


    return ui_err;
}



///////////////////////////////////////////////////////////////////////////
//
// Function Name:       td_notify
//
// Description:         It's used to receive data from the modem
//               
// Parameter:
//        INPUT:
//			wait_interval:	wait interval for the first byte
//                                      received. The number of 1ms.
//                      num_of_wait:	The number of wait_interval * 1ms for 
//					the data from Modem
//       OUTPUT:
//                      rx_ptr:         Received data from modem
//
// Return Value:
//                      0:              OK
//                      else:           Error
//
//////////////////////////////////////////////////////////////////////////
unsigned int td_notify(struct modemPacket* rx_ptr, int wait_interval, int num_of_wait)
{
    unsigned int ui_err = 0;
    int i, j, loop, total_len;
    char msglen[2];
    char* cur_ptr;

    total_len = loop = 0;
    // First, try to receive '@' symbol
    do {
        Com_uSleep(wait_interval * 1000);
        Com_Receive(&(rx_ptr->header), 1);
        //if (num != 0) {
        //    printf("Number is %dBytes, Header is 0x%x\r\n", num, rx_ptr->header);
        //}
    } while ((rx_ptr->header != '@') && (++ loop <= num_of_wait));
    // Then, receive the rest
    if (rx_ptr->header == '@') {
        // Wait for another 6ms to receive 3bytes from modem (Baudrate is 9600)
        Com_uSleep(6 * 1000);
        Com_Receive(&(rx_ptr->xid), 1);
        Com_Receive(&(rx_ptr->type), 1);
        Com_Receive(&(rx_ptr->numParam), 1);
        total_len += 4;
        for (i = 0; i < rx_ptr->numParam; i ++) {
            Com_uSleep(40 * 1000);
            Com_Receive(&(rx_ptr->subsys[i]), 1);
            Com_Receive(&(rx_ptr->field[i]), 1);
            Com_Receive(msglen, 2);
            rx_ptr->len[i] = ((int)msglen[0] << 8) + (int)msglen[1];
            total_len += 4;
            if (rx_ptr->len[i] != 0) {
                Com_Receive(rx_ptr->message[i], rx_ptr->len[i]);
                total_len += rx_ptr->len[i];
            }
            else {
                memset(rx_ptr->message[i], 0, MESSAGE_LEN);
            }
        }
    }
    else {
        rx_ptr->header = 0x0;
        ui_err = 1;
    }
    if (total_len != 0) {
        //For debug
        cur_ptr = (char*)rx_ptr;
        printf("Data received from Modem: total len is %dBytes.", total_len);
        for (i = 0; i < 4; i ++) {
            printf("0x%x, ", cur_ptr[i]);
        }
        for (i = 0; i < rx_ptr->numParam; i ++) {
            printf("0x%x, ", rx_ptr->subsys[i]);
            printf("0x%x, ", rx_ptr->field[i]);
            printf("0x%x, 0x%x, ", (char)(rx_ptr->len[i] >> 8), (char)(rx_ptr->len[i]));
            for (j = 0; j < rx_ptr->len[i]; j ++) {
                printf("0x%x, ", rx_ptr->message[i][j]);
            }
        }
        printf("\r\n");
        total_received += total_len;
        //printf("Total Received: %d Bytes.\r\n", total_received);
    }


    return ui_err;
}



///////////////////////////////////////////////////////////////////////////
//
// Function Name:       td_get
//
// Description:         It's used to read the parameters from the Modem
//               
// Parameter:
//        INPUT:
//                      get_request:	Request field for Modem
//                      ID:		XID for Modem
//       OUTPUT:
//                      NONE
//
// Return Value:
//                      0:              OK
//                      else:           Error
//
//////////////////////////////////////////////////////////////////////////
unsigned int td_get(char* get_request, char ID)
{
    unsigned int ui_err = 0;
    struct modemPacket tx_pkt;


    memset(&tx_pkt, 0, sizeof(struct modemPacket));
    tx_pkt.xid = ID;
    tx_pkt.type = 'g';
    if (strcmp(get_request, "localAddr") == 0) {
        printf("td_get, Command: localAddr.\r\n");
        tx_pkt.numParam = 1;
        tx_pkt.subsys[0] = 1;
        tx_pkt.field[0] = 18;
        tx_pkt.len[0] = 0;
    }
   else {
        printf("td_get, Command: It's unknown.......\r\n");
        ui_err = 1;
    }
    // Send command
    if (ui_err == 0) {
        ui_err = td_send(&tx_pkt);
    }

    return ui_err;
}


