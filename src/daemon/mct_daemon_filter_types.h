#ifndef _MCT_DAEMON_FILTER_TYPES_H
#define _MCT_DAEMON_FILTER_TYPES_H

#include "mct_config_file_parser.h"
#include "mct_daemon_connection_types.h"
#include "mct_protocol.h"
#include "mct_common.h"

#define MCT_FILTER_CONFIG_FILE CONFIGURATION_FILES_DIR "/mct_message_filter.conf"

#define MCT_FILTER_BASIC_CONTROL_MESSAGE_MASK 0xCFFFFFFF

/* whatever connection always available: timers, msg, unix socket control (, app) */
#define MCT_FILTER_CLIENT_CONNECTION_DEFAULT_MASK ( \
        MCT_CON_MASK_APP_MSG | \
        MCT_CON_MASK_APP_CONNECT | \
        MCT_CON_MASK_ONE_S_TIMER | \
        MCT_CON_MASK_SIXTY_S_TIMER | \
        MCT_CON_MASK_SYSTEMD_TIMER | \
        MCT_CON_MASK_CONTROL_CONNECT | \
        MCT_CON_MASK_CONTROL_MSG)

#define MCT_FILTER_CLIENT_ALL       "ALL"
#define MCT_FILTER_CLIENT_NONE      "NONE"

#define MCT_FILTER_LEVEL_MIN                0
#define MCT_FILTER_LEVEL_MAX                100
#define MCT_FILTER_INJECTION_CONFIG_MAX     50

/* get bit at position n */
#define BIT(x, n) (((x) >> (n)) & 1)
/* set bit at position n */
#define SET_BIT(x, n) ((x) |= (1 << n))

/* Structure defines flags for AUTOSAR Service IDs and user defined Service IDs.
 * The services ID are defined in mct_protocol.h
 */
typedef struct
{
    unsigned char upper[MCT_NUM_USER_SERVICE_ID]; /* 0xF01..MCT_SERVICE_ID_USER_LAST_ENTRY */
    unsigned char lower[MCT_NUM_SERVICE_ID];      /* 0x01..MCT_SERVICE_ID_LAST_ENTRY */
} MctServiceIdFlag;

typedef struct
{
    char *name;         /* name of injection */
    char *apid;         /* application identifier */
    char *ctid;         /* context identifier */
    char *ecuid;        /* node identifier */
    int num_sevice_ids; /* number of service ids */
    int *service_ids;   /* list of service ids */
} MctInjectionConfig;

typedef struct MctFilterConfiguration
{
    char *name;                          /* name of filter configuration */
    unsigned int level_min;              /* minimum level id of filter configuration*/
    unsigned int level_max;              /* maximum level id of filter configuration */
    unsigned int client_mask;            /* Mask of allowed clients */
    MctServiceIdFlag ctrl_mask;          /* Mask of allowed control messages */
    char **injections;                   /* list of injection messages names */
    int num_injections;                  /* number of injections */
    struct MctFilterConfiguration *next; /* for multiple filter configuration
                                          * using linked list */
} MctFilterConfiguration;

typedef struct
{
    char *name;                 /* name of filter configuration */
    char *backend;              /* name of filter control backend */
    unsigned int default_level; /* default level of message filter */
    MctFilterConfiguration *configs;
    MctFilterConfiguration *current;                                /* active configuration */
    MctInjectionConfig injections[MCT_FILTER_INJECTION_CONFIG_MAX]; /* list of injections */
    int num_injections;                                             /* number of specified injections */
} MctMessageFilter;

#endif
