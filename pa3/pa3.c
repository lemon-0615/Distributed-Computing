#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include "log3pa.h"
#include "communication.h"
#include "banking.h"
#include "ltime.h"
#include "common.h"
#include "pa2345.h"
#include <fcntl.h>


int set_nonblock(int pipe_id);

/* lamport_time operation*/
static timestamp_t lamport_time = 0;

timestamp_t increment_lamport_time(){
	return ++lamport_time;
}

timestamp_t set_lamport_time(timestamp_t new_lamport_time){
	if (lamport_time < new_lamport_time){
		lamport_time = new_lamport_time;
	}
	return lamport_time;
}

timestamp_t set_lamport_time_from_msg(Message* msg){
	set_lamport_time(msg->s_header.s_local_time);
	return increment_lamport_time();
}

timestamp_t get_lamport_time(){
	return lamport_time;
}


FILE* pipes_log_f;
FILE* events_log_f;

void log_init(){
	pipes_log_f = fopen(pipes_log, "w");
	events_log_f = fopen(events_log, "w");
}

void log_started(local_id id, balance_t balance){
	printf(log_started_fmt, get_lamport_time(), id, getpid(), getppid(), balance);
    fprintf(events_log_f, log_started_fmt, get_lamport_time(), id, getpid(), getppid(), balance);
}

void log_received_all_started(local_id id){
	printf(log_received_all_started_fmt, get_lamport_time(), id);
    fprintf(events_log_f, log_received_all_started_fmt, get_lamport_time(), id);
}

void log_done(local_id id, balance_t balance){
	printf(log_done_fmt, get_lamport_time(), id, balance);
    fprintf(events_log_f, log_done_fmt, get_lamport_time(), id, balance);
}

void log_received_all_done(local_id id){
	printf(log_received_all_done_fmt, get_lamport_time(), id);
    fprintf(events_log_f, log_received_all_done_fmt, get_lamport_time(), id);
}

void log_transfer_out(local_id from, local_id dst, balance_t amount){
	printf(log_transfer_out_fmt, get_lamport_time(), from, amount, dst);
	fprintf(events_log_f, log_transfer_out_fmt, get_lamport_time(), from, amount, dst);
}

void log_transfer_in(local_id from, local_id dst, balance_t amount){
	printf(log_transfer_in_fmt, get_lamport_time(), dst, amount, from);
	fprintf(events_log_f, log_transfer_in_fmt, get_lamport_time(), dst, amount, from);
}

void log_destroy(){
	fclose(pipes_log_f);
    fclose(events_log_f);
}

void log_pipes(PipesCommunication* pipes_comm){
	size_t i;
	
	fprintf(pipes_log_f, "process %d pipes:\n", pipes_comm->current_id);
	
	for (i = 0; i < pipes_comm->total_ids; i++){
		if (i == pipes_comm->current_id){
			continue;
		}
		
		fprintf(pipes_log_f, "P%ld|R%d|W%d ", i, pipes_comm->pipes[(i < pipes_comm->current_id ? i : i-1) * 2 + PIPE_READ_TYPE], 
			pipes_comm->pipes[(i < pipes_comm->current_id ? i : i-1) * 2 + PIPE_WRITE_TYPE]);
	}
	fprintf(pipes_log_f, "\n");
}

void update_history(BalanceState* state, BalanceHistory* history, balance_t amount, timestamp_t timestamp_msg, char inc, char fix){
	static timestamp_t prev_time = 0;
    //timestamp_t curr_time = get_physical_time();
   timestamp_t curr_time = get_lamport_time() < timestamp_msg ? timestamp_msg : get_lamport_time();
	timestamp_t i;
	if (inc){
		curr_time++;
	}
	set_lamport_time(curr_time);
	if (fix){
		timestamp_msg--;
	}
    history->s_history_len = curr_time + 1;
	
	for (i = prev_time; i < curr_time; i++){
		state->s_time = i;
		history->s_history[i] = *state;
	}
	if (amount > 0){
	  for (i = timestamp_msg; i < curr_time; i++){
	      history->s_history[i].s_balance_pending_in += amount;
		}
	}
	prev_time = curr_time;
	state->s_time = curr_time;
	state->s_balance += amount;
	history->s_history[curr_time] = *state;
}


