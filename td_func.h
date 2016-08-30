


#ifndef TD_FUNC_H_
#define TD_FUNC_H_


#define NUM_OF_MESSAGE          4
#define MESSAGE_LEN             64

struct modemPacket {
    char header;
    char xid;
    char type;
    char numParam;
    char subsys[NUM_OF_MESSAGE];
    char field[NUM_OF_MESSAGE];
    short len[NUM_OF_MESSAGE];
    char message[NUM_OF_MESSAGE][MESSAGE_LEN];
};



unsigned int td_send(struct modemPacket* tx_ptr);
unsigned int td_notify(struct modemPacket* rx_ptr, int wait_interval, int num_of_wait);
unsigned int td_get(char* get_request, char ID);


















#endif


