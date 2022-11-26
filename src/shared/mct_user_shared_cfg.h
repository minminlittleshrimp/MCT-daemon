#ifndef DLT_USER_SHARED_CFG_H
#define DLT_USER_SHARED_CFG_H

/*************/
/* Changable */
/*************/

/************************/
/* Don't change please! */
/************************/

/* The different types of internal messages between user application and daemon. */
#define DLT_USER_MESSAGE_LOG 1
#define DLT_USER_MESSAGE_REGISTER_APPLICATION 2
#define DLT_USER_MESSAGE_UNREGISTER_APPLICATION 3
#define DLT_USER_MESSAGE_REGISTER_CONTEXT 4
#define DLT_USER_MESSAGE_UNREGISTER_CONTEXT 5
#define DLT_USER_MESSAGE_LOG_LEVEL 6
#define DLT_USER_MESSAGE_INJECTION 7
#define DLT_USER_MESSAGE_OVERFLOW 8
#define DLT_USER_MESSAGE_APP_LL_TS 9
#define DLT_USER_MESSAGE_LOG_SHM 10
#define DLT_USER_MESSAGE_LOG_MODE 11
#define DLT_USER_MESSAGE_LOG_STATE 12
#define DLT_USER_MESSAGE_MARKER 13
#define DLT_USER_MESSAGE_SET_BLOCK_MODE 14
#define DLT_USER_MESSAGE_GET_BLOCK_MODE 15
#define DLT_USER_MESSAGE_NOT_SUPPORTED 16

/* Internal defined values */

/* must be different from DltLogLevelType */
#define DLT_USER_LOG_LEVEL_NOT_SET    -2
/* must be different from DltTraceStatusType */
#define DLT_USER_TRACE_STATUS_NOT_SET -2

#endif /* DLT_USER_SHARED_CFG_H */

