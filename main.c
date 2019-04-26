#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

int get_arguments(int argc, char **argv) {
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

Message init_msg(MessageType type , size_t payload_len){
    Message msg;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_type = type;
    msg.s_header.s_payload_len = payload_len;
    msg.s_header.s_local_time = get_physical_time();
    return msg;
}

void receive_all_msg(proc_t *proc, MessageType m_type) {
    Message tmp_msg = { {0} };
    for (local_id i = 1; i <= proc_number; i++) {
        while(receive((void*)proc, i, &tmp_msg) != 0);
        if (tmp_msg.s_header.s_type != m_type) {
            fprintf(proc->io->pipes_log_stream, 
                    "Bad received message: waited for %d, but got %d.\n",
                    m_type, tmp_msg.s_header.s_type);
        }
    }
}


void receive_all_balance(proc_t *proc, AllHistory *all_history) {
    Message tmp_msg = {{ 0 }};
    local_id i = 1;
    while (i <= proc_number) {
        while(receive((void*)proc, i, &tmp_msg) != 0);

        if (tmp_msg.s_header.s_type == BALANCE_HISTORY) {
            memcpy(&all_history->s_history[i-1], &tmp_msg.s_payload, tmp_msg.s_header.s_payload_len);
            balance_t b  = all_history->s_history[i-1].s_history[all_history->s_history[i-1].s_history_len].s_balance;
            fprintf(proc->io->events_log_stream, log_done_fmt, get_physical_time(),proc->self_id, b);
            i++;
        }
    }
    all_history->s_history_len = proc_number;
}

int main(int argc, char *argv[]) {

    IO io = { 0 };
    proc_t process = (proc_t){ 
        .io = &io
    };

    if(get_arguments(argc, argv) != 0) {
        perror("can't read arguments");
        return -1;
    }
    io.events_log_stream = fopen(events_log, "w+");
    io.pipes_log_stream  = fopen(pipes_log, "w");

    if (init_pipes(&io, proc_number) < 0)
        return -1;

    for (int i = 1; i <= proc_number; i++) {
        pid_t pid = fork();
        if (0 > pid) {
            exit(EXIT_FAILURE);
        } else if (0 == pid) {
            /* Child. */
            process.self_id = i;
            int ret = process_c(&process, balances[i-1]);
            exit(ret);
        }
    }
    close_fds(&io, PARENT_ID, proc_number);

    process.self_id = PARENT_ID;
    AllHistory all_history = { 0 };

    receive_all_msg(&process, STARTED);
    fprintf(io.events_log_stream, log_received_all_started_fmt, 
            get_physical_time(), PARENT_ID);

    bank_robbery(&process, proc_number);

    Message msg = init_msg(STOP,0);
    send_multicast(&process,&msg);

    receive_all_msg(&process, DONE);
    fprintf(io.events_log_stream,log_received_all_done_fmt,
            get_physical_time(), PARENT_ID);
    receive_all_balance(&process, &all_history);
    print_history(&all_history);

    while(wait(NULL) > 0);

    fclose(io.pipes_log_stream);
    fclose(io.events_log_stream);
    return 0;
}
