#ifndef MCT_DAEMON_COMMON_CFG_H
#define MCT_DAEMON_COMMON_CFG_H

/*************/
/* Changable */
/*************/


/* Default Path for runtime configuration */
#define MCT_RUNTIME_DEFAULT_DIRECTORY "/tmp"
/* Path and filename for runtime configuration (applications) */
#define MCT_RUNTIME_APPLICATION_CFG "/mct-runtime-application.cfg"
/* Path and filename for runtime configuration (contexts) */
#define MCT_RUNTIME_CONTEXT_CFG     "/mct-runtime-context.cfg"
/* Path and filename for runtime configuration */
#define MCT_RUNTIME_CONFIGURATION     "/mct-runtime.cfg"

/* Default Path for control socket */
#define MCT_DAEMON_DEFAULT_CTRL_SOCK_PATH MCT_RUNTIME_DEFAULT_DIRECTORY \
    "/mct-ctrl.sock"

#ifdef MCT_DAEMON_USE_UNIX_SOCKET_IPC
#define MCT_DAEMON_DEFAULT_APP_SOCK_PATH MCT_RUNTIME_DEFAULT_DIRECTORY \
    "/mct-app.sock"
#endif

/* Size of text buffer */
#define MCT_DAEMON_COMMON_TEXTBUFSIZE          255

/* Application ID used when the mct daemon creates a control message */
#define MCT_DAEMON_CTRL_APID         "DA1"
/* Context ID used when the mct daemon creates a control message */
#define MCT_DAEMON_CTRL_CTID         "DC1"

/* Number of entries to be allocated at one in application table,
 * when no more entries are available */
#define MCT_DAEMON_APPL_ALLOC_SIZE      500
/* Number of entries to be allocated at one in context table,
 * when no more entries are available */
#define MCT_DAEMON_CONTEXT_ALLOC_SIZE  1000

/* Debug get log info function,
 * set to 1 to enable, 0 to disable debugging */
#define MCT_DEBUG_GETLOGINFO 0

/************************/
/* Don't change please! */
/************************/

/* Minimum ID for an injection message */
#define MCT_DAEMON_INJECTION_MIN      0xFFF
/* Maximum ID for an injection message */
#define MCT_DAEMON_INJECTION_MAX 0xFFFFFFFF

/* Remote interface identifier */
#define MCT_DAEMON_REMO_STRING "remo"

#endif /* MCT_DAEMON_COMMON_CFG_H */
