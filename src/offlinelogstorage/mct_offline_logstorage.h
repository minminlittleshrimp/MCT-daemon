#ifndef MCT_OFFLINE_LOGSTORAGE_H
#define MCT_OFFLINE_LOGSTORAGE_H

#include <search.h>
#include <stdbool.h>
#include "mct_common.h"
#include "mct-daemon_cfg.h"
#include "mct_config_file_parser.h"

#define MCT_OFFLINE_LOGSTORAGE_MAXIDS               100 /* Maximum entries for each apids and ctids */
#define MCT_OFFLINE_LOGSTORAGE_MAX_POSSIBLE_KEYS   7  /* Max number of possible keys when searching for */

#define MCT_OFFLINE_LOGSTORAGE_INIT_DONE           1  /* For device configuration status */
#define MCT_OFFLINE_LOGSTORAGE_DEVICE_CONNECTED    1
#define MCT_OFFLINE_LOGSTORAGE_FREE                0
#define MCT_OFFLINE_LOGSTORAGE_DEVICE_DISCONNECTED 0
#define MCT_OFFLINE_LOGSTORAGE_CONFIG_DONE         1

#define MCT_OFFLINE_LOGSTORAGE_SYNC_CACHES         2  /* sync logstorage caches */

#define MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN         15  /* Maximum size for key */
#define MCT_OFFLINE_LOGSTORAGE_MAX_FILE_NAME_LEN   100 /* Maximum file name length of the log file including path under mount point */

#define MCT_OFFLINE_LOGSTORAGE_FILE_EXTENSION_LEN   4
#define MCT_OFFLINE_LOGSTORAGE_INDEX_LEN            3
#define MCT_OFFLINE_LOGSTORAGE_MAX_INDEX          999
#define MCT_OFFLINE_LOGSTORAGE_TIMESTAMP_LEN       16
#define MCT_OFFLINE_LOGSTORAGE_INDEX_OFFSET        (MCT_OFFLINE_LOGSTORAGE_TIMESTAMP_LEN + \
                                                    MCT_OFFLINE_LOGSTORAGE_FILE_EXTENSION_LEN + \
                                                    MCT_OFFLINE_LOGSTORAGE_INDEX_LEN)
#define MCT_OFFLINE_LOGSTORAGE_MAX_LOG_FILE_LEN    (MCT_OFFLINE_LOGSTORAGE_MAX_FILE_NAME_LEN + \
                                                    MCT_OFFLINE_LOGSTORAGE_TIMESTAMP_LEN + \
                                                    MCT_OFFLINE_LOGSTORAGE_INDEX_LEN + \
                                                    MCT_OFFLINE_LOGSTORAGE_FILE_EXTENSION_LEN + 1)

#define MCT_OFFLINE_LOGSTORAGE_CONFIG_FILE_NAME    "mct_logstorage.conf"

/* +3 because of device number and \0 */
#define MCT_OFFLINE_LOGSTORAGE_MAX_PATH_LEN (MCT_OFFLINE_LOGSTORAGE_MAX_LOG_FILE_LEN + \
                                             MCT_MOUNT_PATH_MAX + 3)

#define MCT_OFFLINE_LOGSTORAGE_MAX(A, B)   ((A) > (B) ? (A) : (B))
#define MCT_OFFLINE_LOGSTORAGE_MIN(A, B)   ((A) < (B) ? (A) : (B))

#define MCT_OFFLINE_LOGSTORAGE_MAX_ERRORS           5
#define MCT_OFFLINE_LOGSTORAGE_MAX_KEY_NUM          8

#define MCT_OFFLINE_LOGSTORAGE_CONFIG_SECTION "FILTER"
#define MCT_OFFLINE_LOGSTORAGE_GENERAL_CONFIG_SECTION "GENERAL"
#define MCT_OFFLINE_LOGSTORAGE_NONVERBOSE_STORAGE_SECTION "NON-VERBOSE-STORAGE-FILTER"
#define MCT_OFFLINE_LOGSTORAGE_NONVERBOSE_CONTROL_SECTION "NON-VERBOSE-LOGLEVEL-CTRL"

