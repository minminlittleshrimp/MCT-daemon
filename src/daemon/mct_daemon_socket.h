#ifndef DLT_DAEMON_SOCKET_H
#define DLT_DAEMON_SOCKET_H

#include <limits.h>
#include <semaphore.h>
#include "mct_common.h"
#include "mct_user.h"

int mct_daemon_socket_open(int *sock, unsigned int servPort, char *ip);
int mct_daemon_socket_close(int sock);

int mct_daemon_socket_get_send_qeue_max_size(int sock);

int mct_daemon_socket_send(int sock,
                           void *data1,
                           int size1,
                           void *data2,
                           int size2,
                           char serialheader);

/**
 * @brief mct_daemon_socket_sendreliable - sends data to socket with additional checks and resending functionality - trying to be reliable
 * @param sock
 * @param data_buffer
 * @param message_size
 * @return on sucess: DLT_DAEMON_ERROR_OK, on error: DLT_DAEMON_ERROR_SEND_FAILED
 */
int mct_daemon_socket_sendreliable(int sock, void *data_buffer, int message_size);

#endif /* DLT_DAEMON_SOCKET_H */
