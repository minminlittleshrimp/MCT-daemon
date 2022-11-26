#ifndef MCT_COMMON_API_H
#define MCT_COMMON_API_H

#include "mct.h"

/**
 * Create an object for a new context.
 * Common API with MCT Embedded
 * This macro has to be called first for every.
 * @param CONTEXT object containing information about one special logging context
 */
/* #define MCT_DECLARE_CONTEXT(CONTEXT) */
/* UNCHANGED */

/**
 * Use an object of a new context created in another module.
 * Common API with MCT Embedded
 * This macro has to be called first for every.
 * @param CONTEXT object containing information about one special logging context
 */
/* #define MCT_IMPORT_CONTEXT(CONTEXT) */
/* UNCHANGED */

/**
 * Register application.
 * Common API with MCT Embedded
 * @param APPID application id with maximal four characters
 * @param DESCRIPTION ASCII string containing description
 */
/* #define MCT_REGISTER_APP(APPID,DESCRIPTION) */
/* UNCHANGED */

/**
 * Register context including application (with default log level and default trace status)
 * Common API with MCT Embedded
 * @param CONTEXT object containing information about one special logging context
 * @param CONTEXTID context id with maximal four characters
 * @param APPID context id with maximal four characters
 * @param DESCRIPTION ASCII string containing description
 */
#define MCT_REGISTER_CONTEXT_APP(CONTEXT, CONTEXTID, APPID, DESCRIPTION) \
    MCT_REGISTER_CONTEXT(CONTEXT, CONTEXTID, DESCRIPTION)

/**
 * Send log message with variable list of messages (intended for verbose mode)
 * Common API with MCT Embedded
 * @param CONTEXT object containing information about one special logging context
 * @param LOGLEVEL the log level of the log message
 * @param ARGS variable list of arguments
 */
/*****************************************/
#define MCT_LOG0(CONTEXT, LOGLEVEL) \
    MCT_LOG(CONTEXT, LOGLEVEL)
/*****************************************/
#define MCT_LOG1(CONTEXT, LOGLEVEL, ARGS1) \
    MCT_LOG(CONTEXT, LOGLEVEL, ARGS1)
/*****************************************/
#define MCT_LOG2(CONTEXT, LOGLEVEL, ARGS1, ARGS2) \
    MCT_LOG(CONTEXT, LOGLEVEL, ARGS1, ARGS2)
/*****************************************/
#define MCT_LOG3(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3) \
    MCT_LOG(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3)
/*****************************************/
#define MCT_LOG4(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4) \
    MCT_LOG(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4)
/*****************************************/
#define MCT_LOG5(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5) \
    MCT_LOG(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5)
/*****************************************/
#define MCT_LOG6(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6) \
    MCT_LOG(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6)
/*****************************************/
#define MCT_LOG7(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7) \
    MCT_LOG(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7)
/*****************************************/
#define MCT_LOG8(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8) \
    MCT_LOG(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8)
/*****************************************/
#define MCT_LOG9(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8, ARGS9) \
    MCT_LOG(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8, ARGS9)
/*****************************************/
#define MCT_LOG10(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8, ARGS9, ARGS10) \
    MCT_LOG(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8, ARGS9, ARGS10)
/*****************************************/
#define MCT_LOG11(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8, ARGS9, ARGS10, ARGS11) \
    MCT_LOG(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8, ARGS9, ARGS10, ARGS11)
/*****************************************/
#define MCT_LOG12(CONTEXT, \
                  LOGLEVEL, \
                  ARGS1, \
                  ARGS2, \
                  ARGS3, \
                  ARGS4, \
                  ARGS5, \
                  ARGS6, \
                  ARGS7, \
                  ARGS8, \
                  ARGS9, \
                  ARGS10, \
                  ARGS11, \
                  ARGS12) \
    MCT_LOG(CONTEXT, LOGLEVEL, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8, ARGS9, ARGS10, ARGS11, ARGS12)
/*****************************************/
#define MCT_LOG13(CONTEXT, \
                  LOGLEVEL, \
                  ARGS1, \
                  ARGS2, \
                  ARGS3, \
                  ARGS4, \
                  ARGS5, \
                  ARGS6, \
                  ARGS7, \
                  ARGS8, \
                  ARGS9, \
                  ARGS10, \
                  ARGS11, \
                  ARGS12, \
                  ARGS13) \
    MCT_LOG(CONTEXT, \
            LOGLEVEL, \
            ARGS1, \
            ARGS2, \
            ARGS3, \
            ARGS4, \
            ARGS5, \
            ARGS6, \
            ARGS7, \
            ARGS8, \
            ARGS9, \
            ARGS10, \
            ARGS11, \
            ARGS12, \
            ARGS13)
