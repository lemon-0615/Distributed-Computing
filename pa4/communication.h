
#ifndef __IFMO_DISTRIBUTED_CLASS_COMMUNICATION__H
#define __IFMO_DISTRIBUTED_CLASS_COMMUNICATION__H

#include "ipc.h"

typedef struct{
	int* pipes;
	size_t total_ids;
	local_id current_id;
	local_id last_msg_from;
} PipesCommunication;

enum PipeTypeOffset 
{
    PIPE_READ_TYPE = 0,
    PIPE_WRITE_TYPE
};

int* pipes_init(size_t proc_count);
PipesCommunication* communication_init(int* pipes, size_t proc_count, local_id curr_proc);
void communication_destroy(PipesCommunication* pipes_comm);

int send_all_proc_event_msg(PipesCommunication* pipes_comm, MessageType type);
void send_all_request_msg(PipesCommunication* pipes_comm);
void send_all_release_msg(PipesCommunication* pipes_comm);
void send_reply_msg(PipesCommunication* pipes_comm, local_id dst);

void receive_all_msgs(PipesCommunication* pipes_comm, MessageType type);

#endif
