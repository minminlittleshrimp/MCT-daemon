#ifndef MCT_USER_SHARED_H
#define MCT_USER_SHARED_H

#include "mct_types.h"
#include "mct_user.h"

#include <sys/types.h>

/**
 * This is the header of each message to be exchanged between application and daemon.
 */
typedef struct
{
    char pattern[MCT_ID_SIZE];      /**< This pattern should be DUH0x01 */
    uint32_t message;               /**< messsage info */
} MCT_PACKED MctUserHeader;

/**
 * This is the internal message content to exchange control msg register app information between application and daemon.
 */
typedef struct
{
    char apid[MCT_ID_SIZE];          /**< application id */
    pid_t pid;                       /**< process id of user application */
    uint32_t description_length;     /**< length of description */
} MCT_PACKED MctUserControlMsgRegisterApplication;

/**
 * This is the internal message content to exchange control msg unregister app information between application and daemon.
 */
typedef struct
{
    char apid[MCT_ID_SIZE];         /**< application id */
    pid_t pid;                      /**< process id of user application */
} MCT_PACKED MctUserControlMsgUnregisterApplication;

/**
 * This is the internal message content to exchange control msg register information between application and daemon.
 */
typedef struct
{
    char apid[MCT_ID_SIZE];          /**< application id */
    char ctid[MCT_ID_SIZE];          /**< context id */
    int32_t log_level_pos;           /**< offset in management structure on user-application side */
    int8_t log_level;                /**< log level */
    int8_t trace_status;             /**< trace status */
    pid_t pid;                       /**< process id of user application */
    uint32_t description_length;     /**< length of description */
} MCT_PACKED MctUserControlMsgRegisterContext;

/**
 * This is the internal message content to exchange control msg unregister information between application and daemon.
 */
typedef struct
{
    char apid[MCT_ID_SIZE];         /**< application id */
    char ctid[MCT_ID_SIZE];         /**< context id */
    pid_t pid;                      /**< process id of user application */
} MCT_PACKED MctUserControlMsgUnregisterContext;

/**
 * This is the internal message content to exchange control msg log level information between application and daemon.
 */
typedef struct
{
    uint8_t log_level;             /**< log level */
    uint8_t trace_status;          /**< trace status */
    int32_t log_level_pos;          /**< offset in management structure on user-application side */
} MCT_PACKED MctUserControlMsgLogLevel;

/**
 * This is the internal message content to exchange control msg injection information between application and daemon.
 */
typedef struct
{
    int32_t log_level_pos;          /**< offset in management structure on user-application side */
    uint32_t service_id;            /**< service id of injection */
    uint32_t data_length_inject;    /**< length of injection message data field */
} MCT_PACKED MctUserControlMsgInjection;

/**
 * This is the internal message content to exchange information about application log level and trace stats between
 * application and daemon.
 */
typedef struct
{
    char apid[MCT_ID_SIZE];        /**< application id */
    uint8_t log_level;             /**< log level */
    uint8_t trace_status;          /**< trace status */
} MCT_PACKED MctUserControlMsgAppLogLevelTraceStatus;

/**
 * This is the internal message content to set the logging mode: off, external, internal, both.
 */
typedef struct
{
    int8_t log_mode;          /**< the mode to be used for logging: off, external, internal, both */
} MCT_PACKED MctUserControlMsgLogMode;

/**
 * This is the internal message content to get the logging state: 0 = off, 1 = external client connected.
 */
typedef struct
{
    int8_t log_state;          /**< the state to be used for logging state: 0 = off, 1 = external client connected */
} MCT_PACKED MctUserControlMsgLogState;

/**
 * This is the internal message content to get the number of lost messages reported to the daemon.
 */
typedef struct
{
    uint32_t overflow_counter;          /**< counts the number of lost messages */
    char apid[4];                        /**< application which lost messages */
} MCT_PACKED MctUserControlMsgBufferOverflow;

/**
 * This is the internal message content to get the sending the number of bytes of messages discarded when PIPE and BUFFER Full.
 */
typedef struct
{
        int8_t block_mode;
} MCT_PACKED MctUserControlMsgBlockMode;

/**************************************************************************************************
* The folowing functions are used shared between the user lib and the daemon implementation
**************************************************************************************************/

/**
 * Set user header marker and store message type in user header
 * @param userheader pointer to the userheader
 * @param mtype user message type of internal message
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_user_set_userheader(MctUserHeader *userheader, uint32_t mtype);

/**
 * Check if user header contains its marker
 * @param userheader pointer to the userheader
 * @return 0 no, 1 yes, negative value if there was an error
 */
int mct_user_check_userheader(MctUserHeader *userheader);

/**
 * Atomic write to file descriptor, using vector of 2 elements
 * @param handle file descriptor
 * @param ptr1 generic pointer to first segment of data to be written
 * @param len1 length of first segment of data to be written
 * @param ptr2 generic pointer to second segment of data to be written
 * @param len2 length of second segment of data to be written
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_user_log_out2(int handle, void *ptr1, size_t len1, void *ptr2, size_t len2);

/**
 * Atomic write to file descriptor, using vector of 3 elements
 * @param handle file descriptor
 * @param ptr1 generic pointer to first segment of data to be written
 * @param len1 length of first segment of data to be written
 * @param ptr2 generic pointer to second segment of data to be written
 * @param len2 length of second segment of data to be written
 * @param ptr3 generic pointer to third segment of data to be written
 * @param len3 length of third segment of data to be written
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_user_log_out3(int handle, void *ptr1, size_t len1, void *ptr2, size_t len2, void *ptr3, size_t len3);

#endif /* MCT_USER_SHARED_H */
