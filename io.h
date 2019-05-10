#ifndef __IFMO_DISTRIBUTED_CLASS_IO__H
#define __IFMO_DISTRIBUTED_CLASS_IO__H

#include <stdio.h>
#include "ipc.h"
#include "banking.h"

#define MAX_PROC    10
#define MAX_PIPES   110
#define NUM_FD      2

#define READ_FD     0
#define WRITE_FD    1

typedef struct {
   
    FILE *events_log_stream;
    FILE *pipes_log_stream;

    int fds[MAX_PROC+1][MAX_PROC+1][NUM_FD];
} IO;

int proc_number;
balance_t balances[MAX_PROCESS_ID];

int create_pipes(IO *io, local_id pnum);

void close_unsed_fds(const IO* io, local_id id, local_id pnum);

#endif /* __IFMO_DISTRIBUTED_CLASS_IO__H */
