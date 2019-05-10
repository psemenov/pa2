#define _DEFAULT_SOURCE
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>

#include "proc.h"


int send(void *self, local_id dst, const Message *msg) {
    process* p = (process*)self;
    local_id src = p->id;
    if (write(pipes[src][dst][WRITE_FD], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) < 0) {
        perror("Send ipc.c");
        return -1;
    }
    return 0;
}

int send_multicast(void *self, const Message *msg) {
    process* p = (process*)self;
    for (local_id i = 0; i <= proc_number; i++) 
        if (i != p->id) send(self, i, msg);
    return 0;
}

int receive(void *self, local_id from, Message *msg) {
    process* p = (process*)self;
    local_id dst = p->id;
    if(from == dst) return -1;
    ssize_t read_result = read(pipes[from][dst][READ_FD], msg, sizeof(Message));
    if (read_result < 0 && errno != EAGAIN) {
        perror("Receive ipc.c");
        return -1;
    }
    if(read_result < 0) return -1;
    return 0;
}

int receive_any(void *self, Message *msg) {
    for (local_id i = 0; i <= proc_number; i++) 
        if (receive(self, i, msg) == 0) return 0;
    return -1;   
}