/*****************************************/
#define MCT_LOG14(CONTEXT, \
                  LOGLEVEL, \
                  ARGS1, \
                  ARGS2, \
                  ARGS3, \
                  ARGS4, \
                  ARGS5, \
                  ARGS6, \
                  ARGS7, \
                  ARGS8, \
                  ARGS9, \
                  ARGS10, \
                  ARGS11, \
                  ARGS12, \
                  ARGS13, \
                  ARGS14) \
    MCT_LOG(CONTEXT, \
            LOGLEVEL, \
            ARGS1, \
            ARGS2, \
            ARGS3, \
            ARGS4, \
            ARGS5, \
            ARGS6, \
            ARGS7, \
            ARGS8, \
            ARGS9, \
            ARGS10, \
            ARGS11, \
            ARGS12, \
            ARGS13, \
            ARGS14)
/*****************************************/
#define MCT_LOG15(CONTEXT, \
                  LOGLEVEL, \
                  ARGS1, \
                  ARGS2, \
                  ARGS3, \
                  ARGS4, \
                  ARGS5, \
                  ARGS6, \
                  ARGS7, \
                  ARGS8, \
                  ARGS9, \
                  ARGS10, \
                  ARGS11, \
                  ARGS12, \
                  ARGS13, \
                  ARGS14, \
                  ARGS15) \
    MCT_LOG(CONTEXT, \
            LOGLEVEL, \
            ARGS1, \
            ARGS2, \
            ARGS3, \
            ARGS4, \
            ARGS5, \
            ARGS6, \
            ARGS7, \
            ARGS8, \
            ARGS9, \
            ARGS10, \
            ARGS11, \
            ARGS12, \
            ARGS13, \
            ARGS14, \
            ARGS15)
/*****************************************/
#define MCT_LOG16(CONTEXT, \
                  LOGLEVEL, \
                  ARGS1, \
                  ARGS2, \
                  ARGS3, \
                  ARGS4, \
                  ARGS5, \
                  ARGS6, \
                  ARGS7, \
                  ARGS8, \
                  ARGS9, \
                  ARGS10, \
                  ARGS11, \
                  ARGS12, \
                  ARGS13, \
                  ARGS14, \
                  ARGS15, \
                  ARGS16) \
    MCT_LOG(CONTEXT, \
            LOGLEVEL, \
            ARGS1, \
            ARGS2, \
            ARGS3, \
            ARGS4, \
            ARGS5, \
            ARGS6, \
            ARGS7, \
            ARGS8, \
            ARGS9, \
            ARGS10, \
            ARGS11, \
            ARGS12, \
            ARGS13, \
            ARGS14, \
            ARGS15, \
            ARGS16)

/**
 * Send log message with variable list of messages (intended for non-verbose mode)
 * Common API with MCT Embedded
 * @param CONTEXT object containing information about one special logging context
 * @param LOGLEVEL the log level of the log message
 * @param MSGID the message id of log message
 * @param ARGS variable list of arguments:
 * calls to MCT_STRING(), MCT_BOOL(), MCT_FLOAT32(), MCT_FLOAT64(),
 * MCT_INT(), MCT_UINT(), MCT_RAW()
 */
/*****************************************/
#define MCT_LOG_ID0(CONTEXT, LOGLEVEL, MSGID) \
    MCT_LOG_ID(CONTEXT, LOGLEVEL, MSGID)
/*****************************************/
#define MCT_LOG_ID1(CONTEXT, LOGLEVEL, MSGID, ARGS1) \
    MCT_LOG_ID(CONTEXT, LOGLEVEL, MSGID, ARGS1)
/*****************************************/
#define MCT_LOG_ID2(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2) \
    MCT_LOG_ID(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2)
/*****************************************/
#define MCT_LOG_ID3(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3) \
    MCT_LOG_ID(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3)
/*****************************************/
#define MCT_LOG_ID4(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3, ARGS4) \
    MCT_LOG_ID(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3, ARGS4)
/*****************************************/
#define MCT_LOG_ID5(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5) \
    MCT_LOG_ID(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5)
/*****************************************/
#define MCT_LOG_ID6(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6) \
    MCT_LOG_ID(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6)
/*****************************************/
#define MCT_LOG_ID7(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7) \
    MCT_LOG_ID(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7)
/*****************************************/
#define MCT_LOG_ID8(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8) \
    MCT_LOG_ID(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8)
/*****************************************/
#define MCT_LOG_ID9(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8, ARGS9) \
    MCT_LOG_ID(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8, ARGS9)
/*****************************************/
#define MCT_LOG_ID10(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8, ARGS9, ARGS10) \
    MCT_LOG_ID(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8, ARGS9, ARGS10)
/*****************************************/
#define MCT_LOG_ID11(CONTEXT, \
                     LOGLEVEL, \
                     MSGID, \
                     ARGS1, \
                     ARGS2, \
                     ARGS3, \
                     ARGS4, \
                     ARGS5, \
                     ARGS6, \
                     ARGS7, \
                     ARGS8, \
                     ARGS9, \
                     ARGS10, \
                     ARGS11) \
    MCT_LOG_ID(CONTEXT, LOGLEVEL, MSGID, ARGS1, ARGS2, ARGS3, ARGS4, ARGS5, ARGS6, ARGS7, ARGS8, ARGS9, ARGS10, ARGS11)
