#define _DEFAULT_SOURCE
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>

#include "io.h"
#include "ipc.h"
#include "child.h"

int send(void *self, local_id dst, const Message *msg) {
    proc_t* p   = (proc_t*)self;
    local_id src  = p->self_id;
    if (write(p->io->fds[src][dst][WRITE_FD], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) < 0) {
        perror("write");
        return -1;
    }

    int    fd   = p->io->fds[src][dst][WRITE_FD];
    write(fd, msg, sizeof(MessageHeader) + msg->s_header.s_payload_len);
    return 0;
}

int send_multicast(void *self, const Message *msg) {
    proc_t* p   = (proc_t*)self;
    for (local_id i = 0; i <= proc_number; i++) 
        if (i != p->self_id) send(self, i, msg);
    return 0;
}

int receive(void *self, local_id from, Message *msg) {
    proc_t* p   = (proc_t*)self;
    local_id dst = p->self_id;
    ssize_t read_result = read(p->io->fds[from][dst][READ_FD], msg, sizeof(Message));
    if (read_result < 0 && errno != EAGAIN) {
        perror("read");
        return -1;
    }
    if(read_result <= 0) return -1;
    return 0;
}

int receive_any(void *self, Message *msg) {
    for (local_id i = 0; i <= proc_number; i++) {
        if (receive(self, i, msg) == 0) return 0;
    }
    return -1;   
}