/* Offline Logstorage sync strategies */
#define MCT_LOGSTORAGE_SYNC_ON_ERROR                  -1 /* error case */
#define MCT_LOGSTORAGE_SYNC_UNSET                     0  /* strategy not set */
#define MCT_LOGSTORAGE_SYNC_ON_MSG                    1 /* default, on message sync */
#define MCT_LOGSTORAGE_SYNC_ON_DAEMON_EXIT            (1 << 1) /* sync on daemon exit */
#define MCT_LOGSTORAGE_SYNC_ON_DEMAND                 (1 << 2) /* sync on demand */
#define MCT_LOGSTORAGE_SYNC_ON_DEVICE_DISCONNECT      (1 << 3) /* sync on device disconnect*/
#define MCT_LOGSTORAGE_SYNC_ON_SPECIFIC_SIZE          (1 << 4) /* sync on after specific size */
#define MCT_LOGSTORAGE_SYNC_ON_FILE_SIZE              (1 << 5) /* sync on file size reached */

#define MCT_OFFLINE_LOGSTORAGE_IS_STRATEGY_SET(S, s) ((S)&(s))

/* Offline Logstorage overwrite strategies */
#define MCT_LOGSTORAGE_OVERWRITE_ERROR         -1 /* error case */
#define MCT_LOGSTORAGE_OVERWRITE_UNSET          0 /* strategy not set */
#define MCT_LOGSTORAGE_OVERWRITE_DISCARD_OLD    1 /* default, discard old */
#define MCT_LOGSTORAGE_OVERWRITE_DISCARD_NEW   (1 << 1) /* discard new */

/* Offline Logstorage disable network routing */
#define MCT_LOGSTORAGE_DISABLE_NW_ERROR         -1 /* error case */
#define MCT_LOGSTORAGE_DISABLE_NW_UNSET          0 /* not set */
#define MCT_LOGSTORAGE_DISABLE_NW_OFF            1 /* default, enable network routing */
#define MCT_LOGSTORAGE_DISABLE_NW_ON            (1 << 1) /* disable network routing */

/* logstorage max cache */
extern unsigned int g_logstorage_cache_max;
/* current logstorage cache size */
extern unsigned int g_logstorage_cache_size;

typedef struct
{
    unsigned int offset;          /* current write offset */
    unsigned int wrap_around_cnt; /* wrap around counter */
    unsigned int last_sync_offset; /* last sync position */
    unsigned int end_sync_offset; /* end position of previous round */
} MctLogStorageCacheFooter;

typedef struct
{
    /* File name user configurations */
    int logfile_timestamp;              /* Timestamp set/reset */
    char logfile_delimiter;             /* Choice of delimiter */
    unsigned int logfile_maxcounter;    /* Maximum file index counter */
    unsigned int logfile_counteridxlen; /* File index counter length */
    int logfile_optional_counter;       /* Don't append counter for num_files=1 */
} MctLogStorageUserConfig;

typedef struct MctLogStorageFileList
{
    /* List for filenames */
    char *name;                         /* Filename */
    unsigned int idx;                   /* File index */
    struct MctLogStorageFileList *next; /* Pointer to next */
} MctLogStorageFileList;

typedef struct MctNewestFileName MctNewestFileName;

struct MctNewestFileName
{
    char *file_name;    /* The unique name of file in whole a mct_logstorage.conf */
    char *newest_file;  /* The real newest name of file which is associated with filename.*/
    unsigned int wrap_id;   /* Identifier of wrap around happened for this file_name */
    MctNewestFileName *next; /* Pointer to next */
};

typedef struct MctLogStorageFilterConfig MctLogStorageFilterConfig;

