#ifndef __IFMO_DISTRIBUTED_CLASS_LOG4PA__H
#define __IFMO_DISTRIBUTED_CLASS_LOG4PA__H

#include "communication.h"
#include "ipc.h"


void log_init();
void log_started(local_id id);
void log_received_all_started(local_id id);
void log_done(local_id id);
void log_received_all_done(local_id id);
void log_pipes(PipesCommunication* pipes_comm);
void log_destroy();

#endif
