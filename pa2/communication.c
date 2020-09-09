
#include "communication.h"
#include "log2pa.h"
#include "pa2345.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>


int set_nonblock(int pipe_id);

int* pipes_init(size_t proc_count){
	

	int* pipes = malloc(sizeof(int) * proc_count * (proc_count-1)*2);
	size_t i, j;
	size_t offset = proc_count - 1;
	for (i = 0; i < proc_count; i++){
		for (j = 0; j < proc_count; j++){
			int tmp_fd[2];
			
			if (i == j){
				continue;
			}
			
			if (pipe(tmp_fd) < 0){
				return (int*)NULL;
			}
			
			if (set_nonblock(tmp_fd[0]) || set_nonblock(tmp_fd[1])){
				return (int*)NULL;
			}
			pipes[i * offset * 2 + (i > j ? j : j - 1) * 2 + PIPE_READ_TYPE] = tmp_fd[0];  
			pipes[j * offset * 2 + (j > i ? i : i - 1) * 2 + PIPE_WRITE_TYPE] = tmp_fd[1]; 
		}
	}
	return pipes;
}


PipesCommunication* communication_init(int* pipes, size_t proc_count, local_id curr_proc, balance_t balance){
	PipesCommunication* this = malloc(sizeof(PipesCommunication));;
	size_t i, j;
	size_t offset = proc_count - 1;
	this->pipes = malloc(sizeof(int) * offset * 2);
	this->total_ids = proc_count;
	this->current_id = curr_proc;
	this->balance = balance;
	
	memcpy(this->pipes, pipes + curr_proc * 2 * offset, sizeof(int) * offset * 2);
	
	/* Close unnecessary fds */
	for (i = 0; i < proc_count; i++){
		if (i == curr_proc){
			continue;
		}
		for (j = 0; j < proc_count; j++){
			close(pipes[i * offset * 2 + (i > j ? j : j - 1) * 2 + PIPE_READ_TYPE]);
			close(pipes[i * offset * 2 + (i > j ? j : j - 1) * 2 + PIPE_WRITE_TYPE]);
		}
	}
	free(pipes);
	return this;
}


void communication_destroy(PipesCommunication* comm){
	size_t i;
	for (i = 0; i < comm->total_ids - 1; i++){
		close(comm->pipes[i * 2 + PIPE_READ_TYPE]);
		close(comm->pipes[i * 2 + PIPE_WRITE_TYPE]);
	}
	free(comm);
}


int send_all_proc_event_msg(PipesCommunication* comm, MessageType type){
	Message message;
	uint16_t length = 0;
	char buf[MAX_PAYLOAD_LEN];
	
	message.s_header.s_magic = MESSAGE_MAGIC;
    	message.s_header.s_type = type;
    	message.s_header.s_local_time = get_physical_time();
	
	switch (type){
        case STARTED:
			length = snprintf(buf, MAX_PAYLOAD_LEN, log_started_fmt, get_physical_time(), comm->current_id, getpid(), getppid(), comm->balance);
			break;
		case DONE:
			length = snprintf(buf, MAX_PAYLOAD_LEN, log_done_fmt, get_physical_time(), comm->current_id, comm->balance);
			break;
		default:
			return -1;
	}
		
	if (length <= 0){
		return -2;
	}
	
	message.s_header.s_payload_len = length;
    memcpy(message.s_payload, buf, sizeof(char) * length);
	
	send_multicast(comm, &message);
	
	type == STARTED ? log_started(comm->current_id, comm->balance) : log_done(comm->current_id, comm->balance);
	
	return 0;
}

 /* send STOP message to processes*/
void send_all_stop_msg(PipesCommunication* comm){
	Message message;
	message.s_header.s_magic = MESSAGE_MAGIC;
    	message.s_header.s_type = STOP;
   	message.s_header.s_local_time = get_physical_time();
	message.s_header.s_payload_len = 0;
	
	send_multicast(comm, &message);
}


 
void send_transfer_msg(PipesCommunication* comm, local_id dst, TransferOrder* order){
	Message message;
	message.s_header.s_magic = MESSAGE_MAGIC;
    	message.s_header.s_type = TRANSFER;
    	message.s_header.s_local_time = get_physical_time();
	message.s_header.s_payload_len = sizeof(TransferOrder);
	
	memcpy(message.s_payload, order, message.s_header.s_payload_len);
	
	while (send(comm, dst, &message) < 0);
}

/** Send ACK message */
void send_ack_msg(PipesCommunication* comm, local_id dst){
	Message message;
	message.s_header.s_magic = MESSAGE_MAGIC;
    	message.s_header.s_type = ACK;
   	message.s_header.s_local_time = get_physical_time();
	message.s_header.s_payload_len = 0;
	
	while (send(comm, dst, &message) < 0);
}


void send_balance_history(PipesCommunication* comm, local_id dst, BalanceHistory* history){
	Message message;
	message.s_header.s_magic = MESSAGE_MAGIC;
   	message.s_header.s_type = BALANCE_HISTORY;
    	message.s_header.s_local_time = get_physical_time();
	message.s_header.s_payload_len = sizeof(BalanceHistory);
	
	memcpy(message.s_payload, history, message.s_header.s_payload_len);
	
	while (send(comm, dst, &message) < 0);
}


void receive_all_msgs(PipesCommunication* comm, MessageType type){
	Message message;
	local_id i;
	
	for (i = 1; i < comm->total_ids; i++){
		if (i == comm->current_id){
			continue;
		}
		while (receive(comm, i, &message) < 0);
	}
	
	switch (type){
        case STARTED:
            log_received_all_started(comm->current_id);
            break;
        case DONE:
            log_received_all_done(comm->current_id);
            break;
		default:
			break;
    }
}


int set_nonblock(int pipe_id){
	int ff = fcntl(pipe_id, F_GETFL);
    if (ff == -1){
        return -1;
    }
    ff = fcntl(pipe_id, F_SETFL, ff | O_NONBLOCK);
    if (ff == -1){
        return -2;
    }
    return 0;
}