/*****************************************/
#define MCT_LOG_ID12(CONTEXT, \
                     LOGLEVEL, \
                     MSGID, \
                     ARGS1, \
                     ARGS2, \
                     ARGS3, \
                     ARGS4, \
                     ARGS5, \
                     ARGS6, \
                     ARGS7, \
                     ARGS8, \
                     ARGS9, \
                     ARGS10, \
                     ARGS11, \
                     ARGS12) \
    MCT_LOG_ID(CONTEXT, \
               LOGLEVEL, \
               MSGID, \
               ARGS1, \
               ARGS2, \
               ARGS3, \
               ARGS4, \
               ARGS5, \
               ARGS6, \
               ARGS7, \
               ARGS8, \
               ARGS9, \
               ARGS10, \
               ARGS11, \
               ARGS12)
/*****************************************/
#define MCT_LOG_ID13(CONTEXT, \
                     LOGLEVEL, \
                     MSGID, \
                     ARGS1, \
                     ARGS2, \
                     ARGS3, \
                     ARGS4, \
                     ARGS5, \
                     ARGS6, \
                     ARGS7, \
                     ARGS8, \
                     ARGS9, \
                     ARGS10, \
                     ARGS11, \
                     ARGS12, \
                     ARGS13) \
    MCT_LOG_ID(CONTEXT, \
               LOGLEVEL, \
               MSGID, \
               ARGS1, \
               ARGS2, \
               ARGS3, \
               ARGS4, \
               ARGS5, \
               ARGS6, \
               ARGS7, \
               ARGS8, \
               ARGS9, \
               ARGS10, \
               ARGS11, \
               ARGS12, \
               ARGS13)
/*****************************************/
#define MCT_LOG_ID14(CONTEXT, \
                     LOGLEVEL, \
                     MSGID, \
                     ARGS1, \
                     ARGS2, \
                     ARGS3, \
                     ARGS4, \
                     ARGS5, \
                     ARGS6, \
                     ARGS7, \
                     ARGS8, \
                     ARGS9, \
                     ARGS10, \
                     ARGS11, \
                     ARGS12, \
                     ARGS13, \
                     ARGS14) \
    MCT_LOG_ID(CONTEXT, \
               LOGLEVEL, \
               MSGID, \
               ARGS1, \
               ARGS2, \
               ARGS3, \
               ARGS4, \
               ARGS5, \
               ARGS6, \
               ARGS7, \
               ARGS8, \
               ARGS9, \
               ARGS10, \
               ARGS11, \
               ARGS12, \
               ARGS13, \
               ARGS14)
/*****************************************/
#define MCT_LOG_ID15(CONTEXT, \
                     LOGLEVEL, \
                     MSGID, \
                     ARGS1, \
                     ARGS2, \
                     ARGS3, \
                     ARGS4, \
                     ARGS5, \
                     ARGS6, \
                     ARGS7, \
                     ARGS8, \
                     ARGS9, \
                     ARGS10, \
                     ARGS11, \
                     ARGS12, \
                     ARGS13, \
                     ARGS14, \
                     ARGS15) \
    MCT_LOG_ID(CONTEXT, \
               LOGLEVEL, \
               MSGID, \
               ARGS1, \
               ARGS2, \
               ARGS3, \
               ARGS4, \
               ARGS5, \
               ARGS6, \
               ARGS7, \
               ARGS8, \
               ARGS9, \
               ARGS10, \
               ARGS11, \
               ARGS12, \
               ARGS13, \
               ARGS14, \
               ARGS15)
/*****************************************/
#define MCT_LOG_ID16(CONTEXT, \
                     LOGLEVEL, \
                     MSGID, \
                     ARGS1, \
                     ARGS2, \
                     ARGS3, \
                     ARGS4, \
                     ARGS5, \
                     ARGS6, \
                     ARGS7, \
                     ARGS8, \
                     ARGS9, \
                     ARGS10, \
                     ARGS11, \
                     ARGS12, \
                     ARGS13, \
                     ARGS14, \
                     ARGS15, \
                     ARGS16) \
    MCT_LOG_ID(CONTEXT, \
               LOGLEVEL, \
               MSGID, \
               ARGS1, \
               ARGS2, \
               ARGS3, \
               ARGS4, \
               ARGS5, \
               ARGS6, \
               ARGS7, \
               ARGS8, \
               ARGS9, \
               ARGS10, \
               ARGS11, \
               ARGS12, \
               ARGS13, \
               ARGS14, \
               ARGS15, \
               ARGS16)

/**
 * Unregister context.
 * Common API with MCT Embedded
 * @param CONTEXT object containing information about one special logging context
 */
/* #define MCT_UNREGISTER_CONTEXT(CONTEXT) */
/* UNCHANGED */

/**
 * Unregister application.
 * Common API with MCT Embedded
 */
/* #define MCT_UNREGISTER_APP() */
/* UNCHANGED */

/**
 * Add string parameter to the log messsage.
 * Common API with MCT Embedded
 * In the future in none verbose mode the string will not be sent via MCT message.
 * @param TEXT ASCII string
 */
/* #define MCT_CSTRING(TEXT) */
/* UNCHANGED */

#endif /* MCT_COMMON_API_H */

