#ifndef DLT_DAEMON_UNIX_SOCKET_H
#define DLT_DAEMON_UNIX_SOCKET_H

int mct_daemon_unix_socket_open(int *sock, char *socket_path, int type, int mask);
int mct_daemon_unix_socket_close(int sock);

#endif /* DLT_DAEMON_UNIX_SOCKET_H */
