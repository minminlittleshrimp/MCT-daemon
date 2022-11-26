#ifndef MCT_DAEMON_UNIX_SOCKET_H
#define MCT_DAEMON_UNIX_SOCKET_H

int mct_daemon_unix_socket_open(int *sock, char *socket_path, int type, int mask);
int mct_daemon_unix_socket_close(int sock);

#endif /* MCT_DAEMON_UNIX_SOCKET_H */
