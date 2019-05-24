#define _DEFAULT_SOURCE
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>

#include "proc.h"


int send(void *self, local_id dst, const Message *msg) {
    process* p = (process*)self;
    local_id src = p->id;
    if(dst == src) return -1;
    write(pipes[src][dst][WRITE_FD], &msg->s_header, sizeof(MessageHeader));
    write(pipes[src][dst][WRITE_FD], msg->s_payload, msg->s_header.s_payload_len);
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
    char* buffer = (char*)msg;
    ssize_t read_result = read(pipes[from][dst][READ_FD], buffer, sizeof(MessageHeader));
    if(read_result <= 0) return -1;
    read_result = 0;
    if(msg->s_header.s_payload_len > 0)
    for(;;){
        read_result = read(pipes[from][dst][READ_FD], buffer + sizeof(MessageHeader),  msg->s_header.s_payload_len);
        if(read_result >= 0) break;
    }
    return 0;

}

int receive_any(void *self, Message *msg) {
    for (local_id i = 0; i <= proc_number; i++) 
        if (receive(self, i, msg) == 0) return 0;
    return -1;   
}
