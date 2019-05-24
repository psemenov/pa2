#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>


#include "proc.h"

TransferOrder init_transfer_order(local_id src, local_id dst,balance_t amount){
    TransferOrder order = (TransferOrder) {
        .s_src = src,
        .s_dst = dst,
        .s_amount = amount
    };
    return order;
}

void transfer(void * parent_data, local_id src, local_id dst,balance_t amount){
    inc_time();
    Message tmp_msg= init_msg(TRANSFER,sizeof(TransferOrder));
    TransferOrder order = init_transfer_order(src, dst, amount);
    memcpy(tmp_msg.s_payload, &order, sizeof(TransferOrder));
    send(parent_data, src, &tmp_msg);
    while (receive(parent_data, dst, &tmp_msg))
        if (tmp_msg.s_header.s_type == ACK) break;
    set_lamport_time(tmp_msg.s_header.s_local_time);
    inc_time();
}

/** Syncronization cycle. */
void synchronize(process *proc, MessageType m_type, char *payload, size_t payload_len) {
    inc_time();
    Message tmp_msg = init_msg(m_type , payload_len);
    memcpy(tmp_msg.s_payload, payload, payload_len);
    send_multicast((void*)proc, (const Message *)&tmp_msg);
    for (size_t i = 1; i <= proc_number; i++) {
       if (i != proc->id) {
           while(receive((void*)proc, i, &tmp_msg) != 0);
           set_lamport_time(tmp_msg.s_header.s_local_time);
           inc_time();
       }
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
    balance_t pending_balance = balance_history->s_history[balance_history->s_history_len-1].s_balance_pending_in;
    balance_copy(balance_history, (uint8_t)get_lamport_time());
    balance_history->s_history[get_lamport_time()] = (BalanceState) {
            .s_balance = balance_history->s_history[balance_history->s_history_len-1].s_balance + balance,
            .s_time = get_lamport_time(),
            .s_balance_pending_in = pending_balance - balance
    };
    balance_history->s_history_len = get_lamport_time()+1;
}



/** Transfer cycle.  */
void transfer_cycle(process *proc, BalanceHistory *balance_history, TransferOrder *order) {
    inc_time();
    Message message;
    if (proc->id == order->s_src) {
        balance_set(balance_history, -order->s_amount);
        message = init_msg(TRANSFER,sizeof(TRANSFER));
        memcpy(message.s_payload, order, sizeof(TransferOrder));
        send((void *)proc, order->s_dst, &message);

        fprintf(event_log, log_transfer_out_fmt,
                get_lamport_time(), proc->id,
                order->s_amount, order->s_dst);

    } else if (proc->id == order->s_dst) {
        balance_set(balance_history, order->s_amount);
        fprintf(event_log, log_transfer_in_fmt,
                get_lamport_time(), proc->id,
                order->s_amount, order->s_dst);
        message = init_msg(ACK,0);
        send((void *)proc, PARENT_ID, &message);
    } else {
        fprintf(event_log, "PID %d received messsage for %d and %d.\n",
                proc->id, order->s_src, order->s_dst);
    }
    
}

void working_cycle(process *proc, BalanceHistory *balance_history) {
    while (true) {
        Message tmp_msg = {{ 0 }};
        if (receive_any(proc, &tmp_msg) < 0) continue;
        set_lamport_time(tmp_msg.s_header.s_local_time);
        inc_time();

        if(tmp_msg.s_header.s_type == TRANSFER){
            fprintf(pipe_log,"PID %d received TRANSFER message.\n",proc->id);
            transfer_cycle(proc, balance_history, (TransferOrder*)(tmp_msg.s_payload));
        }else if(tmp_msg.s_header.s_type == STOP){
            fprintf(pipe_log,"PID %d received STOP message.\n",proc->id);
            return;
        } else {
            fprintf(pipe_log,"PID %d received wrong message with m_type %d.\n",
                proc->id, tmp_msg.s_header.s_type);
            continue;
        }
    }
}

BalanceHistory init_balance_history(process *proc){
    BalanceHistory balance_history = {
        .s_id = proc->id,
        .s_history_len = 0,
        .s_history = {{ 0 }}
    };
    return balance_history;
}

/** Child main function. */ 
int process_c(process *proc, balance_t balance) {

    char payload[MAX_PAYLOAD_LEN];
    size_t payload_len;
    BalanceHistory balance_history = init_balance_history(proc);

    for (timestamp_t i = 0; i <= get_lamport_time(); i++) {
        balance_history.s_history[i] = (BalanceState) {
            .s_balance = balance,
            .s_time = i,
            .s_balance_pending_in = 0
        };
    }
    balance_history.s_history_len = get_lamport_time()+1;

    close_fds(pipes, proc->id);
    /* Process starts. */

    payload_len = sprintf(payload, log_started_fmt, 
                  get_lamport_time(), proc->id, getpid(),
                  getppid(), balance_history.s_history[0].s_balance);
    fputs(payload, event_log); 

    /* Synchronization */
    synchronize(proc, STARTED, payload, payload_len);
    fprintf(event_log, log_received_all_started_fmt,
            get_lamport_time(), proc->id);

    /* Work. */
    working_cycle(proc, &balance_history);

    /* Process's done. */
    payload_len = sprintf(payload, log_done_fmt, get_lamport_time(), 
            proc->id, balance_history.s_history[balance_history.s_history_len-1].s_balance);
    fputs(payload, event_log); 

    /* Synchronization */
    synchronize(proc, DONE, payload, payload_len);
    fprintf(event_log, log_received_all_done_fmt, get_lamport_time(), proc->id);

    balance_copy(&balance_history, get_lamport_time());

    Message tmp_msg;
    payload_len = sizeof(balance_history.s_history[0]) * balance_history.s_history_len;
    inc_time();
    tmp_msg = init_msg(BALANCE_HISTORY, payload_len);
    memcpy(tmp_msg.s_payload, (char*)&balance_history, payload_len);
    send((void*)proc, PARENT_ID, &tmp_msg);
    return 0;
}
