#ifndef DLT_DAEMON_COMMON_CFG_H
#define DLT_DAEMON_COMMON_CFG_H

/*************/
/* Changable */
/*************/


/* Default Path for runtime configuration */
#define DLT_RUNTIME_DEFAULT_DIRECTORY "/tmp"
/* Path and filename for runtime configuration (applications) */
#define DLT_RUNTIME_APPLICATION_CFG "/mct-runtime-application.cfg"
/* Path and filename for runtime configuration (contexts) */
#define DLT_RUNTIME_CONTEXT_CFG     "/mct-runtime-context.cfg"
/* Path and filename for runtime configuration */
#define DLT_RUNTIME_CONFIGURATION     "/mct-runtime.cfg"

/* Default Path for control socket */
#define DLT_DAEMON_DEFAULT_CTRL_SOCK_PATH DLT_RUNTIME_DEFAULT_DIRECTORY \
    "/mct-ctrl.sock"

#ifdef DLT_DAEMON_USE_UNIX_SOCKET_IPC
#define DLT_DAEMON_DEFAULT_APP_SOCK_PATH DLT_RUNTIME_DEFAULT_DIRECTORY \
    "/mct-app.sock"
#endif

/* Size of text buffer */
#define DLT_DAEMON_COMMON_TEXTBUFSIZE          255

/* Application ID used when the mct daemon creates a control message */
#define DLT_DAEMON_CTRL_APID         "DA1"
/* Context ID used when the mct daemon creates a control message */
#define DLT_DAEMON_CTRL_CTID         "DC1"

/* Number of entries to be allocated at one in application table,
 * when no more entries are available */
#define DLT_DAEMON_APPL_ALLOC_SIZE      500
/* Number of entries to be allocated at one in context table,
 * when no more entries are available */
#define DLT_DAEMON_CONTEXT_ALLOC_SIZE  1000

/* Debug get log info function,
 * set to 1 to enable, 0 to disable debugging */
#define DLT_DEBUG_GETLOGINFO 0

/************************/
/* Don't change please! */
/************************/

/* Minimum ID for an injection message */
#define DLT_DAEMON_INJECTION_MIN      0xFFF
/* Maximum ID for an injection message */
#define DLT_DAEMON_INJECTION_MAX 0xFFFFFFFF

/* Remote interface identifier */
#define DLT_DAEMON_REMO_STRING "remo"

#endif /* DLT_DAEMON_COMMON_CFG_H */
