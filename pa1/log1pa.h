#ifndef __IFMO_DISTRIBUTED_CLASS_LOG1PA__H
#define __IFMO_DISTRIBUTED_CLASS_LOG1PA__H

#include "ipc.h"
#include "communication.h"

void log_init();
void log_started(local_id id);
void log_received_all_started(local_id id);
void log_done(local_id id);
void log_received_all_done(local_id id);
void log_destroy();
void log_pipes(PipesCommunication* comm);

#endif
