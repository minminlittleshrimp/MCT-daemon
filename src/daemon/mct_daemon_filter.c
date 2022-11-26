#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "mct_common.h"
#include "mct-daemon.h"
#include "mct_protocol.h"
#include "mct-daemon_cfg.h"
#include "mct_daemon_event_handler.h"
#include "mct_daemon_connection.h"
#include "mct_config_file_parser.h"
#include "mct_daemon_filter.h"
#include "mct_daemon_filter_internal.h"
#include "mct_daemon_filter_backend.h"
#include "mct_daemon_socket.h"

#define MOST_CLOSED_FILTER_NAME "Customer"

/* General filter configuration options */
#define GENERAL_BASE_NAME "General"
typedef struct
{
    char *name;
    int (*handler)(MctMessageFilter *mf, char *val);
    int is_opt;
} general_opts;

/* Filter section options */
#define FILTER_BASE_NAME "Filter"
typedef struct
{
    char *name;
    int (*handler)(MctMessageFilter *mf, MctFilterConfiguration *config, char *value);
} filter_opts;

/* Injection section options */
#define INJECTION_BASE_NAME "Injection"
typedef struct
{
    char *name;
    int (*handler)(MctMessageFilter *mf, MctInjectionConfig *config, char *value);
} injection_opts;

/**
 * @brief Set Control Message bit mask to 1
 *
 * @param flags MctServiceIdFlags
 * @return MCT_RETURN_OK on success, MCT error code otherwise
 */
