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
 * @param pnum  Current process number including parent.
 */
int
create_pipes(IO *io, local_id pnum) {
    int count = 1;
    for (int i = 0; i <= pnum; i++) {
        for (int j = 0; j <= pnum; j++) {
            if (i == j) {
                io->fds[i][j][READ_FD]  = -1;
                io->fds[i][j][WRITE_FD] = -1;
                continue;
            }
            fprintf(io->pipes_log_stream, "Created pipe number %d.\n", count++);

            if (pipe2(io->fds[i][j], O_NONBLOCK | O_DIRECT) < 0) {
               perror("pipe");
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
void
close_unsed_fds(const IO *io, local_id id, local_id pnum) {
    for (local_id i = 0; i <= pnum; i++) {
        for (local_id j = 0; j <= pnum; j++) {
            if (i != j) {
                if (i == id) {
                    close(io->fds[i][j][READ_FD]);
                    fprintf(io->pipes_log_stream, 
                            "ID %d closes read(%hhd -- %hhd)\n", id, i,j);
                }

                if (j == id) {
                    close(io->fds[i][j][WRITE_FD]);
                    fprintf(io->pipes_log_stream,
                            "ID %d closes write(%hhd -- %hhd)\n", id, i,j);
                }

                if (i != id && j != id) {
                    fprintf(io->pipes_log_stream,
                            "ID %d closes pipe(%hhd -- %hhd)\n", id, i,j);
                    close(io->fds[i][j][WRITE_FD]);
                    close(io->fds[i][j][READ_FD]);
                }
            }
        }
    }

    fprintf(io->pipes_log_stream, "ID %d closes all fds.\n", id);
}

