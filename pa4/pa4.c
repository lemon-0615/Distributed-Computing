#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <getopt.h>
#include <fcntl.h>
#include "log4pa.h"
#include "communication.h"
#include "lamport_time.h"
#include "cs_pa4.h"
#include "pa2345.h"
#include "common.h"



FILE* pipes_log_f;
FILE* events_log_f;

void log_init(){
	pipes_log_f = fopen(pipes_log, "w");
	events_log_f = fopen(events_log, "w");
}



void log_started(local_id id){
	printf(log_started_fmt, get_lamport_time(), id, getpid(), getppid(), 0);
    fprintf(events_log_f, log_started_fmt, get_lamport_time(), id, getpid(), getppid(), 0);
}

void log_received_all_started(local_id id){
	printf(log_received_all_started_fmt, get_lamport_time(), id);
    fprintf(events_log_f, log_received_all_started_fmt, get_lamport_time(), id);
}

void log_done(local_id id){
	printf(log_done_fmt, get_lamport_time(), id, 0);
    fprintf(events_log_f, log_done_fmt, get_lamport_time(), id, 0);
}

void log_received_all_done(local_id id){
	printf(log_received_all_done_fmt, get_lamport_time(), id);
    fprintf(events_log_f, log_received_all_done_fmt, get_lamport_time(), id);
}
void log_pipes(PipesCommunication* pipes_comm){
	size_t i;
	
	fprintf(pipes_log_f, "Process %d pipes:\n", pipes_comm->current_id);
	
	for (i = 0; i < pipes_comm->total_ids; i++){
		if (i == pipes_comm->current_id){
			continue;
		}
		
		fprintf(pipes_log_f, "P%ld|R%d|W%d ", i, pipes_comm->pipes[(i < pipes_comm->current_id ? i : i-1) * 2 + PIPE_READ_TYPE], 
			pipes_comm->pipes[(i < pipes_comm->current_id ? i : i-1) * 2 + PIPE_WRITE_TYPE]);
	}
	fprintf(pipes_log_f, "\n");
}

void log_destroy(){
	fclose(pipes_log_f);
    fclose(events_log_f);
}

int request_cs(const void * self){
	CS* lamport_comm = (CS*) self;
	PipesCommunication* comm = lamport_comm->comm;
	LamportQueue* queue = lamport_comm->queue;
	Message msg;
	size_t reply_left = comm->total_ids - 2;
	
	lamport_queue_insert(queue, get_lamport_time(), comm->current_id);
	send_all_request_msg(comm);
	
	
	while (reply_left){
		while (receive_any(comm, &msg));
		
		set_lamport_time_from_msg(&msg);
		
		cs_work(lamport_comm, &msg);
		
		if (msg.s_header.s_type == CS_REPLY){
            reply_left--;
        }
	}
	
	
	while (lamport_queue_peek(queue) != comm->current_id){
		while (receive_any(comm, &msg));
		
		set_lamport_time_from_msg(&msg);
		
		cs_work(lamport_comm, &msg);
	}
	
	return 0;
}

int release_cs(const void * self){
	CS* lamport_comm = (CS*) self;
	PipesCommunication* comm = lamport_comm->comm;
	LamportQueue* queue = lamport_comm->queue;
	
	send_all_release_msg(comm);
	lamport_queue_get(queue);
	return 0;
}

int cs_work(CS* lamport_comm, Message* msg){
	PipesCommunication* comm = lamport_comm->comm;
	LamportQueue* queue = lamport_comm->queue;
	
	if (msg->s_header.s_type == CS_REQUEST){
        lamport_queue_insert(queue, msg->s_header.s_local_time - 1, comm->last_msg_from);

        send_reply_msg(comm, comm->last_msg_from);
    }
    else if (msg->s_header.s_type == CS_RELEASE){
        if (lamport_queue_get(queue) != comm->last_msg_from){
            return -1;
        }
    }
	else if (msg->s_header.s_type == DONE){
		lamport_comm->done_left--;
	}
	return 0;
}

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



