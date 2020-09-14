#ifndef __IFMO_DISTRIBUTED_CLASS_LTIME__H
#define __IFMO_DISTRIBUTED_CLASS_LTIME__H

#include "ipc.h"

timestamp_t increment_lamport_time();
timestamp_t set_lamport_time(timestamp_t new_lamport_time);
timestamp_t set_lamport_time_from_msg(Message* msg);
timestamp_t get_lamport_time();

#endif

