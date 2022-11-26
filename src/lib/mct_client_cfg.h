#ifndef MCT_CLIENT_CFG_H
#define MCT_CLIENT_CFG_H

/*************/
/* Changable */
/*************/

/* Dummy application id of MCT client */
#define MCT_CLIENT_DUMMY_APP_ID "CA1"

/* Dummy context id of MCT client */
#define MCT_CLIENT_DUMMY_CON_ID "CC1"

/* Size of buffer */
#define MCT_CLIENT_TEXTBUFSIZE          512

/* Initial baudrate */
#if !defined (__WIN32__) && !defined(_MSC_VER)
#define MCT_CLIENT_INITIAL_BAUDRATE B115200
#else
#define MCT_CLIENT_INITIAL_BAUDRATE 0
#endif

/* Name of environment variable for specifying the daemon port */
#define MCT_CLIENT_ENV_DAEMON_TCP_PORT "MCT_DAEMON_TCP_PORT"

/************************/
/* Don't change please! */
/************************/

#endif /* MCT_CLIENT_CFG_H */
