#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "io.h"
#include "ipc.h"
#include "child.h"


/** Creates pipes for IPC.
 * 
 * @param io    Metadata to perfom I/O.
 * @param procnum  Current process number including parent.
 */
int init_pipes(IO *io, local_id procnum) {
    int count = 1;
    for (int i = 0; i <= procnum; i++) {
        for (int j = 0; j <= procnum; j++) {
            if (i == j) {
                io->fds[i][j][READ_FD]  = -1;
                io->fds[i][j][WRITE_FD] = -1;
                continue;
            }
            fprintf(io->pipes_log_stream, "Pipe with number %d was created.\n", count++);

            if (pipe2(io->fds[i][j], O_NONBLOCK | O_DIRECT) < 0) {
               perror("pipe:init_pipes() io.c");
               return -1;
            }
        }
    }
    return 0;
}

/** Close unused file descriptors.
 * 
 * @param io    Metadata to perfom I/O; includes pipes.
 * @param id    ID of the child process.
 */ 
void close_fds(const IO *io, local_id id, local_id procnum) {
    for (local_id i = 0; i <= procnum; i++) {
        for (local_id j = 0; j <= procnum; j++) {
            if (i != j) {
                if (i == id) {
                    close(io->fds[i][j][READ_FD]);
                    fprintf(io->pipes_log_stream, 
                            "PID:%d closed read(%hhd -- %hhd).\n", id, i,j);
                }
                if (j == id) {
                    close(io->fds[i][j][WRITE_FD]);
                    fprintf(io->pipes_log_stream,
                            "PID:%d closed write(%hhd -- %hhd).\n", id, i,j);
                }
                if (i != id && j != id) {
                    fprintf(io->pipes_log_stream,
                            "PID:%d closed pipe(%hhd -- %hhd).\n", id, i,j);
                    close(io->fds[i][j][WRITE_FD]);
                    close(io->fds[i][j][READ_FD]);
                }
            }
        }
    }
    fprintf(io->pipes_log_stream, "PID:%d closed all fds.\n", id);
}

