#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "ipc.h"
#include "pa2345.h"
#include "child.h"
#include "banking.h"

#define ARGUMENTS_OFFSET     3


int get_args(int argc, char **argv) {
    if (argc != atoi(argv[2])+ARGUMENTS_OFFSET || (strcmp(argv[1], "-p") != 0))     return -1;
    proc_number = atoi(argv[2]);
    if(proc_number < 0) return -1;
    for (int i = 0; i < proc_number; i++){
        balances[i] = atoi(argv[i+ARGUMENTS_OFFSET]);
        if (balances[i] < 0){
            return -1;
        }
    }
    return 0;
}

static void
wait_msg_from_all(proc_t *p, MessageType type) {
    Message msg = { {0} };
    for (local_id i = 1; i <= proc_number; i++) {
        while(receive((void*)p, i, &msg) != 0);
        if (msg.s_header.s_type != type) {
            fprintf(p->io->pipes_log_stream, 
                    "Bad received message: waited for %d, but got %d.\n",
                    type, msg.s_header.s_type);
        }
    }
}

static void
send_stop(proc_t *p) {
    Message msg;
    msg.s_header = (MessageHeader){
        .s_magic       = MESSAGE_MAGIC,
        .s_payload_len = 0,
        .s_type        = STOP,
        .s_local_time  = get_physical_time(),
    };

    send_multicast((void*)p, &msg);
}

static void
wait_balance_from_all(proc_t *p, AllHistory *all_history) {
    Message msg = {{ 0 }};
    local_id i = 1;

    while (i <= proc_number) {
        while(receive((void*)p, i, &msg) != 0);

        if (msg.s_header.s_type == BALANCE_HISTORY) {
            memcpy(&all_history->s_history[i-1], &msg.s_payload, 
                                             msg.s_header.s_payload_len);

            uint8_t hlen = all_history->s_history[i-1].s_history_len;

            balance_t b  = all_history->s_history[i-1].s_history[hlen].s_balance;
            fprintf(p->io->events_log_stream, log_done_fmt, get_physical_time(),
                    p->self_id, b);
            i++;
        }
    }
    all_history->s_history_len = proc_number;
}

static void
main_cycle(proc_t *p) {
    AllHistory all_history = { 0 };
    const IO *io = p->io;

    wait_msg_from_all(p, STARTED);
    fprintf(io->events_log_stream, log_received_all_started_fmt, 
            get_physical_time(), PARENT_ID);

    bank_robbery((void*)p, proc_number);

    send_stop(p);

    wait_msg_from_all(p, DONE);
    fprintf(io->events_log_stream, log_received_all_done_fmt,
            get_physical_time(), PARENT_ID);
    wait_balance_from_all(p, &all_history);
    print_history(&all_history);

    while(wait(NULL) > 0);
}

int
main(int argc, char *argv[]) {


    IO io = { 0 };
    proc_t proc = (proc_t){ 
        .io = &io
    };

    if(get_args(argc, argv) != 0) {
        perror("can't read arguments");
        return -1;
    }
    proc.procnum = proc_number;
    io.events_log_stream = fopen(events_log, "w+");
    io.pipes_log_stream  = fopen(pipes_log, "w");

    if (create_pipes(&io, proc_number) < 0)
        return -1;

    for (int i = 1; i <= proc_number; i++) {
        pid_t pid = fork();
        if (0 > pid) {
            exit(EXIT_FAILURE);
        } else if (0 == pid) {
            /* Child. */
            proc.self_id = i;
            int ret = child(&proc, balances[i-1]);
            exit(ret);
        }
    }
    close_unsed_fds(&io, PARENT_ID, proc_number);

    proc.self_id = PARENT_ID;
    main_cycle(&proc);

    fclose(io.pipes_log_stream);
    fclose(io.events_log_stream);
    return 0;
}