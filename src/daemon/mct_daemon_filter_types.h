#ifndef _DLT_DAEMON_FILTER_TYPES_H
#define _DLT_DAEMON_FILTER_TYPES_H

#include "mct_config_file_parser.h"
#include "mct_daemon_connection_types.h"
#include "mct_protocol.h"
#include "mct_common.h"

#define DLT_FILTER_CONFIG_FILE CONFIGURATION_FILES_DIR "/mct_message_filter.conf"

#define DLT_FILTER_BASIC_CONTROL_MESSAGE_MASK 0xCFFFFFFF

/* whatever connection always available: timers, msg, unix socket control (, app) */
#define DLT_FILTER_CLIENT_CONNECTION_DEFAULT_MASK ( \
        DLT_CON_MASK_APP_MSG | \
        DLT_CON_MASK_APP_CONNECT | \
        DLT_CON_MASK_ONE_S_TIMER | \
        DLT_CON_MASK_SIXTY_S_TIMER | \
        DLT_CON_MASK_SYSTEMD_TIMER | \
        DLT_CON_MASK_CONTROL_CONNECT | \
        DLT_CON_MASK_CONTROL_MSG)

#define DLT_FILTER_CLIENT_ALL       "ALL"
#define DLT_FILTER_CLIENT_NONE      "NONE"

#define DLT_FILTER_LEVEL_MIN                0
#define DLT_FILTER_LEVEL_MAX                100
#define DLT_FILTER_INJECTION_CONFIG_MAX     50

/* get bit at position n */
#define BIT(x, n) (((x) >> (n)) & 1)
/* set bit at position n */
#define SET_BIT(x, n) ((x) |= (1 << n))

/* Structure defines flags for AUTOSAR Service IDs and user defined Service IDs.
 * The services ID are defined in mct_protocol.h
 */
typedef struct
{
    unsigned char upper[DLT_NUM_USER_SERVICE_ID]; /* 0xF01..DLT_SERVICE_ID_USER_LAST_ENTRY */
    unsigned char lower[DLT_NUM_SERVICE_ID];      /* 0x01..DLT_SERVICE_ID_LAST_ENTRY */
} DltServiceIdFlag;

typedef struct
{
    char *name;         /* name of injection */
    char *apid;         /* application identifier */
    char *ctid;         /* context identifier */
    char *ecuid;        /* node identifier */
    int num_sevice_ids; /* number of service ids */
    int *service_ids;   /* list of service ids */
} DltInjectionConfig;

typedef struct DltFilterConfiguration
{
    char *name;                          /* name of filter configuration */
    unsigned int level_min;              /* minimum level id of filter configuration*/
    unsigned int level_max;              /* maximum level id of filter configuration */
    unsigned int client_mask;            /* Mask of allowed clients */
    DltServiceIdFlag ctrl_mask;          /* Mask of allowed control messages */
    char **injections;                   /* list of injection messages names */
    int num_injections;                  /* number of injections */
    struct DltFilterConfiguration *next; /* for multiple filter configuration
                                          * using linked list */
} DltFilterConfiguration;

typedef struct
{
    char *name;                 /* name of filter configuration */
    char *backend;              /* name of filter control backend */
    unsigned int default_level; /* default level of message filter */
    DltFilterConfiguration *configs;
    DltFilterConfiguration *current;                                /* active configuration */
    DltInjectionConfig injections[DLT_FILTER_INJECTION_CONFIG_MAX]; /* list of injections */
    int num_injections;                                             /* number of specified injections */
} DltMessageFilter;

#endif
