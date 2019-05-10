#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>


#include "io.h"
#include "main.h"


/** Syncronization cycle. */
void synchronize(proc_t *proc, MessageType m_type, char *payload, size_t payload_len) {
    Message tmp_msg = init_msg(m_type , payload_len);
    memcpy(tmp_msg.s_payload, payload, payload_len);
    send_multicast((void*)proc, (const Message *)&tmp_msg);
    for (size_t i = 1; i <= proc_number; i++) {
       if (i != proc->self_id)
           while(receive((void*)proc, i, &tmp_msg) != 0);
    }
}

/* Copy balance if there was no transfer on the <time>. */
void balance_copy(BalanceHistory *balance_history, uint8_t time) {
    BalanceState template = balance_history->s_history[balance_history->s_history_len-1];
    for (uint8_t i = balance_history->s_history_len; i <= time; i++) {
        balance_history->s_history[i] = template;
        balance_history->s_history[i].s_time = i;
    }
    balance_history->s_history_len = time + 1;
}

void balance_set(BalanceHistory *balance_history, balance_t balance) {
    balance_copy(balance_history, (uint8_t)get_physical_time());
    balance_history->s_history[get_physical_time()] = (BalanceState) {
            .s_balance = balance_history->s_history[balance_history->s_history_len-1].s_balance + balance,
            .s_time = get_physical_time(),
            .s_balance_pending_in = 0
    };
    balance_history->s_history_len = get_physical_time()+1;
}

/** Transfer cycle.  */
void transfer_cycle(proc_t *proc, BalanceHistory *balance_history, TransferOrder *transfer_order) {
    Message message;
    if (proc->self_id == transfer_order->s_src) {
        balance_set(balance_history, -transfer_order->s_amount);
        message = init_msg(TRANSFER,sizeof(TRANSFER));
        memcpy(message.s_payload, transfer_order, sizeof(TransferOrder));
        send((void *)proc, transfer_order->s_dst, &message);

        fprintf(event_log, log_transfer_out_fmt,
                get_physical_time(), proc->self_id,
                transfer_order->s_amount, transfer_order->s_dst);

    } else if (proc->self_id == transfer_order->s_dst) {
        balance_set(balance_history, transfer_order->s_amount);
        fprintf(event_log, log_transfer_in_fmt,
                get_physical_time(), proc->self_id,
                transfer_order->s_amount, transfer_order->s_dst);
        message = init_msg(ACK,0);
        send((void *)proc, PARENT_ID, &message);
    } else {
        fprintf(event_log, "PID %d received messsage for %d and %d.\n",
                proc->self_id, transfer_order->s_src, transfer_order->s_dst);
    }
    
}

void working_cycle(proc_t *proc, BalanceHistory *balance_history) {
    while (true) {
        Message tmp_msg = {{ 0 }};
        if (receive_any(proc, &tmp_msg) < 0) continue;
        if(tmp_msg.s_header.s_type == TRANSFER){
            fprintf(pipe_log,"PID %d received TRANSFER message.\n",proc->self_id);
            transfer_cycle(proc, balance_history, (TransferOrder*)(tmp_msg.s_payload));
        }else if(tmp_msg.s_header.s_type == STOP){
            fprintf(pipe_log,"PID %d received STOP message.\n",proc->self_id);
            return;
        } else {
            fprintf(pipe_log,"PID %d received wrong message with m_type %d.\n",
                proc->self_id, tmp_msg.s_header.s_type);
            continue;
        }
    }
}

BalanceHistory init_balance_history(proc_t *proc){
    BalanceHistory balance_history = {
        .s_id = proc->self_id,
        .s_history_len = 0,
        .s_history = {{ 0 }}
    };
    return balance_history;
}

/** Child main function. */ 
int process_c(proc_t *proc, balance_t balance) {

    char payload[MAX_PAYLOAD_LEN];
    size_t len;
    BalanceHistory balance_history = init_balance_history(proc);

    for (timestamp_t i = 0; i <= get_physical_time(); i++) {
        balance_history.s_history[i] = (BalanceState) {
            .s_balance = balance,
            .s_time = i,
            .s_balance_pending_in = 0
        };
    }
    balance_history.s_history_len = get_physical_time()+1;

    close_fds(proc->io, proc->self_id);
    /* Process starts. */

    len = sprintf(payload, log_started_fmt, 
                  get_physical_time(), proc->self_id, getpid(),
                  getppid(), balance_history.s_history[0].s_balance);

    fputs(payload, event_log); 

    /* Proces sync with others. */
    synchronize(proc, STARTED, payload, len);
    fprintf(event_log, log_received_all_started_fmt,
            get_physical_time(), proc->self_id);

    /* Work. */
    working_cycle(proc, &balance_history);

    /* Process's done. */
    len = sprintf(payload, log_done_fmt, get_physical_time(), 
            proc->self_id, balance_history.s_history[balance_history.s_history_len-1].s_balance);
    fputs(payload, event_log); 

    /* Process syncs wih ohers. */
    synchronize(proc, DONE, payload, len);
    fprintf(event_log, log_received_all_done_fmt, 
            get_physical_time(), proc->self_id);

    balance_copy(&balance_history, get_physical_time());

    synchronize(proc, BALANCE_HISTORY, (char*)&balance_history, sizeof(balance_history));
    return 0;
}