static MctReturnValue enable_all(MctServiceIdFlag *flags)
{
    if (flags == NULL) {
        mct_vlog(LOG_ERR, "%s: invalid arguments\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    memset(flags, 0xff, sizeof(MctServiceIdFlag));

    return MCT_RETURN_OK;
}

/**
 * @brief Init Control Message bit mask
 *
 * @param flags MctServiceIdFlags
 * @return MCT_RETURN_OK on success, MCT error code otherwise
 */
static MctReturnValue init_flags(MctServiceIdFlag *flags)
{
    if (flags == NULL) {
        mct_vlog(LOG_ERR, "%s: invalid arguments\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    memset(flags, 0, sizeof(MctServiceIdFlag));

    return MCT_RETURN_OK;
}

/**
 * @brief Set bit in Control Message bit mask
 *
 * @param flags MctServiceIdFlags
 * @param id    set bit for given id
 * @return MCT_RETURN_OK on success, MCT error code otherwise
 */
static MctReturnValue set_bit(MctServiceIdFlag *flags, int id)
{
    int is_upper;
    int byte_pos;
    int bit_pos;
    int tmp;

    if (flags == NULL) {
        mct_vlog(LOG_ERR, "%s: invalid arguments\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if ((id <= MCT_SERVICE_ID) || (id >= MCT_USER_SERVICE_ID_LAST_ENTRY) ||
        ((id >= MCT_SERVICE_ID_LAST_ENTRY) && (id <= MCT_USER_SERVICE_ID))) {
        mct_vlog(LOG_WARNING, "Given ID = %d is invalid\n", id);
        return MCT_RETURN_ERROR;
    }

    is_upper = id & MCT_USER_SERVICE_ID;
    tmp = id & 0xFF;

    byte_pos = tmp >> 3; /*  tmp / 8 */
    bit_pos = tmp & 7;   /* tmp % 8 */

    if (is_upper) {
        SET_BIT(flags->upper[byte_pos], bit_pos);
    } else {
        SET_BIT(flags->lower[byte_pos], bit_pos);
    }

    return MCT_RETURN_OK;
}

/**
 * @brief Get bit in Control Message bit mask
 *
 * @param flags MctServiceIdFlags
 * @param id    set bit for given id
 * @return 0,1 on success, MCT error code otherwise
 */
static MctReturnValue bit(MctServiceIdFlag *flags, int id)
{
    int is_upper;
    int byte_pos;
    int bit_pos;
    int tmp;

    if (flags == NULL) {
        mct_vlog(LOG_ERR, "%s: invalid arguments\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if ((id <= MCT_SERVICE_ID) || (id >= MCT_USER_SERVICE_ID_LAST_ENTRY) ||
        ((id >= MCT_SERVICE_ID_LAST_ENTRY) && (id <= MCT_USER_SERVICE_ID))) {
        mct_vlog(LOG_WARNING, "Given ID = %d is invalid\n", id);
        return MCT_RETURN_ERROR;
    }

    is_upper = id & MCT_USER_SERVICE_ID;
    tmp = id & 0xFF;

    byte_pos = tmp >> 3; /*  tmp / 8 */
    bit_pos = tmp & 7;   /*  tmp % 8 */

    if (is_upper) {
        return BIT(flags->upper[byte_pos], bit_pos);
    } else {
        return BIT(flags->lower[byte_pos], bit_pos);
    }
}

/**
 * @brief Check if the filter configuration name is unique
 *
 * In case the name is not unique, print a warning to the user
 *
 * @param mf        MessageFilter pointer
 * @param config    MctFilterConfiguration pointer
 * @param value     Value given in configuration file
 *
 * @return MCT_RETURN_OK on success, MCT error code otherwise
 */
static MctReturnValue mct_daemon_filter_name(MctMessageFilter *mf,
                                                 MctFilterConfiguration *config,
                                                 char *value)
{
    MctFilterConfiguration *conf = NULL;

    if ((mf == NULL) || (config == NULL) || (value == NULL)) {
        mct_vlog(LOG_ERR,
                 "Cannot check section name. Invalid parameter in %s\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    conf = mf->configs;

    while (conf) {
        if (conf->name != NULL) {
            if (strncmp(conf->name, value, strlen(value)) == 0) {
                mct_vlog(LOG_WARNING,
                         "Section name '%s' already in use\n", value);
                return MCT_RETURN_ERROR;
            }
        }

        conf = conf->next;
    }

    config->name = strdup(value);

    if (config->name == NULL) {
        mct_log(LOG_ERR, "Cannot duplicate string\n");
        return MCT_RETURN_ERROR;
    }

    return MCT_RETURN_OK;
}

/**
 * @brief Convert string with filter level information to a number
 *
 * Convert a filter level string from configuration file to a valid filter
 * level.
 *
 * @param mf        MessageFilter pointer
 * @param config    MctFilterConfiguration pointer
 * @param value     Value given in configuration file
 *
 * @return MCT_RETURN_OK on success, MCT error code otherwise
 */
static MctReturnValue mct_daemon_filter_level(MctMessageFilter *mf,
                                                  MctFilterConfiguration *config,
                                                  char *value)
{
    char *max_ptr = NULL;
    unsigned long int max;

    if ((mf == NULL) || (config == NULL) || (value == NULL)) {
        mct_vlog(LOG_ERR,
                 "Cannot check section name. Invalid parameter in %s\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* level should be the upper value of the range */
    max = strtoul(value, &max_ptr, 10);

    if (max_ptr == value) {
        mct_vlog(LOG_WARNING, "Level %s is not a number\n", value);
        return MCT_RETURN_ERROR;
    }

    if (max > MCT_FILTER_LEVEL_MAX) {
        mct_vlog(LOG_WARNING, "Level %ld is invalid\n", max);
        return MCT_RETURN_ERROR;
    }

    /* set level (level_min will be set afterwards) */
    config->level_max = (unsigned int)max;

    return MCT_RETURN_OK;
}

/**
 * @brief Set the control message mask for a filter configuration
 *
 * Use the string given in the configuration file for that filter configuration
 * to create a mask of allowed control messages.
 *
 * @param mf        MessageFilter pointer
 * @param config    MctFilterConfiguration pointer
 * @param value     Value given in configuration file
 *
 * @return MCT_RETURN_OK on success, MCT error code otherwise
 */
static MctReturnValue mct_daemon_filter_control_mask(MctMessageFilter *mf,
                                                         MctFilterConfiguration *config,
                                                         char *value)
{
    char *token = NULL;
    char *save_ptr;

    if ((mf == NULL) || (config == NULL) || (value == NULL)) {
        mct_vlog(LOG_ERR,
                 "Cannot check section name. Invalid parameter in %s\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* initialize mask */
    init_flags(&config->ctrl_mask);

    /* check wildcard */
    if (value[0] == '*') {
        enable_all(&config->ctrl_mask);
        return MCT_RETURN_OK;
    }

    /* check for no client specifier */
    if (strncasecmp(value, MCT_FILTER_CLIENT_NONE, strlen(value)) == 0) {
        return MCT_RETURN_OK;
    }

    /* list of allowed control messages given */
    token = strtok_r(value, ",", &save_ptr);

    while (token != NULL) {
        int base = 16;
        long id = strtol(token, NULL, base);

        if ((id <= MCT_SERVICE_ID) || (id >= MCT_USER_SERVICE_ID_LAST_ENTRY) ||
            ((id >= MCT_SERVICE_ID_LAST_ENTRY) && (id <= MCT_USER_SERVICE_ID))) {
            mct_vlog(LOG_WARNING, "Ignore invalid service ID: %s\n", token);
        } else {
            set_bit(&config->ctrl_mask, id);
        }

        token = strtok_r(NULL, ",", &save_ptr);
    }

    return MCT_RETURN_OK;
}

/**
 * @brief Set the client (connection) mask for a filter configuration
 *
 * Get here a list of strings from the configuration file and create a mask out
 * of it.
 * E.g Serial, TCP will become a mask for connection defined in
 * mct_daemon_connection_types.h:
 * (MCT_CON_MASK_CLIENT_MSG_SERIAL | MCT_CON_MASK_CLIENT_MSG_TCP)
 *
 * @param mf        MessageFilter pointer
 * @param config    MctFilterConfiguration pointer
 * @param value     Value given in configuration file
 *
 * @return MCT_RETURN_OK on success, MCT error code otherwise
 */
static MctReturnValue mct_daemon_filter_client_mask(MctMessageFilter *mf,
                                                        MctFilterConfiguration *config,
                                                        char *value)
{
    char *token = NULL;
    char *rest = NULL;

    if ((mf == NULL) || (config == NULL) || (value == NULL)) {
        mct_vlog(LOG_ERR,
                 "Cannot check section name. Invalid parameter in %s\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* initialize mask */
    config->client_mask = MCT_FILTER_CLIENT_CONNECTION_DEFAULT_MASK;

    /* check wildcard */
    if (value[0] == '*') {
        config->client_mask = MCT_CON_MASK_ALL;
        return MCT_RETURN_OK;
    }

    /* check for no client specifier */
    if (strncasecmp(value, MCT_FILTER_CLIENT_NONE, strlen(value)) == 0) {
        /* return default mask */
        return MCT_RETURN_OK;
    }

    /* list of allowed clients given */
    token = strtok_r(value, ",", &rest);

    while (token != NULL) {
        if (strncasecmp(token, "Serial", strlen(token)) == 0) {
            config->client_mask |= MCT_CON_MASK_CLIENT_MSG_SERIAL;
        } else if (strncasecmp(token, "TCP", strlen(token)) == 0) {
            config->client_mask |= MCT_CON_MASK_CLIENT_CONNECT;
            config->client_mask |= MCT_CON_MASK_CLIENT_MSG_TCP;
        } else if (strncasecmp(token, "Logstorage", strlen(token)) == 0) {
            config->client_mask |= MCT_CON_MASK_CLIENT_MSG_OFFLINE_LOGSTORAGE;
        } else if (strncasecmp(token, "Trace", strlen(token)) == 0) {
            config->client_mask |= MCT_CON_MASK_CLIENT_MSG_OFFLINE_TRACE;
        } else {
            mct_vlog(LOG_INFO, "Ignoring unknown client type: %s\n", token);
        }

        token = strtok_r(NULL, ",", &rest);
    }

    return MCT_RETURN_OK;
}

/**
 * @brief Set injections per filter
 *
 * @param mf        MessageFilter pointer
 * @param config    MctFilterConfiguration pointer
 * @param value     Value given in configuration file
 *
 * @return MCT_RETURN_OK on success, MCT error code otherwise
 */
static MctReturnValue mct_daemon_filter_injections(MctMessageFilter *mf,
                                                       MctFilterConfiguration *config,
                                                       char *value)
{
    int i;
    char *token = NULL;
    char *save_ptr;

    if ((mf == NULL) || (config == NULL) || (value == NULL)) {
        mct_vlog(LOG_ERR,
                 "Cannot check section name. Invalid parameter in %s\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /*value is a komma separated list of injections or '*' or NONE */
    if (strncmp(value, "*", strlen("*")) == 0) {
        config->num_injections = -1;
    } else if (strncasecmp(value, MCT_FILTER_CLIENT_NONE, strlen(value)) == 0) {
        config->num_injections = 0;
    } else { /* at least one specific injection is given */
        config->num_injections = 1;

        /* count numbers of commata to get number of */
        for (i = 0; value[i]; i++) {
            if (value[i] == ',') {
                config->num_injections += 1;
            }
        }

        config->injections = calloc(config->num_injections, sizeof(char *));

        if (config->injections == NULL) {
            mct_log(LOG_CRIT,
                    "Memory allocation for injection configuration failed!\n");
            return MCT_RETURN_ERROR;
        }

        i = 0;

        token = strtok_r(value, ",", &save_ptr);

        while (token != NULL) {
            config->injections[i] = strdup(token);
            i++;
            token = strtok_r(NULL, ",", &save_ptr);
        }
    }

    return MCT_RETURN_OK;
}

static filter_opts filter[] = {
    {.name = "Name", .handler = mct_daemon_filter_name},
    {.name = "Level", .handler = mct_daemon_filter_level},
    {.name = "Clients", .handler = mct_daemon_filter_client_mask},
    {.name = "ControlMessages", .handler = mct_daemon_filter_control_mask},
    {.name = "Injections", .handler = mct_daemon_filter_injections},
    {NULL, NULL}
};

/**
 * @brief Free filter configuration
 *
 * @param conf      Filter Configuration
 */
void mct_daemon_free_filter_configuration(MctFilterConfiguration *conf)
{
    int i = 0;

    if (conf == NULL) {
        return;
    }

    free(conf->name);
    conf->name = NULL;

    for (i = 0; i < conf->num_injections; i++) {
        free(conf->injections[i]);
        conf->injections[i] = NULL;
    }

    free(conf->injections);
    conf->injections = NULL;

    return;
}

/**
 * @brief Check the filter level
 *
 * Check whether the filter level is already defined or not.
 *
 * @param conf      Filter Configuration which has to be inserted
 * @param configs   Filter Configurations
 *
 * @return MCT_RETURN_OK on success, MCT error code otherwise
 */
static MctReturnValue mct_daemon_check_filter_level(MctFilterConfiguration *conf,
                                                        MctFilterConfiguration *configs)
{
    if ((conf == NULL) || (configs == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (conf->level_max == configs->level_max) {
        mct_vlog(LOG_WARNING, "Level %d already defined\n", conf->level_max);
        return MCT_RETURN_ERROR;
    }

    return MCT_RETURN_OK;
}

/**
 * @brief Insert filter and sort
 *
 * Insert filter to the list and sort filter accordingly.
 *
 * @param conf      Filter Configuration which has to be inserted
 * @param configs   Filter Configurations
 *
 * @return MCT_RETURN_OK on success, MCT error code otherwise
 */
static MctReturnValue mct_daemon_insert_filter(MctFilterConfiguration *conf,
                                                   MctFilterConfiguration **configs)
{
    if (conf == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (*configs == NULL) {
        *configs = conf;
        return MCT_RETURN_OK;
    } else {
        if (conf->level_max < (*configs)->level_max) {
            /* insert at the beginning */
            conf->next = *configs;
            (*configs)->level_min = conf->level_max + 1;
            *configs = conf;
            return MCT_RETURN_OK;
        } else {
            while ((*configs)->next != NULL) {
                if (mct_daemon_check_filter_level(conf, *configs) == MCT_RETURN_ERROR) {
                    return MCT_RETURN_ERROR;
                }

                if (conf->level_max < (*configs)->next->level_max) {
                    /* insert in middle */
                    conf->level_min = (*configs)->next->level_min;
                    conf->next = (*configs)->next;
                    (*configs)->next->level_min = conf->level_max + 1;
                    (*configs)->next = conf;
                    return MCT_RETURN_OK;
                } else {
                    configs = &(*configs)->next;
                }
            }

            /* insert at the end */
            if (mct_daemon_check_filter_level(conf, *configs) == MCT_RETURN_ERROR) {
                return MCT_RETURN_ERROR;
            }

            conf->level_min = (*configs)->level_max + 1;
            (*configs)->next = conf;
            return MCT_RETURN_OK;
        }
    }

    return MCT_RETURN_ERROR;
}

/**
 * @brief Setup a filter configuration
 *
 * Use the information of a filter configuration section in the configuration
 * file to setup a filter configuration
 *
 * @param mf        Message filter
 * @param config    Config file handle
 * @param sec_name  Name of the section to setup this filter configuration
 *
 * @return MCT_RETURN_OK on success, MCT error code otherwise
 */
static MctReturnValue mct_daemon_setup_filter_section(MctMessageFilter *mf,
                                                          MctConfigFile *config,
                                                          char *sec_name)
{
    int i = 0;
    char value[MCT_CONFIG_FILE_ENTRY_MAX_LEN + 1] = {'\0'};
    MctFilterConfiguration tmp = {0};
    MctFilterConfiguration *conf = NULL;

    if ((mf == NULL) || (config == NULL) || (sec_name == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    while (filter[i].name) {
        if (mct_config_file_get_value(config,
                                      sec_name,
                                      filter[i].name,
                                      value) != MCT_RETURN_OK) {
            return MCT_RETURN_ERROR;
        }

        if (filter[i].handler) {
            if (filter[i].handler(mf, &tmp, value) != MCT_RETURN_OK) {
                mct_vlog(LOG_ERR,
                         "Failed to set injection parameter: %s\n", filter[i].name);
                return MCT_RETURN_ERROR;
            }
        } else {
            mct_vlog(LOG_CRIT,
                     "No handler for option '%s' found\n", filter[i].name);
            return MCT_RETURN_ERROR;
        }

        i++;
    }

    conf = calloc(1, sizeof(MctFilterConfiguration));

    if (!conf) {
        mct_vlog(LOG_ERR, "%s: Configs could not be allocated\n", __func__);
        return MCT_RETURN_ERROR;
    }

    /* set filter */
    if (tmp.name != NULL) {
        conf->name = strdup(tmp.name);
    }

    conf->level_min = MCT_FILTER_LEVEL_MIN;
    conf->level_max = tmp.level_max;
    conf->client_mask = tmp.client_mask;
    conf->ctrl_mask = tmp.ctrl_mask;
    conf->num_injections = tmp.num_injections;
    conf->next = NULL;

    if (tmp.num_injections > 0) {
        conf->injections = calloc(tmp.num_injections,
                                  sizeof(char *));

        if (!conf->injections) {
            mct_vlog(LOG_ERR, "Injections could not be allocated\n");
            return MCT_RETURN_ERROR;
        }
    }

    for (i = 0; i < tmp.num_injections; i++) {
        conf->injections[i] = strdup(tmp.injections[i]);
    }

    /* set level_min and set conf in the right place */
    if (mct_daemon_insert_filter(conf, &(mf->configs)) != MCT_RETURN_OK) {
        mct_daemon_free_filter_configuration(&tmp);
        mct_daemon_free_filter_configuration(conf);
        free(conf);
        conf = NULL;
        return MCT_RETURN_ERROR;
    }

    mct_daemon_free_filter_configuration(&tmp);

    return MCT_RETURN_OK;
}

/**
 * @brief Set service identifier for an injection configuration
 *
 * Use the string from the configuration and configure the service identifier:
 *
 *  - num_injections == 0:  No injection is allowed
 *  - num_injections == -1: All injections are allowed
 *  - num_injections > 0:   Number of injections given
 *
 * @param ids        Pointer to serivce identifier
 * @param num        Number of service identifier
 * @param value      String taken from configuration file
 *
 * @return MCT_RETURN_OK on success, MCT error code otherwise
 */
static MctReturnValue mct_daemon_set_injection_service_ids(int **ids,
                                                               int *num,
                                                               char *value)
{
    if ((ids == NULL) || (num == NULL) || (value == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* value is a komma separated list of injections or '*' or NONE */
    if (strncmp(value, "*", strlen("*")) == 0) {
        *num = -1;
    } else if (strncasecmp(value, MCT_FILTER_CLIENT_NONE, strlen(value)) == 0) {
        *num = 0;
    } else { /* at least one specific service id is given */
        int i;
        *num = 1;

        /* count numbers of commata to get number of */
        for (i = 0; value[i]; i++) {
            if (value[i] == ',') {
                *num += 1;
            }
        }

        if (*ids != NULL) {
            free(*ids);
        }

        *ids = calloc(*num, sizeof(int));

        if (*ids == NULL) {
            mct_log(LOG_CRIT, "Failed to allocate memory for service IDs\n");
            return MCT_RETURN_ERROR;
        }

        char *token = NULL;
        char *save_ptr;
        i = 0;

        token = strtok_r(value, ",", &save_ptr);

        while (token != NULL) {
            (*ids)[i] = strtol(token, NULL, 10);
            i++;
            token = strtok_r(NULL, ",", &save_ptr);
        }
    }

    return MCT_RETURN_OK;
}

/**
 * @brief Find an injection configuration by given name
 *
 * This function searches for an injection configuration with the given name. It
 * is assumed that each injection has a unique name.
 *
 * @param injections    Array of known injections
 * @param name          Name of the injection configuration
 *
 * @return Corressponding injection on success, NULL otherwise.
 */
static MctInjectionConfig *mct_daemon_filter_find_injection_by_name(
    MctInjectionConfig *injections,
    char *name)
{
    int i;

    if ((injections == NULL) || (name == NULL)) {
        return NULL;
    }

    for (i = 0; i < MCT_FILTER_INJECTION_CONFIG_MAX; i++) {
        if (injections[i].name == NULL) {
            return NULL;
        }

        if (strncmp(injections[i].name, name, strlen(injections[i].name)) == 0) {
            /* injection found */
            return &injections[i];
        }
    }

    return NULL;
}

static MctReturnValue mct_daemon_injection_name(MctMessageFilter *mf,
                                                    MctInjectionConfig *config,
                                                    char *value)
{
    if ((mf == NULL) || (config == NULL) || (value == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (mct_daemon_filter_find_injection_by_name(mf->injections,
                                                 value) != NULL) {
        mct_vlog(LOG_ERR,
                 "Injection configuration name '%s'already in use\n", value);
        return MCT_RETURN_ERROR;
    }

    config->name = strdup(value);

    if (config->name == NULL) {
        mct_log(LOG_CRIT, "Cannot duplicate string\n");
        return MCT_RETURN_ERROR;
    }

    return MCT_RETURN_OK;
}

static MctReturnValue mct_daemon_injection_apid(MctMessageFilter *mf,
                                                    MctInjectionConfig *config,
                                                    char *value)
{
    if ((mf == NULL) || (config == NULL) || (value == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    config->apid = strdup(value);

    if (config->apid == NULL) {
        return MCT_RETURN_ERROR;
    }

    return MCT_RETURN_OK;
}

static MctReturnValue mct_daemon_injection_ctid(MctMessageFilter *mf,
                                                    MctInjectionConfig *config,
                                                    char *value)
{
    if ((mf == NULL) || (config == NULL) || (value == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    config->ctid = strdup(value);

    if (config->ctid == NULL) {
        return MCT_RETURN_ERROR;
    }

    return MCT_RETURN_OK;
}

static MctReturnValue mct_daemon_injection_ecu_id(MctMessageFilter *mf,
                                                      MctInjectionConfig *config,
                                                      char *value)
{
    if ((mf == NULL) || (config == NULL) || (value == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    config->ecuid = strdup(value);

    if (config->ecuid == NULL) {
        return MCT_RETURN_ERROR;
    }

    return MCT_RETURN_OK;
}

static MctReturnValue mct_daemon_injection_service_id(MctMessageFilter *mf,
                                                          MctInjectionConfig *config,
                                                          char *value)
{
    if ((mf == NULL) || (config == NULL) || (value == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (mct_daemon_set_injection_service_ids(&config->service_ids,
                                             &config->num_sevice_ids,
                                             value) == -1) {
        mct_log(LOG_ERR, "Cannot set injection service ID\n");
        return MCT_RETURN_ERROR;
    }

    return MCT_RETURN_OK;
}

static injection_opts injection[] = {
    {.name = "Name", .handler = mct_daemon_injection_name},
    {.name = "LogAppName", .handler = mct_daemon_injection_apid},
    {.name = "ContextName", .handler = mct_daemon_injection_ctid},
    {.name = "NodeID", .handler = mct_daemon_injection_ecu_id},
    {.name = "ServiceID", .handler = mct_daemon_injection_service_id},
    {NULL, NULL}
};

/**
 * @brief Setup an injection message configuration
 *
 * Get all information for an injection message configuration from the message
 * filter configuration file and setup an injection message configuration.
 *
 * @param mf        Message Filter
 * @param config    Configuration file handle
 * @param sec_name   Section name
 *
 * @return MCT_RETURN_OK on success, MCT error code otherwise
 */
static MctReturnValue mct_daemon_setup_injection_config(MctMessageFilter *mf,
                                                            MctConfigFile *config,
                                                            char *sec_name)
{
    int i = 0;
    MctInjectionConfig *tmp = NULL;
    char value[MCT_CONFIG_FILE_ENTRY_MAX_LEN + 1] = {'\0'};

    if ((mf == NULL) || (config == NULL) || (sec_name == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* find next free injection message configuration slot */
    if (mf->num_injections == MCT_FILTER_INJECTION_CONFIG_MAX) {
        mct_log(LOG_ERR, "Maximum number of supported injections reached\n");
        return MCT_RETURN_ERROR;
    }

    /* set current injection configuration */
    tmp = &mf->injections[mf->num_injections];

    while (injection[i].name) {
        if (mct_config_file_get_value(config,
                                      sec_name,
                                      injection[i].name,
                                      value) != MCT_RETURN_OK) {
            mct_vlog(LOG_ERR, "Failed to read parameter: %s\n", injection[i].name);
        }

        if (injection[i].handler) {
            if (injection[i].handler(mf, tmp, value) != MCT_RETURN_OK) {
                mct_vlog(LOG_ERR,
                         "Failed to set injection parameter: %s\n", injection[i].name);
                return MCT_RETURN_ERROR;
            }
        } else {
            mct_vlog(LOG_CRIT,
                     "No handler for option '%s' found\n", injection[i].name);
            return MCT_RETURN_ERROR;
        }

        i++;
    }

    mf->num_injections++; /* next injection */

    return MCT_RETURN_OK;
}

static MctReturnValue mct_daemon_get_name(MctMessageFilter *mf, char *val)
{
    if ((mf == NULL) || (val == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    mf->name = strdup(val);

    if (mf->name == NULL) {
        mct_log(LOG_CRIT, "Cannot allocate memory for configuration name\n");
        return MCT_RETURN_ERROR;
    }

    return MCT_RETURN_OK;
}

static MctReturnValue mct_daemon_get_default_level(MctMessageFilter *mf, char *val)
{
    char *endptr;
    unsigned int tmp;

    if ((mf == NULL) || (val == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    tmp = strtoul(val, &endptr, 10);

    if (endptr == val) {
        mct_log(LOG_WARNING, "Default Level is not a number\n");
        return MCT_RETURN_ERROR;
    }

    if (tmp > MCT_FILTER_LEVEL_MAX) {
        mct_log(LOG_WARNING, "Default Level is invalid\n");
        return MCT_RETURN_ERROR;
    }

    /* set default level */
    mf->default_level = tmp;

    return MCT_RETURN_OK;
}

static MctReturnValue mct_daemon_get_backend(MctMessageFilter *mf, char *val)
{
    if ((mf == NULL) || (val == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    mf->backend = strdup(val);

    if (mf->backend == NULL) {
        mct_log(LOG_ERR, "Cannot duplicate string for backend name\n");
        return MCT_RETURN_ERROR;
    }

    return MCT_RETURN_OK;
}

static general_opts general[] = {
    {.name = "Name", .handler = mct_daemon_get_name, .is_opt = 1},
    {.name = "DefaultLevel", .handler = mct_daemon_get_default_level, .is_opt = 0},
    {.name = "Backend", .handler = mct_daemon_get_backend, .is_opt = 1},
    {NULL, NULL, 0}
};

static MctReturnValue mct_daemon_setup_filter_properties(MctMessageFilter *mf,
                                                             MctConfigFile *config,
                                                             char *sec_name)
{
    char value[MCT_CONFIG_FILE_ENTRY_MAX_LEN + 1] = {'\0'};
    int i = 0;
    MctReturnValue ret = MCT_RETURN_OK;

    if ((mf == NULL) || (config == NULL) || (sec_name == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    while (general[i].name) {
        ret = mct_config_file_get_value(config,
                                        sec_name,
                                        general[i].name,
                                        value);

        if ((ret != MCT_RETURN_OK) && general[i].is_opt) {
            mct_vlog(LOG_INFO,
                     "Optional parameter '%s' not specified\n", general[i].name);
            i++;
            ret = MCT_RETURN_OK; /* its OK to have no optional parameter */
            continue;
        } else if (ret != MCT_RETURN_OK) {
            mct_vlog(LOG_ERR,
                     "Missing configuration for '%s'\n", general[i].name);
            break;
        }

        if (general[i].handler) {
            ret = general[i].handler(mf, value);
        } else {
            mct_vlog(LOG_CRIT,
                     "No handler for option '%s' found\n", general[i].name);
            ret = MCT_RETURN_ERROR;
            break;
        }

        if (ret != MCT_RETURN_OK) {
            mct_vlog(LOG_ERR, "Configuration for '%s' is invalid: %s\n",
                     general[i].name, value);
        }

        i++;
    }

    return ret;
}

/**
 * add or update the most closed filter
 *
 * @param name      name of the filter
 * @return MctFilterConfiguration
 */
static MctFilterConfiguration *mct_daemon_add_closed_filter(char *name)
{
    MctFilterConfiguration *conf = NULL;

    if (name == NULL) {
        return NULL;
    }

    conf = calloc(1, sizeof(MctFilterConfiguration));

    if (conf == NULL) {
        mct_vlog(LOG_ERR, "%s: Configs could not be allocated\n", __func__);
        return conf;
    }

    conf->name = strdup(name);
    conf->level_min = MCT_FILTER_LEVEL_MIN;
    conf->level_max = MCT_FILTER_LEVEL_MAX;
    init_flags(&conf->ctrl_mask);
    conf->client_mask = MCT_FILTER_CLIENT_CONNECTION_DEFAULT_MASK;
    conf->num_injections = 0;

    return conf;
}

/**
 * check if the filter levels are all covered and in correct restriction order
 *
 * @param mf    Message filter
 *
 * @return MCT_RETURN_OK on success, MCT error code otherwise
 */
static MctReturnValue mct_daemon_check_filter_level_range(MctMessageFilter *mf)
{
    MctFilterConfiguration *conf = NULL;

    if (mf == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (mf->configs == NULL) {
        /* if no mf->configs is defined, add a most closed filter automatically */
        mct_vlog(LOG_WARNING,
                 "Filter not defined. Add a most closed filter named '%s'.\n",
                 MOST_CLOSED_FILTER_NAME);
        mf->configs = mct_daemon_add_closed_filter(MOST_CLOSED_FILTER_NAME);

        if (mf->configs == NULL) {
            mct_log(LOG_ERR, "Cannot prepare filter\n");
            return MCT_RETURN_ERROR;
        }
    } else {
        conf = mf->configs;

        while (conf) {
            if (conf->next == NULL) {
                /* check if it has MCT_FILTER_LEVEL_MAX
                 * as upper value of the level range */
                if (conf->level_max < MCT_FILTER_LEVEL_MAX) {
                    mct_vlog(LOG_WARNING,
                             "Make %s level defined until %d\n",
                             conf->name, MCT_FILTER_LEVEL_MAX);
                    conf->level_max = MCT_FILTER_LEVEL_MAX;
                }
            }

            conf = conf->next;
        }
    }

    return MCT_RETURN_OK;
}

/**
 * set default level
 *
 * @param mf    Message filter
 *
 * @return MCT_RETURN_OK on success, MCT error code otherwise
 */
static MctReturnValue mct_daemon_set_default_level(MctMessageFilter *mf)
{
    MctFilterConfiguration *conf = NULL;

    if (mf == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    conf = mf->configs;

    while (conf) {
        if (conf->level_max >= mf->default_level) {
            mf->current = conf;
            return MCT_RETURN_OK;
        }

        conf = conf->next;
    }

    /* if this happens, the default level was not acceptable.
     * this shouldn't happen. */
    mct_vlog(LOG_ERR,
             "Default level %d is not acceptable\n", mf->default_level);

    return MCT_RETURN_ERROR;
}

MctReturnValue mct_daemon_prepare_message_filter(MctDaemonLocal *daemon_local,
                                                 int verbose)
{
    MctMessageFilter *mf;
    char *filter_config = daemon_local->flags.msgFilterConfFile;
    MctConfigFile *config = NULL;
    MctFilterConfiguration *tmp = NULL;
    int sec = 0;
    int num_sec = 0;
    MctReturnValue ret = MCT_RETURN_OK;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon_local == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (daemon_local->flags.injectionMode == 0) {
        mct_vlog(
            LOG_NOTICE,
            "%s: Injection message is disabled in mct.conf and injection settings in message filter would be ignored\n",
            __func__);
    }

    mf = &daemon_local->pFilter;

    mf->current = NULL;

    config = mct_config_file_init(filter_config);

    if (config == NULL) {
        mct_log(LOG_CRIT, "Failed to open filter configuration file\n");
        return MCT_RETURN_ERROR;
    }

    mct_config_file_get_num_sections(config, &num_sec);

    while (sec < num_sec) {
        char sec_name[MCT_CONFIG_FILE_ENTRY_MAX_LEN + 1] = {'\0'};

        if (mct_config_file_get_section_name(config, sec, sec_name) == MCT_RETURN_ERROR) {
            mct_log(LOG_CRIT, "Failed to read section name\n");
            ret = MCT_RETURN_ERROR;
            break;
        }

        if (strstr(sec_name, GENERAL_BASE_NAME) != NULL) {
            if (mct_daemon_setup_filter_properties(mf, config, sec_name) == MCT_RETURN_ERROR) {
                mct_vlog(LOG_CRIT, "Filter configuration [%s] is invalid\n", sec_name);
                ret = MCT_RETURN_ERROR;
                break;
            }
        } else if (strstr(sec_name, FILTER_BASE_NAME) != NULL) {
            if (mct_daemon_setup_filter_section(mf, config, sec_name) == MCT_RETURN_ERROR) {
                mct_vlog(LOG_CRIT, "Filter configuration [%s] is invalid\n", sec_name);
                ret = MCT_RETURN_ERROR;
                break;
            }
        } else if (strstr(sec_name, INJECTION_BASE_NAME) != NULL) {
            if (mct_daemon_setup_injection_config(mf, config, sec_name) == MCT_RETURN_ERROR) {
                mct_vlog(LOG_CRIT, "Filter configuration [%s] is invalid\n", sec_name);
                ret = MCT_RETURN_ERROR;
                break;
            }
        } else { /* unknown section */
            mct_vlog(LOG_WARNING, "Unknown section: %s", sec_name);
        }

        sec++;
    }

    if (ret != MCT_RETURN_ERROR) {
        /* check if levels are all covered and in correct restriction order */
        if (mct_daemon_check_filter_level_range(mf) == MCT_RETURN_ERROR) {
            mct_log(LOG_CRIT, "Filter level is not covered completely\n");
            ret = MCT_RETURN_ERROR;
        }

        /* set default level */
        if (mct_daemon_set_default_level(mf) == MCT_RETURN_ERROR) {
            mct_log(LOG_CRIT, "Could not set default level\n");
            ret = MCT_RETURN_ERROR;
        }
    } else {
        /* free */
        if (mf->name != NULL) {
            free(mf->name);
            mf->name = NULL;
        }

        if (mf->backend != NULL) {
            free(mf->backend);
            mf->backend = NULL;
        }

        while (mf->configs) {
            tmp = mf->configs;
            mf->configs = mf->configs->next;
            mct_daemon_free_filter_configuration(tmp);
            free(tmp);
            tmp = NULL;
        }
    }

    /* initialize backend if available */
    if ((ret == MCT_RETURN_OK) && (mf->backend != NULL)) {
        if (mct_daemon_filter_backend_init(daemon_local,
                                           mf->default_level,
                                           verbose) != 0) {
            mct_log(LOG_CRIT, "Filter backend initialization failed\n");
            ret = MCT_RETURN_ERROR;
        }
    }

    mct_config_file_release(config);

    return ret;
}

void mct_daemon_cleanup_message_filter(MctDaemonLocal *daemon_local,
                                       int verbose)
{
    int i = 0;
    MctMessageFilter *mf = NULL;
    MctFilterConfiguration *conf = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon_local == NULL) {
        return;
    }

    mf = &daemon_local->pFilter;

    /* free filter configurations */
    while (mf->configs) {
        conf = mf->configs;
        mf->configs = mf->configs->next;
        mct_daemon_free_filter_configuration(conf);
        free(conf);
        conf = NULL;
    }

    /* free injection configurations */
    for (i = 0; i < MCT_FILTER_INJECTION_CONFIG_MAX; i++) {
        /* return when injection was not initialized */
        if (mf->injections[i].name != NULL) {
            free(mf->injections[i].name);
        }

        if (mf->injections[i].apid != NULL) {
            free(mf->injections[i].apid);
        }

        if (mf->injections[i].ctid != NULL) {
            free(mf->injections[i].ctid);
        }

        if (mf->injections[i].ecuid != NULL) {
            free(mf->injections[i].ecuid);
        }

        if (mf->injections[i].service_ids != NULL) {
            free(mf->injections[i].service_ids);
        }

        memset(&mf->injections[i], 0, sizeof(mf->injections[i]));
    }

    if (mf->backend != NULL) {
        mct_daemon_filter_backend_deinit(daemon_local, verbose);
        free(mf->backend);
        mf->backend = NULL;
    }

    free(mf->name);
    mf->name = NULL;
    mf->current = NULL;
}

int mct_daemon_filter_is_connection_allowed(MctMessageFilter *filter,
                                            MctConnectionType type)
{
    if (filter == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    MctFilterConfiguration *curr = filter->current;

    return curr->client_mask & MCT_CONNECTION_TO_MASK(type);
}

int mct_daemon_filter_is_control_allowed(MctMessageFilter *filter,
                                         int message_id)
{
    if (filter == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    return bit(&(filter->current->ctrl_mask), message_id);
}

MctReturnValue mct_daemon_filter_is_injection_allowed(MctMessageFilter *filter,
                                                      char *apid,
                                                      char *ctid,
                                                      char *ecuid,
                                                      int service_id)
{
    MctFilterConfiguration *curr = NULL;
    int i = 0;
    int j = 0;

    if ((filter == NULL) || (apid == NULL) || (ctid == NULL) || (ecuid == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    curr = filter->current;

    /* check first, if all or no injection is allowed */
    if (curr->num_injections == 0) { /* no injection allowed */
        return MCT_RETURN_OK;
    } else if (curr->num_injections == -1) { /* all allowed */
        return MCT_RETURN_TRUE;
    }

    /* Only a certain list of injection messages is allowed. This list is a
     * whitelist. That means, as soon as machting entry is found, true will
     * be returned. */
    for (i = 0; i < curr->num_injections; i++) {
        MctInjectionConfig *icfg = mct_daemon_filter_find_injection_by_name(
                filter->injections,
                curr->injections[i]);

        if (icfg == NULL) {
            mct_log(LOG_ERR, "Injection configuration entry not found!\n");
            return MCT_RETURN_ERROR;
        }

        /* check application identifier, context identifier, node identifier
         * and injection id (service id). Every entry must be valid to allow
         * that injection message */
        if (strncmp(icfg->apid, apid, strlen(icfg->apid)) != 0) {
            break;
        }

        if (strncmp(icfg->ctid, ctid, strlen(icfg->ctid)) != 0) {
            break;
        }

        if (strncmp(icfg->ecuid, ecuid, strlen(icfg->ecuid)) != 0) {
            break;
        }

        for (j = 0; j < icfg->num_sevice_ids; j++) {
            /* if one of the stored ids is the same, the injection message
             * is valid */
            if (icfg->service_ids[j] == service_id) {
                return MCT_RETURN_TRUE;
            }
        }
    }

    return MCT_RETURN_OK;
}

MctReturnValue mct_daemon_filter_change_filter_level(MctDaemonLocal *daemon_local,
                                                     unsigned int level,
                                                     int verbose)
{
    MctMessageFilter *mf = NULL;
    MctFilterConfiguration *conf = NULL;
    MctDaemonFlags *flags = NULL;
    MctBindAddress_t *head = NULL;
    int fd = -1;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon_local == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (level > MCT_FILTER_LEVEL_MAX) {
        mct_vlog(LOG_ERR,
                 "Invalid arguments %s: %p, %u\n", __func__, daemon_local, level);
        return MCT_RETURN_ERROR;
    }

    mf = &daemon_local->pFilter;
    conf = mf->configs;

    while (conf) {
        if (conf->level_max >= level) {
            mf->current = conf;
            break;
        }

        conf = conf->next;
    }

    /* if conf is NULL, the current level happens to be not updated */
    if (!conf) {
        mct_vlog(LOG_ERR, "Level %d is not acceptable\n", level);
        return MCT_RETURN_ERROR;
    }

    MctConnection *temp = mct_connection_get_next(daemon_local->pEvent.connections,
                                                  MCT_CON_MASK_CLIENT_CONNECT);
    flags = &daemon_local->flags;
    head = flags->ipNodes;

    if (temp == NULL) {
        /* This might happen, because all connections are created on daemon
         * startup  but client_connect connection is created only if it is allowed by ALD
         */
        mct_log(LOG_INFO, "Connection not found! creating one if allowed\n");

        if (mct_daemon_filter_is_connection_allowed(&daemon_local->pFilter,
                                                    MCT_CONNECTION_CLIENT_CONNECT) > 0) {
            if (head == NULL) { /* no IP set in BindAddress option, will use "0.0.0.0" as default */
                if (mct_daemon_socket_open(&fd, daemon_local->flags.port,
                                           "0.0.0.0") == MCT_RETURN_OK) {
                    if (mct_connection_create(daemon_local,
                                              &daemon_local->pEvent,
                                              fd,
                                              POLLIN,
                                              MCT_CONNECTION_CLIENT_CONNECT) != MCT_RETURN_OK) {
                        mct_log(LOG_ERR, "Could not create connection for main socket.\n");
                        /* Exit mct-daemon since it cannot serve its purpose if the main socket
                         * is not available
                         */
                        mct_daemon_exit_trigger();
                        return MCT_RETURN_ERROR;
                    } else {
                        return MCT_RETURN_OK;
                    }
                } else {
                    mct_log(LOG_ERR, "Could not initialize main socket.\n");
                    /* Exit mct-daemon since it cannot serve its purpose if the main socket
                     * is not available
                     */
                    mct_daemon_exit_trigger();
                    return MCT_RETURN_ERROR;
                }
            } else {
                while (head != NULL) { /* open socket for each IP in the bindAddress list */
                    if (mct_daemon_socket_open(&fd, daemon_local->flags.port,
                                               head->ip) == MCT_RETURN_OK) {
                        if (mct_connection_create(daemon_local,
                                                  &daemon_local->pEvent,
                                                  fd,
                                                  POLLIN,
                                                  MCT_CONNECTION_CLIENT_CONNECT) !=
                            MCT_RETURN_OK) {
                            mct_log(LOG_ERR, "Could not create connection for main socket.\n");
                            /* Exit mct-daemon since it cannot serve its purpose if the main socket
                             * is not available
                             */
                            mct_daemon_exit_trigger();
                            return MCT_RETURN_ERROR;
                        } else {
                            return MCT_RETURN_OK;
                        }
                    } else {
                        mct_log(LOG_ERR, "Could not initialize main socket.\n");
                        /* Exit mct-daemon since it cannot serve its purpose if the main socket
                         * is not available
                         */
                        mct_daemon_exit_trigger();
                        return MCT_RETURN_ERROR;
                    }

                    head = head->next;
                }
            }
        }
    } else {
        /* When the connection is already available, open the socket if client_connect
         * is allowed and update receiver->fd accordingly
         */
        if (mct_daemon_filter_is_connection_allowed(&daemon_local->pFilter,
                                                    MCT_CON_MASK_CLIENT_CONNECT) > 0) {
            if (temp->receiver->fd == -1) {
                if (head == NULL) { /* no IP set in BindAddress option, will use "0.0.0.0" as default */
                    if (mct_daemon_socket_open(&fd, daemon_local->flags.port, "0.0.0.0")) {
                        mct_log(LOG_ERR, "Could not initialize main socket.\n");
                        /* Exit mct-daemon since it cannot serve its purpose if the main socket
                         * is not available
                         */
                        mct_daemon_exit_trigger();
                        return MCT_RETURN_ERROR;
                    } else {
                        /* Assigning the new fd to the corresponding connection */
                        temp->receiver->fd = fd;
                    }
                } else {
                    while (head != NULL) { /* open socket for each IP in the bindAddress list */
                        if (mct_daemon_socket_open(&fd, daemon_local->flags.port, head->ip)) {
                            mct_log(LOG_ERR, "Could not initialize main socket.\n");
                            /* Exit mct-daemon since it cannot serve its purpose if the main socket
                             * is not available
                             */
                            mct_daemon_exit_trigger();
                            return MCT_RETURN_ERROR;
                        } else {
                            /* Assigning the new fd to the corresponding connection */
                            temp->receiver->fd = fd;
                        }

                        head = head->next;
                    }
                }
            }
        } else {
            mct_daemon_socket_close(temp->receiver->fd);
        }
    }

    /* Will activate the connection if allowed,
     * or deactivate it if its not allowed anymore
     */
    return mct_connection_check_activate(&daemon_local->pEvent,
                                         temp,
                                         &daemon_local->pFilter,
                                         ACTIVATE);
}

MctReturnValue mct_daemon_filter_process_filter_control_messages(
    MctDaemon *daemon,
    MctDaemonLocal *daemon_local,
    MctReceiver *receiver,
    int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    (void)daemon;   /* not needed here, satisfy compiler */
    (void)receiver; /* not needed here, satisfy compiler */

    if (daemon_local == NULL) {
        mct_vlog(LOG_ERR, "Invalid function parameters in %s\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (daemon_local->pFilter.backend != NULL) {
        /* call backend dispatch function with daemon_local as first param */
        return mct_daemon_filter_backend_dispatch(daemon_local, &verbose);
    }

    return MCT_RETURN_OK;
}