PipesCommunication* communication_init(int* pipes, size_t proc_count, local_id curr_proc){
	PipesCommunication* this = malloc(sizeof(PipesCommunication));
	size_t i, j;
	size_t offset = proc_count - 1;

	this->pipes = malloc(sizeof(int) * offset * 2);
	this->total_ids = proc_count;
	this->current_id = curr_proc;
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


void communication_destroy(PipesCommunication* pipes_comm){
	size_t i;
	for (i = 0; i < pipes_comm->total_ids - 1; i++){
		close(pipes_comm->pipes[i * 2 + PIPE_READ_TYPE]);
		close(pipes_comm->pipes[i * 2 + PIPE_WRITE_TYPE]);
	}
	free(pipes_comm);
}



void send_all_request_msg(PipesCommunication* pipes_comm){
	Message message;
	message.s_header.s_magic = MESSAGE_MAGIC;
        message.s_header.s_type = CS_REQUEST;
       //message.s_header.s_local_time = get_physical_time();
       //message.s_header.s_local_time =get_lamport_time()
   	message.s_header.s_local_time = increment_lamport_time();
	message.s_header.s_payload_len = 0;
	send_multicast(pipes_comm, &message);
}

int send_all_proc_event_msg(PipesCommunication* pipes_comm, MessageType type){
	Message message;
	uint16_t length = 0;
	char buf[MAX_PAYLOAD_LEN];
	message.s_header.s_magic = MESSAGE_MAGIC;
    	message.s_header.s_type = type;
       //message.s_header.s_local_time = get_physical_time();
   	//message.s_header.s_local_time = get_lamport_time();
   	message.s_header.s_local_time = increment_lamport_time();
	
	switch (type){
        case STARTED:
			length = snprintf(buf, MAX_PAYLOAD_LEN, log_started_fmt, get_lamport_time(), pipes_comm->current_id, getpid(), getppid(), 0);
			break;
		case DONE:
			length = snprintf(buf, MAX_PAYLOAD_LEN, log_done_fmt, get_lamport_time(), pipes_comm->current_id, 0);
			break;
		default:
			return -1;
	}
		
	if (length <= 0){
		return -2;
	}
	
	message.s_header.s_payload_len = length;
   	memcpy(message.s_payload, buf, sizeof(char) * length);
	send_multicast(pipes_comm, &message);
	
	type == STARTED ? log_started(pipes_comm->current_id) : log_done(pipes_comm->current_id);
	
	return 0;
}

void send_all_release_msg(PipesCommunication* pipes_comm){
	Message message;
	message.s_header.s_magic = MESSAGE_MAGIC;
        message.s_header.s_type = CS_RELEASE;
        message.s_header.s_local_time = increment_lamport_time();
	message.s_header.s_payload_len = 0;
	
	send_multicast(pipes_comm, &message);
}

void send_reply_msg(PipesCommunication* pipes_comm, local_id dst){
	Message message;
	message.s_header.s_magic = MESSAGE_MAGIC;
        message.s_header.s_type = CS_REPLY;
        message.s_header.s_local_time = increment_lamport_time();
	message.s_header.s_payload_len = 0;
	
	while (send(pipes_comm, dst, &message) < 0);
}

void receive_all_msgs(PipesCommunication* pipes_comm, MessageType type){
	Message message;
	local_id i;
	
	for (i = 1; i < pipes_comm->total_ids; i++){
		if (i == pipes_comm->current_id){
			continue;
		}
		while (receive(pipes_comm, i, &message) < 0);
		
		set_lamport_time_from_msg(&message);
	}
	
	switch (type){
        case STARTED:
            log_received_all_started(pipes_comm->current_id);
            break;
        case DONE:
            log_received_all_done(pipes_comm->current_id);
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



int do_parent_work(PipesCommunication* pipes_comm);
int do_child_work(PipesCommunication* pipes_comm, int mutexl);
int get_agrs(int argc, char** argv, int* processes, int* mutexl);

int main(int argc, char** argv){
	size_t i;
	int proc_count;
	int mutexl;
	int* pipes;
	pid_t* children;
	pid_t fork_id;
	local_id current_proc_id;
	PipesCommunication* pipes_comm;
	
	if (argc < 3 || get_agrs(argc, argv, &proc_count, &mutexl) == -1){
		fprintf(stderr, "Usage: %s -p X [--mutexl]\n", argv[0]);
		return -1;
	}
	
	
	log_init();
	
	
	children = malloc(sizeof(pid_t) * proc_count);
	
	
	pipes = pipes_init(proc_count + 1); // Open pipes for all processes 
	
	
	for (i = 0; i < proc_count; i++){
		fork_id = fork();
		if (fork_id < 0){
			return -2;
		}
		else if (!fork_id){
			free(children);
			break;
		}
		children[i] = fork_id;
	}
	

	if (!fork_id){
		current_proc_id = i + 1;
	}
	else{
		current_proc_id = PARENT_ID;
	}
	
	
	pipes_comm = communication_init(pipes, proc_count + 1, current_proc_id);
	log_pipes(pipes_comm);
	
	
	if (current_proc_id == PARENT_ID){
		do_parent_work(pipes_comm);
	}
	else{
		do_child_work(pipes_comm, mutexl);
	}
	
	
	if (current_proc_id == PARENT_ID){
		for (i = 0; i < proc_count; i++){
			waitpid(children[i], NULL, 0);
		}
	}
	

	log_destroy();
	communication_destroy(pipes_comm);
	return 0;
}




int do_parent_work(PipesCommunication* pipes_comm){
	CS lamport_comm;
	lamport_comm.comm = pipes_comm;
	lamport_comm.queue = NULL;
	lamport_comm.done_left = pipes_comm->total_ids - 1;
	
	
	receive_all_msgs(pipes_comm, STARTED);
	
	
	while (lamport_comm.done_left){
		Message message;
		
		while (receive_any(pipes_comm, &message));
		
		if (message.s_header.s_type == DONE){
			set_lamport_time_from_msg(&message);
			cs_work(&lamport_comm, &message);
		}
	}
	
	log_received_all_done(pipes_comm->current_id);
	return 0;
}
int do_child_work(PipesCommunication* pipes_comm, int mutexl){
	LamportQueue* queue = lamport_queue_init();
	CS lamport_comm;
	local_id i;
	char buf[MAX_PAYLOAD_LEN];
	
	lamport_comm.comm = pipes_comm;
	lamport_comm.queue = queue;
	lamport_comm.done_left = pipes_comm->total_ids - 2;
	
	
	send_all_proc_event_msg(pipes_comm, STARTED);
	receive_all_msgs(pipes_comm, STARTED);
	
	
	for (i = 1; i <= pipes_comm->current_id * 5; i++){
		
		if (mutexl){
			request_cs(&lamport_comm);
		}
		/* Critical area */
		snprintf(buf, MAX_PAYLOAD_LEN, log_loop_operation_fmt, pipes_comm->current_id, i, pipes_comm->current_id * 5);
		print(buf);
		
		
		if (mutexl){
			release_cs(&lamport_comm);
		}
	}
	
	
	send_all_proc_event_msg(pipes_comm, DONE);
	
	while (lamport_comm.done_left){
		Message message;
		
		while (receive_any(pipes_comm, &message));
		
		set_lamport_time_from_msg(&message);
		cs_work(&lamport_comm, &message);
	}
	log_received_all_done(pipes_comm->current_id);
	
	lamport_queue_destroy(queue);
	return 0;
}


int get_agrs(int argc, char** argv, int* processes, int* mutexl){
	int res;
	const struct option long_options[] = {
        {"mutexl", no_argument, mutexl, 1},
        {NULL, 0, NULL, 0}
    };
	
	*mutexl = 0;
	
	while ((res = getopt_long(argc, argv, "p:", long_options, NULL)) != -1){
		if (res == 'p'){
			*processes = atoi(optarg);
		}
		else if (res == '?'){
			return -1;
		}
	}
	return 0;
}
