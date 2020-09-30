#ifndef __IFMO_DISTRIBUTED_CLASS_LAMPORT__H
#define __IFMO_DISTRIBUTED_CLASS_LAMPORT__H

#include "ipc.h"

struct Node{
	struct Node* prev;	
	struct Node* next;	
	int key;			
	int value;			
};

typedef struct Node QueueNode;

typedef struct{
	QueueNode* head;	
	QueueNode* tail;	
} LamportQueue;

LamportQueue* lamport_queue_init();
void lamport_queue_destroy(LamportQueue* queue);

void lamport_queue_insert(LamportQueue* queue, timestamp_t key, local_id value);
local_id lamport_queue_peek(LamportQueue* queue);
local_id lamport_queue_get(LamportQueue* queue);

/* Time functions */
timestamp_t increment_lamport_time();
timestamp_t set_lamport_time(timestamp_t new_lamport_time);
timestamp_t set_lamport_time_from_msg(Message* msg);
timestamp_t get_lamport_time();

#endif