int do_transfer(PipesCommunication* pipes_comm, Message* msg, BalanceState* state, BalanceHistory* history){
	TransferOrder order;
	memcpy(&order, msg->s_payload, sizeof(char) * msg->s_header.s_payload_len);
	
		/* Transfer request */
	if (pipes_comm->current_id == order.s_src){
		update_history(state, history, -order.s_amount, msg->s_header.s_local_time, 1, 0);
		update_history(state, history, 0, 0, 1, 0);
		send_transfer_msg(pipes_comm, order.s_dst, &order);
		pipes_comm->balance -= order.s_amount;
	}
		/* Transfer income */
	else if (pipes_comm->current_id == order.s_dst){
		update_history(state, history, order.s_amount, msg->s_header.s_local_time, 1, 1);
		increment_lamport_time();
		send_ack_msg(pipes_comm, PARENT_ID);
		pipes_comm->balance += order.s_amount;
	}
	else{
		return -1;
	}
	return 0;
}


int do_parent_work(PipesCommunication* pipes_comm){
	AllHistory all_history;
	local_id i;
	
	all_history.s_history_len = pipes_comm->total_ids - 1;
	
    receive_all_msgs(pipes_comm, STARTED);

    bank_robbery(pipes_comm, pipes_comm->total_ids - 1);
    increment_lamport_time();
    send_all_stop_msg(pipes_comm);
    receive_all_msgs(pipes_comm, DONE);
	
	for (i = 1; i < pipes_comm->total_ids; i++){
		BalanceHistory balance_history;
		Message msg;
		
		while(receive(pipes_comm, i, &msg));
		
		if (msg.s_header.s_type != BALANCE_HISTORY){
			return -1;
		}
		
		memcpy((void*)&balance_history, msg.s_payload, sizeof(char) * msg.s_header.s_payload_len);
		all_history.s_history[i - 1] = balance_history;
	}
	
	print_history(&all_history);
	return 0;
}

int do_child_work(PipesCommunication* pipes_comm){
	BalanceState balance_state;
        BalanceHistory balance_history;
	size_t done_left = pipes_comm->total_ids - 2;
	int not_stopped = 1;

    	balance_history.s_id = pipes_comm->current_id;
	
	balance_state.s_balance = pipes_comm->balance;
    	balance_state.s_balance_pending_in = 0;
    	balance_state.s_time = 0;
	
	update_history(&balance_state, &balance_history, 0, 0, 0, 0);
	
	increment_lamport_time();   //lamport_time
	send_all_proc_event_msg(pipes_comm, STARTED); //send & receive STARTED message 
	increment_lamport_time();
        receive_all_msgs(pipes_comm, STARTED);
	
	while(done_left || not_stopped){
		Message msg;
		
        while (receive_any(pipes_comm, &msg));
		
		if (msg.s_header.s_type == TRANSFER){
			do_transfer(pipes_comm, &msg, &balance_state, &balance_history);
		}
		else if (msg.s_header.s_type == STOP){
	   update_history(&balance_state, &balance_history, 0, msg.s_header.s_local_time, 1, 0);
			send_all_proc_event_msg(pipes_comm, DONE);
			not_stopped = 0;
		}
		else if (msg.s_header.s_type == DONE){
           update_history(&balance_state, &balance_history, 0, msg.s_header.s_local_time, 1, 0); 
			done_left--;
		}
		else{
			return -1;
		}
	}
	
	log_received_all_done(pipes_comm->current_id);
	
	update_history(&balance_state, &balance_history, 0, 0, 1, 0);
	send_balance_history(pipes_comm, PARENT_ID, &balance_history);
	return 0;
}






int get_proc_count(int argc, char** argv){
	int proc_count;
	if (!strcmp(argv[1], "-p") && (proc_count = atoi(argv[2])) == (argc - 3)){
		return proc_count;
	}
	return -1;
}


