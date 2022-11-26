#ifndef DLT_CLIENT_CFG_H
#define DLT_CLIENT_CFG_H

/*************/
/* Changable */
/*************/

/* Dummy application id of DLT client */
#define DLT_CLIENT_DUMMY_APP_ID "CA1"

/* Dummy context id of DLT client */
#define DLT_CLIENT_DUMMY_CON_ID "CC1"

/* Size of buffer */
#define DLT_CLIENT_TEXTBUFSIZE          512

/* Initial baudrate */
#if !defined (__WIN32__) && !defined(_MSC_VER)
#define DLT_CLIENT_INITIAL_BAUDRATE B115200
#else
#define DLT_CLIENT_INITIAL_BAUDRATE 0
#endif

/* Name of environment variable for specifying the daemon port */
#define DLT_CLIENT_ENV_DAEMON_TCP_PORT "DLT_DAEMON_TCP_PORT"

/************************/
/* Don't change please! */
/************************/

#endif /* DLT_CLIENT_CFG_H */
