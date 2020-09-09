
#include "banking.h"
#include "communication.h"
#include "log2pa.h"

void transfer(void * parent_data, local_id src, local_id dst, balance_t amount){
	Message message;
	
	TransferOrder transferorder;
	transferorder.s_amount = amount;
	transferorder.s_src = src;
        transferorder.s_dst = dst;
        

        PipesCommunication* parent = (PipesCommunication*) parent_data;
	
    	send_transfer_msg(parent, src, &transferorder);
	
	log_transfer_out(src, dst, amount);
		
    while (receive(parent, dst, &message) < 0 || message.s_header.s_type != ACK);
	
	log_transfer_in(src, dst, amount);		
}
