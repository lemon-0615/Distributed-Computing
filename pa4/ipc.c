#include "ipc.h"
#include "communication.h"
#include <unistd.h>

#define GET_INDEX(x, id) ((x) < (id) ? (x) : (x) - 1)


int send(void * self, local_id dst, const Message * message);

int send_multicast(void * self, const Message * message){
	PipesCommunication* from = (PipesCommunication*) self;
	local_id i;
	
	for (i = 0; i < from->total_ids; i++){
		if (i == from->current_id){
			continue;
		}
		while(send(from, i, message) < 0);
	}
	return 0;
}

int receive(void * self, local_id from, Message * message){
	PipesCommunication* this = (PipesCommunication*) self;
	
	if (from == this->current_id){
		return -1;
	}
	
	if (read(this->pipes[GET_INDEX(from, this->current_id) * 2 + PIPE_READ_TYPE], message, sizeof(MessageHeader)) < (int)sizeof(MessageHeader)){
		return -2;
	}
	
	
	if (read(this->pipes[GET_INDEX(from, this->current_id) * 2 + PIPE_READ_TYPE], ((char*) message) + sizeof(MessageHeader), message->s_header.s_payload_len) < 0){
		return -3;
	}
	return 0;
}

int receive_any(void * self, Message * message){
	PipesCommunication* this = (PipesCommunication*) self;
	local_id i;
	
	for (i = 0; i < this->total_ids; i++){
		if (i == this->current_id){
			continue;
		}
		
		if (!receive(this, i, message)){
			this->last_msg_from = i;
			return 0;
		}
	}
	return -1;
}

int send(void * self, local_id dst, const Message * message){
	PipesCommunication* from = (PipesCommunication*) self;
	
	if (dst == from->current_id){
		return -1;
	}
	if (write(from->pipes[GET_INDEX(dst, from->current_id) * 2 + PIPE_WRITE_TYPE], message, sizeof(MessageHeader) + message->s_header.s_payload_len) < 0){
		return -2;
	}
	return 0;
}