struct MctLogStorageFilterConfig
{
    /* filter section */
    char *apids;                    /* Application IDs configured for filter */
    char *ctids;                    /* Context IDs configured for filter */
    char *excluded_apids;           /* Excluded Application IDs configured for filter */
    char *excluded_ctids;           /* Excluded Context IDs configured for filter */
    int log_level;                  /* Log level number configured for filter */
    int reset_log_level;            /* reset Log level to be sent on disconnect */
    char *file_name;                /* File name for log storage configured for filter */
    char *working_file_name;        /* Current open log file name */
    unsigned int wrap_id;           /* Identifier of wrap around happened for this filter */
    unsigned int file_size;         /* MAX File size of storage file configured for filter */
    unsigned int num_files;         /* MAX number of storage files configured for filters */
    int sync;                       /* Sync strategy */
    int overwrite;                  /* Overwrite strategy */
    int skip;                       /* Flag to skip file logging if DISCARD_NEW */
    char *ecuid;                    /* ECU identifier */
    /* callback function for filter configurations */
    int (*mct_logstorage_prepare)(MctLogStorageFilterConfig *config,
                                  MctLogStorageUserConfig *file_config,
                                  char *dev_path,
                                  int log_msg_size,
                                  MctNewestFileName *newest_file_info);
    int (*mct_logstorage_write)(MctLogStorageFilterConfig *config,
                                MctLogStorageUserConfig *file_config,
                                char *dev_path,
                                unsigned char *data1,
                                int size1,
                                unsigned char *data2,
                                int size2,
                                unsigned char *data3,
                                int size3);
    /* status is strategy, e.g. MCT_LOGSTORAGE_SYNC_ON_MSG is used when callback
     * is called on message received */
    int (*mct_logstorage_sync)(MctLogStorageFilterConfig *config,
                               MctLogStorageUserConfig *uconfig,
                               char *dev_path,
                               int status);
    FILE *log;                      /* current open log file */
    void *cache;                    /* log data cache */
    unsigned int specific_size;     /* cache size used for specific_size sync strategy */
    unsigned int current_write_file_offset;    /* file offset for specific_size sync strategy */
    MctLogStorageFileList *records; /* File name list */
    int disable_network_routing;    /* Flag to disable routing to network client */
};

typedef struct MctLogStorageFilterList MctLogStorageFilterList;

struct MctLogStorageFilterList
{
    char *key_list;                   /* List of key */
    int num_keys;                     /* Number of keys */
    MctLogStorageFilterConfig *data;  /* Filter data */
    MctLogStorageFilterList *next;    /* Pointer to next */
};

typedef enum {
    MCT_LOGSTORAGE_CONFIG_FILE = 0,   /* Use mct-logstorage.conf file from device */
} MctLogStorageConfigMode;

typedef struct
{
    MctLogStorageFilterList *config_list; /* List of all filters */
    MctLogStorageUserConfig uconfig;   /* User configurations for file name*/
    int num_configs;                   /* Number of configs */
    char device_mount_point[MCT_MOUNT_PATH_MAX + 1]; /* Device mount path */
    unsigned int connection_type;      /* Type of connection */
    unsigned int config_status;        /* Status of configuration */
    int prepare_errors;                /* number of prepare errors */
    int write_errors;                  /* number of write errors */
    int block_mode;                    /* Block mode status */
    MctNewestFileName *newest_file_list; /* List of newest file name */
    int maintain_logstorage_loglevel;  /* Permission to maintain the logstorage loglevel*/
    MctLogStorageConfigMode config_mode;                   /* Configuration Mechanism */
} MctLogStorage;

typedef struct {
    char *key; /* The configuration key */
    int (*func)(MctLogStorage *handle, char *value); /* conf handler */
    int is_opt; /* If configuration is optional or not */
} MctLogstorageGeneralConf;

typedef enum {
    MCT_LOGSTORAGE_GENERAL_CONF_BLOCKMODE = 0,
    MCT_LOGSTORAGE_GENERAL_CONF_MAINTAIN_LOGSTORAGE_LOGLEVEL,
    MCT_LOGSTORAGE_GENERAL_CONF_COUNT
} MctLogstorageGeneralConfType;

typedef struct {
    char *key; /* Configuration key */
    int (*func)(MctLogStorageFilterConfig *config, char *value); /* conf handler */
    int is_opt; /* If configuration is optional or not */
} MctLogstorageFilterConf;

