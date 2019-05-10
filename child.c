#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "common.h"
#include "ipc.h"
#include "io.h"
#include "pa2345.h"
#include "child.h"
#include "banking.h"

#define BALANCE_HIST_SIZE(hist) \
 (sizeof(BalanceHistory) - (sizeof(hist->s_history) - hist->s_history_len))


/** Syncronization cycle.
 */
static void
sync_state(proc_t *p, MessageType type, char *payload, size_t payload_len) {
    Message msg;
    msg.s_header = (MessageHeader) {
            .s_magic       = MESSAGE_MAGIC,
            .s_payload_len = payload_len,
            .s_type        = type,
            .s_local_time  = 0
    };
    memcpy(msg.s_payload, payload, payload_len);

    send_multicast((void*)p, (const Message *)&msg);

    for (size_t i = 1; i <= p->procnum; i++) {
       if (i != p->self_id)
           while(receive((void*)p, i, &msg) != 0);
    }
}

/* Copy balance if there was no transfer on the <time>. */
static void
copy_range(BalanceHistory *history, uint8_t time) {
    uint8_t min = history->s_history_len;
    BalanceState template = history->s_history[min-1];
    for (uint8_t i = min; i <= time; i++) {
        history->s_history[i] = template;
        history->s_history[i].s_time = i;
    }
    history->s_history_len = time + 1;
}

static void
set_balance(BalanceHistory *history, balance_t balance) {
    timestamp_t t = get_physical_time();
    uint8_t hlen  = history->s_history_len;
    balance_t bal = history->s_history[hlen-1].s_balance;
    copy_range(history, (uint8_t)t);
    history->s_history[t] = (BalanceState) {
            .s_balance = bal + balance,
            .s_time = t,
            .s_balance_pending_in = 0
    };

    history->s_history_len = t+1;
}

/** Add to the history new records about balance changing.
 * 
 * @param sign_balance  If the value is positive balance increases
 *                      and decreases otherwise.
 */
static void
send_transfer(proc_t *p, TransferOrder *order) {
    Message msg;
    msg.s_header = (MessageHeader) {
        .s_magic       = MESSAGE_MAGIC,
        .s_payload_len = sizeof(TransferOrder),
        .s_type        = TRANSFER,
        .s_local_time  = get_physical_time()
    };
    memcpy(msg.s_payload, order, sizeof(TransferOrder));

    send((void *)p, order->s_dst, &msg);
}

static void
send_ack(proc_t *p) {
    Message msg;
    msg.s_header = (MessageHeader) {
        .s_magic       = MESSAGE_MAGIC,
        .s_payload_len = 0,
        .s_type        = ACK,
        .s_local_time  = get_physical_time()
    };
    send((void *)p, PARENT_ID, &msg);
}

/** Transfer cycle.
 * 
 * @algo
 *      (1) if self_id == src then (2)
 *          else if self_id == dst then (3)
 *          else (4)
 *      (2) Decrease balance and send TRANSFER to dst.
 *      (3) Increase balance and send ASK to PARENT_ID.
 *      (4) Do nothing.
 */
static void
transfer_handle(proc_t *p, BalanceHistory *history, TransferOrder *order) {

    if (p->self_id == order->s_src) {
        set_balance(history, -order->s_amount);
        send_transfer(p, order);
        fprintf(p->io->events_log_stream, log_transfer_out_fmt,
                get_physical_time(), p->self_id,
                order->s_amount, order->s_dst);

    } else if (p->self_id == order->s_dst) {
        set_balance(history, order->s_amount);
        fprintf(p->io->events_log_stream, log_transfer_in_fmt,
                get_physical_time(), p->self_id,
                order->s_amount, order->s_dst);
        send_ack(p);
    } else {
        fprintf(p->io->events_log_stream, "ID %d got mesasge for %d and %d.\n",
                p->self_id, order->s_src, order->s_dst);
    }
    
}

static void
work(proc_t *p, BalanceHistory *history) {
    while (true) {
        Message msg = {{ 0 }};
        if (receive_any(p, &msg) < 0)
            continue;

        switch (msg.s_header.s_type) {
            case TRANSFER: {
                fprintf(p->io->pipes_log_stream,
                        "ID %d got TRANSFER message\n",
                        p->self_id);
                transfer_handle(p, history, (TransferOrder*)(msg.s_payload));
                break;
            }
            case STOP:
                fprintf(p->io->pipes_log_stream,
                        "ID %d got STOP message.\n",
                        p->self_id);
                /* Maybe, need to chack if there are any TRANSFER messages*/
                return;
            default: {
                fprintf(p->io->pipes_log_stream,
                        "ID %d got wrong message with type %d.\n",
                        p->self_id, msg.s_header.s_type);
                continue;
            }
        }
    }
}

/** Child main function.
 */ 
int
child(proc_t *p, balance_t balance) {

    char payload[MAX_PAYLOAD_LEN];
    size_t len;
    const IO *io = p->io;
    BalanceHistory history = {
        .s_id = p->self_id,
        .s_history_len = 0,
        .s_history = {{ 0 }}
    };
    timestamp_t t = get_physical_time();

    for (timestamp_t i = 0; i <= t; i++) {
        history.s_history[i] = (BalanceState) {
            .s_balance = balance,
            .s_time = i,
            .s_balance_pending_in = 0
        };
    }
    history.s_history_len = t+1;

    close_unsed_fds(p->io, p->self_id, p->procnum);
    /* Process starts. */

    len = sprintf(payload, log_started_fmt, 
                  get_physical_time(), p->self_id, getpid(),
                  getppid(), history.s_history[0].s_balance);

    fputs(payload, io->events_log_stream); 

    /* Proces sync with others. */
    sync_state(p, STARTED, payload, len);
    fprintf(io->events_log_stream, log_received_all_started_fmt,
            get_physical_time(), p->self_id);

    /* Work. */
    work(p, &history);

    /* Process's done. */
    uint8_t hl  = history.s_history_len;
    balance_t b = history.s_history[hl-1].s_balance;
    len = sprintf(payload, log_done_fmt, get_physical_time(), p->self_id, b);
    fputs(payload, io->events_log_stream); 

    /* Process syncs wih ohers. */
    sync_state(p, DONE, payload, len);
    fprintf(io->events_log_stream, log_received_all_done_fmt, 
            get_physical_time(), p->self_id);

#ifdef DEBUG
    for (int i = 0; i < history.s_history_len; i++) 
        fprintf(stderr, "ID %d end with balance[%d] %d\n", p->self_id, i, history.s_history[i].s_balance);
#endif

    t = get_physical_time();
    copy_range(&history, t);

    sync_state(p, BALANCE_HISTORY, (char*)&history, sizeof(history));
    return 0;
}