balance_t get_proc_balance(local_id proc_id, char** argv){
	return atoi(argv[proc_id + 2]);
}
int main(int argc, char** argv){
	size_t i;
	int proc_count;
	int* pipes;
	pid_t* children;
	pid_t fork_id;
	local_id current_proc_id;
	PipesCommunication* pipes_comm;
	
	
	if (argc < 4 || (proc_count = get_proc_count(argc, argv)) == -1){
		fprintf(stderr, "Usage: %s -p X y1 y2 ... yX\n", argv[0]);
		return -1;
	}
	
	log_init(); // Initialize log files 
	
	children = malloc(sizeof(pid_t) * proc_count);
	
	pipes = pipes_init(proc_count + 1); // open pipes 
	
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
	
	
	pipes_comm = communication_init(pipes, proc_count + 1, current_proc_id, 		    get_proc_balance(current_proc_id, argv));
	log_pipes(pipes_comm);
	
	
	if (current_proc_id == PARENT_ID){
		do_parent_work(pipes_comm);
	}
	else{
		do_child_work(pipes_comm);
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

/*Init PipesCommunication*/
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


void communication_destroy(PipesCommunication* pipes_comm){
	size_t i;
	for (i = 0; i < pipes_comm->total_ids - 1; i++){
		close(pipes_comm->pipes[i * 2 + PIPE_READ_TYPE]);
		close(pipes_comm->pipes[i * 2 + PIPE_WRITE_TYPE]);
	}
	free(pipes_comm);
}


int send_all_proc_event_msg(PipesCommunication* pipes_comm, MessageType type){
	Message message;
	uint16_t length = 0;
	char buf[MAX_PAYLOAD_LEN];
	
	message.s_header.s_magic = MESSAGE_MAGIC;
    	message.s_header.s_type = type;
    	//message.s_header.s_local_time = get_physical_time();
	message.s_header.s_local_time = get_lamport_time();
	switch (type){
        case STARTED:
			length = snprintf(buf, MAX_PAYLOAD_LEN, log_started_fmt, get_lamport_time(), pipes_comm->current_id, getpid(), getppid(), pipes_comm->balance);
			break;
		case DONE:
			length = snprintf(buf, MAX_PAYLOAD_LEN, log_done_fmt, get_lamport_time(), pipes_comm->current_id, pipes_comm->balance);
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
	
	type == STARTED ? log_started(pipes_comm->current_id, pipes_comm->balance) : log_done(pipes_comm->current_id, pipes_comm->balance);
	
	return 0;
}

 /* send STOP message to processes*/
void send_all_stop_msg(PipesCommunication* pipes_comm){
	Message message;
	message.s_header.s_magic = MESSAGE_MAGIC;
    	message.s_header.s_type = STOP;
      //message.s_header.s_local_time = get_physical_time();
   	message.s_header.s_local_time = get_lamport_time();
	message.s_header.s_payload_len = 0;
	
	send_multicast(pipes_comm, &message);
}


 
void send_transfer_msg(PipesCommunication* pipes_comm, local_id dst, TransferOrder* order){
	Message message;
	message.s_header.s_magic = MESSAGE_MAGIC;
    	message.s_header.s_type = TRANSFER;
    	//message.s_header.s_local_time = get_physical_time();
	message.s_header.s_local_time = get_lamport_time();
	message.s_header.s_payload_len = sizeof(TransferOrder);
	
	memcpy(message.s_payload, order, message.s_header.s_payload_len);
	
	while (send(pipes_comm, dst, &message) < 0);
}

/** Send ACK message */
void send_ack_msg(PipesCommunication* pipes_comm, local_id dst){
	Message message;
	message.s_header.s_magic = MESSAGE_MAGIC;
    	message.s_header.s_type = ACK;
   	//message.s_header.s_local_time = get_physical_time();
        message.s_header.s_local_time = get_lamport_time();
	message.s_header.s_payload_len = 0;
	
	while (send(pipes_comm, dst, &message) < 0);
}


void send_balance_history(PipesCommunication* pipes_comm, local_id dst, BalanceHistory* history){
	Message message;
	message.s_header.s_magic = MESSAGE_MAGIC;
   	message.s_header.s_type = BALANCE_HISTORY;
    	//message.s_header.s_local_time = get_physical_time();
        message.s_header.s_local_time = get_lamport_time();
	message.s_header.s_payload_len = sizeof(BalanceHistory);
	
	memcpy(message.s_payload, history, message.s_header.s_payload_len);
	
	while (send(pipes_comm, dst, &message) < 0);
}


void receive_all_msgs(PipesCommunication* pipes_comm, MessageType type){
	Message message;
	local_id i;
	
	for (i = 1; i < pipes_comm->total_ids; i++){
		if (i == pipes_comm->current_id){
			continue;
		}
		while (receive(pipes_comm, i, &message) < 0)
                      ;
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