typedef enum {
    MCT_LOGSTORAGE_FILTER_CONF_LOGAPPNAME = 0,
    MCT_LOGSTORAGE_FILTER_CONF_CONTEXTNAME,
    MCT_LOGSTORAGE_FILTER_CONF_EXCLUDED_LOGAPPNAME,
    MCT_LOGSTORAGE_FILTER_CONF_EXCLUDED_CONTEXTNAME,
    MCT_LOGSTORAGE_FILTER_CONF_LOGLEVEL,
    MCT_LOGSTORAGE_FILTER_CONF_RESET_LOGLEVEL,
    MCT_LOGSTORAGE_FILTER_CONF_FILE,
    MCT_LOGSTORAGE_FILTER_CONF_FILESIZE,
    MCT_LOGSTORAGE_FILTER_CONF_NOFILES,
    MCT_LOGSTORAGE_FILTER_CONF_SYNCBEHAVIOR,
    MCT_LOGSTORAGE_FILTER_CONF_OVERWRITEBEHAVIOR,
    MCT_LOGSTORAGE_FILTER_CONF_ECUID,
    MCT_LOGSTORAGE_FILTER_CONF_SPECIFIC_SIZE,
    MCT_LOGSTORAGE_FILTER_CONF_DISABLE_NETWORK,
    MCT_LOGSTORAGE_FILTER_CONF_COUNT
} MctLogstorageFilterConfType;

/**
 * mct_logstorage_device_connected
 *
 * Initializes MCT Offline Logstorage with respect to device status
 *
 *
 * @param handle         MCT Logstorage handle
 * @param mount_point    Device mount path
 * @return               0 on success, -1 on error
 */
int mct_logstorage_device_connected(MctLogStorage *handle,
                                    const char *mount_point);

/**
 * mct_logstorage_device_disconnected
 * De-Initializes MCT Offline Logstorage with respect to device status
 *
 * @param handle         MCT Logstorage handle
 * @param reason         Reason for device disconnection
 * @return               0 on success, -1 on error
 */
int mct_logstorage_device_disconnected(MctLogStorage *handle,
                                       int reason);
/**
 * mct_logstorage_get_config
 *
 * Obtain the configuration data of all filters for provided apid and ctid
 * For a given apid and ctid, there can be 3 possiblities of configuration
 * data available in the Hash map, this function will return the address
 * of configuration data for all these 3 combinations
 *
 * @param handle    MctLogStorage handle
 * @param config    Pointer to array of filter configurations
 * @param apid      application id
 * @param ctid      context id
 * @param ecuid     ecu id
 * @return          number of found configurations
 */
int mct_logstorage_get_config(MctLogStorage *handle,
                              MctLogStorageFilterConfig **config,
                              char *apid,
                              char *ctid,
                              char *ecuid);

/**
 * mct_logstorage_get_loglevel_by_key
 *
 * Obtain the log level for the provided key
 * This function can be used to obtain log level when the actual
 * key stored in the Hash map is available with the caller
 *
 * @param handle    MctLogstorage handle
 * @param key       key to search for in Hash MAP
 * @return          log level on success:, -1 on error
 */
int mct_logstorage_get_loglevel_by_key(MctLogStorage *handle, char *key);

/**
 * mct_logstorage_write
 *
 * Write a message to one or more configured log files, based on filter
 * configuration.
 *
 * @param handle    MctLogStorage handle
 * @param uconfig   User configurations for log file
 * @param data1     Data buffer of message header
 * @param size1     Size of message header buffer
 * @param data2     Data buffer of extended message body
 * @param size2     Size of extended message body
 * @param data3     Data buffer of message body
 * @param size3     Size of message body
 * @param disable_nw Flag to disable network routing
 * @return          0 on success or write errors < max write errors, -1 on error
 */
int mct_logstorage_write(MctLogStorage *handle,
                         MctLogStorageUserConfig *uconfig,
                         unsigned char *data1,
                         int size1,
                         unsigned char *data2,
                         int size2,
                         unsigned char *data3,
                         int size3,
                         int *disable_nw);

/**
 * mct_logstorage_sync_caches
 *
 * Sync all caches inside the specified logstorage device.
 *
 * @param  handle    MctLogStorage handle
 * @return 0 on success, -1 otherwise
 */
int mct_logstorage_sync_caches(MctLogStorage *handle);

#endif /* MCT_OFFLINE_LOGSTORAGE_H */
