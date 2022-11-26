#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>

#include "mct_offline_logstorage.h"
#include "mct_offline_logstorage_internal.h"
#include "mct_offline_logstorage_behavior.h"
#include "mct_config_file_parser.h"

#define MCT_OFFLINE_LOGSTORAGE_FILTER_ERROR 1
#define MCT_OFFLINE_LOGSTORAGE_STORE_FILTER_ERROR 2
#define MCT_OFFLINE_LOGSTORAGE_FILTER_CONTINUE 3

#define GENERAL_BASE_NAME "General"

static void mct_logstorage_filter_config_free(MctLogStorageFilterConfig *data)
{
    MctLogStorageFileList *n = NULL;
    MctLogStorageFileList *n1 = NULL;

    if (data->apids) {
        free(data->apids);
        data->apids = NULL;
    }

    if (data->ctids) {
        free(data->ctids);
        data->ctids = NULL;
    }

    if (data->excluded_apids) {
        free(data->excluded_apids);
        data->excluded_apids = NULL;
    }

    if (data->excluded_ctids) {
        free(data->excluded_ctids);
        data->excluded_ctids = NULL;
    }

    if (data->file_name) {
        free(data->file_name);
        data->file_name = NULL;
    }

    if (data->working_file_name) {
        free(data->working_file_name);
        data->working_file_name = NULL;
    }

    if (data->ecuid != NULL) {
        free(data->ecuid);
        data->ecuid = NULL;
    }

    if (data->log != NULL)
        fclose(data->log);

    if (data->cache != NULL) {
        free(data->cache);
        data->cache = NULL;
    }

    n = data->records;

    while (n) {
        n1 = n;
        n = n->next;
        if (n1->name) {
            free(n1->name);
            n1->name = NULL;
        }

        free(n1);
        n1 = NULL;
    }
}

/**
 * mct_logstorage_list_destroy
 *
 * Destroy Filter configurations list.
 *
 * @param list List of the filter configurations will be destroyed.
 * @param uconfig User configurations for log file
 * @param dev_path Path to the device
 * @param reason Reason for the destroying of Filter configurations list
 * @return 0 on success, -1 on error
 */
static int mct_logstorage_list_destroy(MctLogStorageFilterList **list,
                                           MctLogStorageUserConfig *uconfig,
                                           char *dev_path,
                                           int reason)
{
    MctLogStorageFilterList *tmp = NULL;

    while (*(list) != NULL) {
        tmp = *list;
        *list = (*list)->next;
        if (tmp->key_list != NULL)
        {
            free(tmp->key_list);
            tmp->key_list = NULL;
        }

        if (tmp->data != NULL) {
            /* sync data if necessary */
            /* ignore return value */
            tmp->data->mct_logstorage_sync(tmp->data,
                                           uconfig,
                                           dev_path,
                                           reason);

            mct_logstorage_filter_config_free(tmp->data);

            free(tmp->data);
            tmp->data = NULL;
        }

        free(tmp);
        tmp = NULL;
    }

    return 0;
}

static int mct_logstorage_list_add_config(MctLogStorageFilterConfig *data,
                                              MctLogStorageFilterConfig **listdata)
{
    if (*(listdata) == NULL)
        return -1;

    /* copy the data to list */
    memcpy(*listdata, data, sizeof(MctLogStorageFilterConfig));

    if (data->apids != NULL)
        (*listdata)->apids = strdup(data->apids);

    if (data->ctids != NULL)
        (*listdata)->ctids = strdup(data->ctids);

    if (data->excluded_apids != NULL)
        (*listdata)->excluded_apids = strdup(data->excluded_apids);

    if (data->excluded_ctids != NULL)
        (*listdata)->excluded_ctids = strdup(data->excluded_ctids);

    if (data->file_name != NULL)
        (*listdata)->file_name = strdup(data->file_name);

    if (data->ecuid != NULL)
        (*listdata)->ecuid = strdup(data->ecuid);

    return 0;
}

/**
 * mct_logstorage_list_add
 *
 * Add Filter configurations to the list.
 *
 * @param keys Keys will be added to the list.
 * @param num_keys Number of keys
 * @param data Filter configurations data will be added to the list.
 * @param list List of the filter configurations
 * @return 0 on success, -1 on error
 */
static int mct_logstorage_list_add(char *keys,
                                       int num_keys,
                                       MctLogStorageFilterConfig *data,
                                       MctLogStorageFilterList **list)
{
    MctLogStorageFilterList *tmp = NULL;

    while (*(list) != NULL) {
        list = &(*list)->next;
    }

    tmp = calloc(1, sizeof(MctLogStorageFilterList));

    if (tmp == NULL)
        return -1;

    tmp->key_list = (char *)calloc(
                (num_keys * MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN), sizeof(char));
    if (tmp->key_list == NULL)
    {
        free(tmp);
        tmp = NULL;
        return -1;
    }

    memcpy(tmp->key_list, keys, num_keys * MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN);
    tmp->num_keys = num_keys;
    tmp->next = NULL;
    tmp->data = calloc(1, sizeof(MctLogStorageFilterConfig));

    if (tmp->data == NULL) {
        free(tmp->key_list);
        tmp->key_list = NULL;
        free(tmp);
        tmp = NULL;
        return -1;
    }

    if (mct_logstorage_list_add_config(data, &(tmp->data)) != 0) {
        free(tmp->key_list);
        tmp->key_list = NULL;
        free(tmp->data);
        tmp->data = NULL;
        free(tmp);
        tmp = NULL;
        return -1;
    }

    *list = tmp;

    return 0;
}

/**
 * mct_logstorage_list_find
 *
 * Find all Filter configurations corresponding with key provided.
 *
 * @param key Key to find the filter configurations
 * @param list List of the filter configurations
 * @param config Filter configurations corresponding with the key.
 * @return Number of the filter configuration found.
 */
static int mct_logstorage_list_find(char *key,
                                        MctLogStorageFilterList **list,
                                        MctLogStorageFilterConfig **config)
{
    int i = 0;
    int num = 0;

    while (*(list) != NULL) {
        for (i = 0; i < (*list)->num_keys; i++)
        {
            if (strncmp(((*list)->key_list
                        + (i * MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN)),
                        key, MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN) == 0)
            {
                config[num] = (*list)->data;
                num++;
                break;
            }
        }
        list = &(*list)->next;
    }

    return num;
}

/* Configuration file parsing helper functions */

static int mct_logstorage_count_ids(const char *str)
{

    if (str == NULL)
        return -1;

    /* delimiter is: "," */
    const char *p = str;
    int i = 0;
    int num = 1;

    while (p[i] != 0) {
        if (p[i] == ',')
            num++;

        i++;
    }

    return num;
}

/**
 * mct_logstorage_free
 *
 * Free all allocated memory used in log storage handle
 *
 * @param handle         MCT Logstorage handle
 * @param reason         Reason for freeing the device
 *
 */
void mct_logstorage_free(MctLogStorage *handle, int reason)
{
    if (handle == NULL) {
        mct_vlog(LOG_ERR, "%s failed: handle is NULL\n", __func__);
        return;
    }

    mct_logstorage_list_destroy(&(handle->config_list), &handle->uconfig,
                                handle->device_mount_point, reason);
}


/**
 * mct_logstorage_read_list_of_names
 *
 * Evaluate app and ctx names given in config file and create a list of names
 * acceptable by MCT Daemon. When using SET_APPLICATION_NAME and SET_CONTEXT_NAME
 * there is no constraint that these names have max 4 characters. Internally,
 * these names are cutted down to max 4 chars. To have create valid keys, the
 * internal representation of these names has to be considered.
 * Therefore, a given configuration of "AppLogName = App1,Application2,A3" will
 * be stored as "App1,Appl,A3".
 *
 * @param names        to store the list of names
 * @param value        string given in config file
 * @return             0 on success, -1 on error
 */
static int mct_logstorage_read_list_of_names(char **names, const char *value)
{
    int i = 0;
    int y = 0;
    int len = 0;
    char *tok;
    int num = 1;

    if ((names == NULL) || (value == NULL)) {
        mct_vlog(LOG_ERR, "%s: Arguments are set to NULL\n", __func__);
        return -1;
    }

    /* free, alloce'd memory to store new apid/ctid */
    if (*names != NULL) {
        free(*names);
        *names = NULL;
    }

    len = strlen(value);

    if (len == 0) {
        mct_vlog(LOG_ERR, "%s: Length of string given in config file is 0\n",
                 __func__);
        return -1;
    }

    /* count number of delimiters to get actual number off names */
    num = mct_logstorage_count_ids(value);

    /* need to alloc space for 5 chars, 4 for the name and "," and "\0" */
    *names = (char *)calloc(num * 5, sizeof(char));

    if (*names == NULL) {
        mct_vlog(LOG_ERR, "%s: Cannot allocate memory\n", __func__);
        return -1;
    }

    tok = strdup(value);
    tok = strtok(tok, ",");

    i = 1;

    while (tok != NULL) {
        len = strlen(tok);
        len = MCT_OFFLINE_LOGSTORAGE_MIN(len, 4);

        strncpy((*names + y), tok, len);

        if ((num > 1) && (i < num))
            strncpy((*names + y + len), ",", 2);

        y += len + 1;

        i++;
        tok = strtok(NULL, ",");
    }

    free(tok);

    return 0;
}

static int mct_logstorage_set_number(unsigned int *number, unsigned int value)
{
    if ((value == 0) || (value > UINT_MAX)) {
        mct_log(LOG_ERR, "Invalid, is not a number \n");
        return -1;
    }

    *number = value;

    return 0;
}

/**
 * mct_logstorage_read_number
 *
 * Evaluate file size and number of files given in config file and set file size
 * The file number is checked by converting a string to an unsigned integer
 * width 0 > result < UINT_MAX (excludes 0!)
 * Non-digit characters including spaces and out of boundary will lead to an
 * error -1.
 *
 * @param number       Number to be read
 * @param value        string given in config file
 * @return             0 on success, -1 on error
 */
static int mct_logstorage_read_number(unsigned int *number, char *value)
{
    int i = 0;
    int len = 0;
    unsigned long size = 0;

    if (value == NULL)
        return -1;

    *number = 0;
    len = strlen(value);

    /* check if string consists of digits only */
    for (i = 0; i < len; i++)
        if (!isdigit(value[i])) {
            mct_log(LOG_ERR, "Invalid, is not a number \n");
            return -1;
        }

    size = strtoul(value, NULL, 10);

    return mct_logstorage_set_number(number, size);
}

/**
 * mct_logstorage_get_keys_list
 *
 * Obtain key list and number of keys for id list passed
 * after splitting it between seperator (,)
 *
 * @param ids            ID's
 * @param sep            Seperator
 * @param list           Prepared key list is stored here
 * @param numids         Number of keys in the list is stored here
 * @return: 0 on success, error on failure*
 */
static int mct_logstorage_get_keys_list(char *ids, char *sep, char **list,
                                            int *numids)
{
    char *token = NULL;
    char *tmp_token = NULL;
    char *ids_local = NULL;

    *numids = 0;

    /* Duplicate the ids passed for using in strtok_r() */
    ids_local = strdup(ids);

    if (ids_local == NULL)
        return -1;

    token = strtok_r(ids_local, sep, &tmp_token);

    if (token == NULL) {
        free(ids_local);
        return -1;
    }

    *list = (char *)calloc(MCT_OFFLINE_LOGSTORAGE_MAXIDS * (MCT_ID_SIZE + 1),
                           sizeof(char));

    if (*(list) == NULL) {
        free(ids_local);
        return -1;
    }

    while (token != NULL) {
        /* If it reached the max then other ids are ignored */
        if (*numids >= MCT_OFFLINE_LOGSTORAGE_MAXIDS) {
            free(ids_local);
            return 0;
        }

        strncpy(((*list) + ((*numids) * (MCT_ID_SIZE + 1))), token,
                MCT_ID_SIZE);
        *numids = *numids + 1;
        token = strtok_r(NULL, sep, &tmp_token);
    }

    free(ids_local);

    return 0;
}

static bool mct_logstorage_check_excluded_ids(char *id, char *delim, char *excluded_ids)
{
    char *token = NULL;
    char *tmp_token = NULL;
    char *ids_local = NULL;

    if ((id == NULL) || (delim == NULL) || (excluded_ids == NULL)) {
        mct_vlog(LOG_ERR, "%s: Invalid parameters\n", __func__);
        return false;
    }

    ids_local = strdup(excluded_ids);

    if (ids_local == NULL) {
        mct_vlog(LOG_ERR, "%s: Cannot duplicate string.\n", __func__);
        return false;
    }

    token = strtok_r(ids_local, delim, &tmp_token);

    if (token == NULL) {
        mct_vlog(LOG_ERR, "%s: %s could not be parsed.\n", __func__, ids_local);
        free(ids_local);
        return false;
    }

    while (token != NULL) {
        if(strncmp(id, token, MCT_ID_SIZE) == 0) {
            free(ids_local);
            return true;
        }

        token = strtok_r(NULL, delim, &tmp_token);
    }

    free(ids_local);
    return false;
}

/**
 * mct_logstorage_create_keys_only_ctid
 *
 * Prepares keys with context ID alone, will use ecuid if provided
 * (ecuid\:\:ctid) or (\:\:ctid)
 *
 * @param ecuid          ECU ID
 * @param ctid           Context ID
 * @param key            Prepared key stored here
 * @return               None
 */
static void mct_logstorage_create_keys_only_ctid(char *ecuid, char *ctid,
                                                     char *key)
{
    char curr_str[MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN + 1] = { 0 };
    int curr_len = 0;

    if (ecuid != NULL) {
        strncpy(curr_str, ecuid, strlen(ecuid));
        strncat(curr_str, "::", 2);
    }
    else {
        strncpy(curr_str, "::", 2);
    }

    curr_len = strlen(ctid);
    strncat(curr_str, ctid, curr_len);
    curr_len = strlen(curr_str);

    strncpy(key, curr_str, curr_len);
}

/**
 * mct_logstorage_create_keys_only_apid
 *
 * Prepares keys with application ID alone, will use ecuid if provided
 * (ecuid:apid::) or (:apid::)
 *
 * @param ecuid          ECU ID
 * @param apid           Application ID
 * @param key            Prepared key stored here
 * @return               None
 */
static void mct_logstorage_create_keys_only_apid(char *ecuid, char *apid,
                                                     char *key)
{
    char curr_str[MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN + 1] = { 0 };
    int curr_len = 0;

    if (ecuid != NULL) {
        strncpy(curr_str, ecuid, strlen(ecuid));
        strncat(curr_str, ":", 1);
    }
    else {
        strncpy(curr_str, ":", 1);
    }

    curr_len = strlen(apid);
    strncat(curr_str, apid, curr_len);
    strncat(curr_str, ":", 1);
    curr_len = strlen(curr_str);

    strncpy(key, curr_str, curr_len);
}

/**
 * mct_logstorage_create_keys_multi
 *
 * Prepares keys with apid, ctid (ecuid:apid:ctid), will use ecuid if is provided
 * (ecuid:apid:ctid) or (:apid:ctid)
 *
 * @param ecuid          ECU ID
 * @param apid           Application ID
 * @param ctid           Context ID
 * @param key            Prepared key stored here
 * @return               None
 */
static void mct_logstorage_create_keys_multi(char *ecuid, char *apid,
                                                 char *ctid, char *key)
{
    char curr_str[MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN + 1] = { 0 };
    int curr_len = 0;

    if (ecuid != NULL) {
        strncpy(curr_str, ecuid, strlen(ecuid));
        strncat(curr_str, ":", 1);
    }
    else {
        strncpy(curr_str, ":", 1);
    }

    curr_len = strlen(apid);
    strncat(curr_str, apid, curr_len);
    strncat(curr_str, ":", 1);

    curr_len = strlen(ctid);
    strncat(curr_str, ctid, curr_len);
    curr_len = strlen(curr_str);

    strncpy(key, curr_str, curr_len);
}

/**
 * mct_logstorage_create_keys_only_ecu
 *
 * Prepares keys with only ecuid (ecuid::)
 *
 * @param ecuid          ECU ID
 * @param key            Prepared key stored here
 * @return               None
 */
static void mct_logstorage_create_keys_only_ecu(char *ecuid, char *key)
{
    char curr_str[MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN + 1] = { 0 };

    strncpy(curr_str, ecuid, strlen(ecuid));
    strncat(curr_str, "::", 2);

    strncpy(key, curr_str, strlen(curr_str));
}

/**
 * mct_logstorage_create_keys
 *
 * Create keys for hash table
 *
 * From each section [filter] in offline logstorage configuration file, we
 * receive application and context id strings.
 * Application and context id can consist of
 * - a 4char long name
 * - a comma separated list of ids
 * - a wildcard: .*
 *
 * If both application and context id are set to wildcard, this will be treated
 * in the same way of the case application and context id are not present:
 * - EcuID must be specified
 *
 * If lists given for application and/or context id, all possible combinations
 * are returned as keys in a form "[apid][ctid], e.g. "APP1\:CTX1".
 * If wildcards are used, the non-wildcard value becomes the key, e.g. "APP1\:"
 * or "\:CTX2".
 *
 * @param[in] apids string given from filter configuration
 * @param[in] ctids string given from filter configuration
 * @param[in] ecuid string given from filter configuration
 * @param[out] keys keys to fill into hash table
 * @param[out] num_keys number of keys
 * @return: 0 on success, error on failure*
 */
static int mct_logstorage_create_keys(char *apids,
                                          char *ctids,
                                          char *ecuid,
                                          char **keys,
                                          int *num_keys)
{
    int i, j;
    int num_apids = 0;
    int num_ctids = 0;
    char *apid_list = NULL;
    char *ctid_list = NULL;
    char *curr_apid = NULL;
    char *curr_ctid = NULL;
    char curr_key[MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN + 1] = { 0 };
    int num_currkey = 0;

    /* Handle ecuid alone case here */
    if (((apids == NULL) && (ctids == NULL) && (ecuid != NULL)) ||
        ((apids != NULL) && (strncmp(apids, ".*", 2) == 0) &&
         (ctids != NULL) && (strncmp(ctids, ".*", 2) == 0) && (ecuid != NULL)) ) {
        mct_logstorage_create_keys_only_ecu(ecuid, curr_key);
        *(num_keys) = 1;
        *(keys) = (char *)calloc(*num_keys * MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN,
                                 sizeof(char));

        if (*(keys) == NULL)
            return -1;

        strncpy(*keys, curr_key, strlen(curr_key));
        return 0;
    }

    if ((apids == NULL) || (ctids == NULL)) {
        mct_log(LOG_ERR, "Required inputs (apid and ctid) are NULL\n");
        return -1;
    }

    /* obtain key list and number of keys for application ids */
    if (mct_logstorage_get_keys_list(apids, ",", &apid_list, &num_apids) != 0) {
        mct_log(LOG_ERR, "Failed to obtain apid, check configuration file \n");
        return -1;
    }

    /* obtain key list and number of keys for context ids */
    if (mct_logstorage_get_keys_list(ctids, ",", &ctid_list, &num_ctids) != 0) {
        mct_log(LOG_ERR, "Failed to obtain ctid, check configuration file \n");
        free(apid_list);
        return -1;
    }

    *(num_keys) = num_apids * num_ctids;

    /* allocate memory for needed number of keys */
    *(keys) = (char *)calloc(*num_keys * MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN,
                             sizeof(char));

    if (*(keys) == NULL) {
        free(apid_list);
        free(ctid_list);
        return -1;
    }

    /* store all combinations of apid ctid in keys */
    for (i = 0; i < num_apids; i++) {
        curr_apid = apid_list + (i * (MCT_ID_SIZE + 1));

        for (j = 0; j < num_ctids; j++) {
            curr_ctid = ctid_list + (j * (MCT_ID_SIZE + 1));

            if (strncmp(curr_apid, ".*", 2) == 0) /* only context id matters */
                mct_logstorage_create_keys_only_ctid(ecuid, curr_ctid, curr_key);
            else if (strncmp(curr_ctid, ".*", 2) == 0) /* only app id matters*/
                mct_logstorage_create_keys_only_apid(ecuid, curr_apid, curr_key);
            else /* key is combination of all */
                mct_logstorage_create_keys_multi(ecuid, curr_apid, curr_ctid, curr_key);

            strncpy((*keys + (num_currkey * MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN)),
                    curr_key, strlen(curr_key));
            num_currkey += 1;
            memset(&curr_key[0], 0, sizeof(curr_key));
        }
    }

    free(apid_list);
    free(ctid_list);

    return 0;
}

/**
 * mct_logstorage_prepare_table
 *
 * Prepares hash table with keys and data
 *
 * @param handle         MCT Logstorage handle
 * @param data           Holds all other configuration values
 * @return               0 on success, -1 on error
 */
static int mct_logstorage_prepare_table(MctLogStorage *handle,
                                            MctLogStorageFilterConfig *data)
{
    int ret = 0;
    int num_keys = 0;
    int found = 0;
    char *keys = NULL;
    MctNewestFileName *tmp = NULL;
    MctNewestFileName *prev_tmp = NULL;
    MctNewestFileName *new_tmp = NULL;

    if ((handle == NULL) || (data == NULL)) {
        mct_vlog(LOG_ERR, "Invalid parameters in %s\n", __func__);
        return -1;
    }

    ret = mct_logstorage_create_keys(data->apids,
                                     data->ctids,
                                     data->ecuid,
                                     &keys,
                                     &num_keys);

    if (ret != 0) {
        mct_log(LOG_ERR, "Not able to create keys for hash table\n");
        return -1;
    }

    /* hash_add */
    if (mct_logstorage_list_add(keys,
                                num_keys,
                                data,
                                &(handle->config_list)) != 0)
    {
        mct_log(LOG_ERR, "Adding to hash table failed, returning failure\n");
        mct_logstorage_free(handle, MCT_LOGSTORAGE_SYNC_ON_ERROR);
        free(keys);
        keys = NULL;
        return -1;
    }

    if (data->file_name) {
        if (handle->newest_file_list != NULL) {
            tmp = handle->newest_file_list;
            while (tmp) {
                if (strcmp(tmp->file_name, data->file_name) == 0) {
                    found = 1;
                    break;
                }
                else {
                    prev_tmp = tmp;
                    tmp = tmp->next;
                }
            }
        }

        if (!found) {
            new_tmp = calloc(1, sizeof(MctNewestFileName));
            if (new_tmp == NULL) {
                /* In this case, the existing list does not need to be freed.*/
                mct_vlog(LOG_ERR,
                        "Failed to allocate memory for new file name [%s]\n",
                        data->file_name);
                free(keys);
                keys = NULL;
                return -1;
            }
            new_tmp->file_name = strdup(data->file_name);
            new_tmp->newest_file = NULL;
            new_tmp->next = NULL;

            if (handle->newest_file_list == NULL)
                handle->newest_file_list = new_tmp;
            else
                prev_tmp->next = new_tmp;
        }
    }

    free(keys);
    keys = NULL;
    return 0;
}

/**
 * mct_logstorage_validate_filter_name
 *
 * Validates if the provided filter name is as required [FILTER<number>]
 *
 * @param name           Filter name
 * @return               0 on success, -1 on error
 *
 */
static int mct_logstorage_validate_filter_name(char *name)
{
    int len = 0;
    int idx = 0;
    int config_sec_len = strlen(MCT_OFFLINE_LOGSTORAGE_CONFIG_SECTION);
    int storage_sec_len = strlen(MCT_OFFLINE_LOGSTORAGE_NONVERBOSE_STORAGE_SECTION);
    int control_sec_len = strlen(MCT_OFFLINE_LOGSTORAGE_NONVERBOSE_CONTROL_SECTION);

    if (name == NULL)
        return -1;

    len = strlen(name);

    /* Check if section header is of format "FILTER" followed by a number */
    if (strncmp(name,
                MCT_OFFLINE_LOGSTORAGE_CONFIG_SECTION,
                config_sec_len) == 0) {
        for (idx = config_sec_len; idx < len - 1; idx++)
            if (!isdigit(name[idx]))
                return -1;

        return 0;
    }
    /* Check if section header is of format "FILTER" followed by a number */
    else if (strncmp(name,
                     MCT_OFFLINE_LOGSTORAGE_NONVERBOSE_STORAGE_SECTION,
                     storage_sec_len) == 0)
    {
        for (idx = storage_sec_len; idx < len - 1; idx++)
            if (!isdigit(name[idx]))
                return -1;

        return 0;
    }
    /* Check if section header is of format "FILTER" followed by a number */
    else if (strncmp(name,
                     MCT_OFFLINE_LOGSTORAGE_NONVERBOSE_CONTROL_SECTION,
                     control_sec_len) == 0)
    {
        for (idx = control_sec_len; idx < len - 1; idx++)
            if (!isdigit(name[idx]))
                return -1;

        return 0;
    }
    else {
        return -1;
    }
}

static void mct_logstorage_filter_set_strategy(MctLogStorageFilterConfig *config,
                                                   int strategy)
{
    if (config == NULL)
        return;

    /* file based */
    if ((strategy == MCT_LOGSTORAGE_SYNC_ON_MSG) ||
        (strategy == MCT_LOGSTORAGE_SYNC_UNSET)) {
        config->mct_logstorage_prepare = &mct_logstorage_prepare_on_msg;
        config->mct_logstorage_write = &mct_logstorage_write_on_msg;
        config->mct_logstorage_sync = &mct_logstorage_sync_on_msg;
    }
    else { /* cache based */
        config->mct_logstorage_prepare = &mct_logstorage_prepare_msg_cache;
        config->mct_logstorage_write = &mct_logstorage_write_msg_cache;
        config->mct_logstorage_sync = &mct_logstorage_sync_msg_cache;
    }
}

static int mct_logstorage_check_apids(MctLogStorageFilterConfig *config,
                                          char *value)
{
    if ((config == NULL) || (value == NULL)) {
        mct_log(LOG_ERR, "Not able to create keys for hash table\n");
        return -1;
    }

    return mct_logstorage_read_list_of_names(&config->apids, value);
}

static int mct_logstorage_check_ctids(MctLogStorageFilterConfig *config,
                                          char *value)
{
    if ((config == NULL) || (value == NULL))
        return -1;

    return mct_logstorage_read_list_of_names(&config->ctids, (const char*)value);
}

static int mct_logstorage_store_config_excluded_apids(MctLogStorageFilterConfig *config,
                                          char *value)
{
    if ((config == NULL) || (value == NULL)) {
        mct_vlog(LOG_ERR, "%s: Invalid parameters\n", __func__);
        return -1;
    }

    return mct_logstorage_read_list_of_names(&config->excluded_apids, value);
}

static int mct_logstorage_store_config_excluded_ctids(MctLogStorageFilterConfig *config,
                                          char *value)
{
    if ((config == NULL) || (value == NULL)) {
        mct_vlog(LOG_ERR, "%s: Invalid parameters\n", __func__);
        return -1;
    }

    return mct_logstorage_read_list_of_names(&config->excluded_ctids, (const char*)value);
}

static int mct_logstorage_set_loglevel(int *log_level,
                                           int value)
{
    *log_level = value;
    if ((value <= MCT_LOG_DEFAULT) || (value >= MCT_LOG_MAX)) {
        *log_level = -1;
        mct_log(LOG_ERR, "Invalid log level \n");
        return -1;
    }
    return 0;
}

static int mct_logstorage_check_loglevel(MctLogStorageFilterConfig *config,
                                             char *value)
{
    int ll = -1;

    if ((config == NULL) || (value == NULL))
        return -1;

    if (value == NULL) {
        config->log_level = 0;
        return -1;
    }

    if (strcmp(value, "MCT_LOG_FATAL") == 0) {
        ll = 1;
    }
    else if (strcmp(value, "MCT_LOG_ERROR") == 0)
    {
        ll = 2;
    }
    else if (strcmp(value, "MCT_LOG_WARN") == 0)
    {
        ll = 3;
    }
    else if (strcmp(value, "MCT_LOG_INFO") == 0)
    {
        ll = 4;
    }
    else if (strcmp(value, "MCT_LOG_DEBUG") == 0)
    {
        ll = 5;
    }
    else if (strcmp(value, "MCT_LOG_VERBOSE") == 0)
    {
        ll = 6;
    }

    return mct_logstorage_set_loglevel(&config->log_level, ll);
}

static int mct_logstorage_check_reset_loglevel(MctLogStorageFilterConfig *config,
                                                   char *value)
{
    if (config == NULL)
        return -1;

    if (value == NULL) {
        config->reset_log_level = 0;
        return -1;
    }

    if (strcmp(value, "MCT_LOG_OFF") == 0) {
        config->reset_log_level = MCT_LOG_OFF;
    }
    else if (strcmp(value, "MCT_LOG_FATAL") == 0)
    {
        config->reset_log_level = MCT_LOG_FATAL;
    }
    else if (strcmp(value, "MCT_LOG_ERROR") == 0)
    {
        config->reset_log_level = MCT_LOG_ERROR;
    }
    else if (strcmp(value, "MCT_LOG_WARN") == 0)
    {
        config->reset_log_level = MCT_LOG_WARN;
    }
    else if (strcmp(value, "MCT_LOG_INFO") == 0)
    {
        config->reset_log_level = MCT_LOG_INFO;
    }
    else if (strcmp(value, "MCT_LOG_DEBUG") == 0)
    {
        config->reset_log_level = MCT_LOG_DEBUG;
    }
    else if (strcmp(value, "MCT_LOG_VERBOSE") == 0)
    {
        config->reset_log_level = MCT_LOG_VERBOSE;
    }
    else {
        config->reset_log_level = -1;
        mct_log(LOG_ERR, "Invalid log level \n");
        return -1;
    }

    return 0;
}

static int mct_logstorage_check_filename(MctLogStorageFilterConfig *config,
                                             char *value)
{
    int len;

    if ((value == NULL) || (strcmp(value, "") == 0)) {
        mct_vlog(LOG_ERR, "%s: Arguments are set to NULL\n", __func__);
        return -1;
    }

    if (config->file_name != NULL) {
        free(config->file_name);
        config->file_name = NULL;
    }

    len = strlen(value);

    if (len == 0) {
        mct_vlog(LOG_ERR, "%s: Length of string given in config file is 0\n",
                 __func__);
        return -1;
    }

    /* do not allow the user to change directory by adding a relative path */
    if (strstr(value, "..") == NULL) {
        config->file_name = calloc((len + 1), sizeof(char));

        if (config->file_name == NULL) {
            mct_log(LOG_ERR,
                    "Cannot allocate memory for filename\n");
            return -1;
        }

        strncpy(config->file_name, value, len);
    }
    else {
        mct_log(LOG_ERR,
                "Invalid filename, paths not accepted due to security issues\n");
        return -1;
    }

    return 0;
}

static int mct_logstorage_check_filesize(MctLogStorageFilterConfig *config,
                                             char *value)
{
    if ((config == NULL) || (value == NULL))
        return -1;

    return mct_logstorage_read_number(&config->file_size, value);
}

static int mct_logstorage_check_nofiles(MctLogStorageFilterConfig *config,
                                            char *value)
{
    if ((config == NULL) || (value == NULL))
        return -1;

    return mct_logstorage_read_number(&config->num_files, value);
}

static int mct_logstorage_check_specificsize(MctLogStorageFilterConfig *config,
                                                 char *value)
{
    if ((config == NULL) || (value == NULL))
        return -1;

    return mct_logstorage_read_number(&config->specific_size, value);
}

/**
 * mct_logstorage_check_sync_strategy
 *
 * Evaluate sync strategy. The sync strategy is an optional filter
 * configuration parameter.
 * If the given value cannot be associated with a sync strategy, the default
 * sync strategy will be assigned.
 *
 * @param config       MctLogStorageFilterConfig
 * @param value        string given in config file
 * @return             0 on success, -1 on error
 */
static int mct_logstorage_check_sync_strategy(MctLogStorageFilterConfig *config,
                                                  char *value)
{
    if ((config == NULL) || (value == NULL))
        return -1;

    if (strcasestr(value, "ON_MSG") != NULL) {
        config->sync = MCT_LOGSTORAGE_SYNC_ON_MSG;
        mct_log(LOG_DEBUG, "ON_MSG found, ignore other if added\n");
    }
    else { /* ON_MSG not set, combination of cache based strategies possible */

        if (strcasestr(value, "ON_DAEMON_EXIT") != NULL)
            config->sync |= MCT_LOGSTORAGE_SYNC_ON_DAEMON_EXIT;

        if (strcasestr(value, "ON_DEMAND") != NULL)
            config->sync |= MCT_LOGSTORAGE_SYNC_ON_DEMAND;

        if (strcasestr(value, "ON_DEVICE_DISCONNECT") != NULL)
            config->sync |= MCT_LOGSTORAGE_SYNC_ON_DEVICE_DISCONNECT;

        if (strcasestr(value, "ON_SPECIFIC_SIZE") != NULL)
            config->sync |= MCT_LOGSTORAGE_SYNC_ON_SPECIFIC_SIZE;

        if (strcasestr(value, "ON_FILE_SIZE") != NULL)
            config->sync |= MCT_LOGSTORAGE_SYNC_ON_FILE_SIZE;

        if (config->sync == 0) {
            mct_log(LOG_WARNING,
                    "Unknown sync strategies. Set default ON_MSG\n");
            config->sync = MCT_LOGSTORAGE_SYNC_ON_MSG;
            return 1;
        }
    }

    return 0;
}

/**
 * mct_logstorage_check_overwrite_strategy
 *
 * Evaluate overwrite strategy. The sync strategy is an optional filter
 * configuration parameter.
 * If the given value cannot be associated with a strategy, the default
 * strategy will be assigned.
 *
 * @param[in] config    MctLogStorageFilterConfig
 * @param[in] value     string given in config file
 * @return              0 on success, 1 on unknown value, -1 on error
 */
static int mct_logstorage_check_overwrite_strategy(MctLogStorageFilterConfig *config,
                                                  char *value)
{
    if ((config == NULL) || (value == NULL))
        return -1;

    if (strcasestr(value, "DISCARD_OLD") != NULL) {
        config->overwrite = MCT_LOGSTORAGE_OVERWRITE_DISCARD_OLD;
    } else if (strcasestr(value, "DISCARD_NEW") != NULL) {
        config->overwrite = MCT_LOGSTORAGE_OVERWRITE_DISCARD_NEW;
    } else {
        mct_log(LOG_WARNING,
                "Unknown overwrite strategy. Set default DISCARD_OLD\n");
        config->overwrite = MCT_LOGSTORAGE_OVERWRITE_DISCARD_OLD;
        return 1;
    }

    return 0;
}

/**
 * mct_logstorage_check_disable_network
 *
 * Evaluate disable network. The disable network is an optional filter
 * configuration parameter.
 * If the given value cannot be associated with a flag, the default
 * flag will be assigned.
 *
 * @param[in] config    MctLogStorageFilterConfig
 * @param[in] value     string given in config file
 * @return              0 on success, 1 on unknown value, -1 on error
 */
static int mct_logstorage_check_disable_network(MctLogStorageFilterConfig *config,
                                                  char *value)
{
    if ((config == NULL) || (value == NULL))
        return -1;

    if (strcasestr(value, "ON") != NULL) {
        config->disable_network_routing = MCT_LOGSTORAGE_DISABLE_NW_ON;
    } else if (strcasestr(value, "OFF") != NULL) {
        config->disable_network_routing = MCT_LOGSTORAGE_DISABLE_NW_OFF;
    } else {
        mct_log(LOG_WARNING,
                "Unknown disable network flag. Set default OFF\n");
        config->disable_network_routing = MCT_LOGSTORAGE_DISABLE_NW_OFF;
        return 1;
    }

    return 0;
}

/**
 * mct_logstorage_check_ecuid
 *
 * Evaluate if ECU idenfifier given in config file
 *
 * @param config       MctLogStorageFilterConfig
 * @param value        string given in config file
 * @return             0 on success, -1 on error
 */
static int mct_logstorage_check_ecuid(MctLogStorageFilterConfig *config,
                                          char *value)
{
    int len;

    if ((config == NULL) || (value == NULL) || (value[0] == '\0'))
        return -1;

    if (config->ecuid != NULL) {
        free(config->ecuid);
        config->ecuid = NULL;
    }

    len = strlen(value);
    config->ecuid = calloc((len + 1), sizeof(char));

    if (config->ecuid == NULL)
        return -1;

    strncpy(config->ecuid, value, len);

    return 0;
}

static MctLogstorageFilterConf
    filter_cfg_entries[MCT_LOGSTORAGE_FILTER_CONF_COUNT] = {
    [MCT_LOGSTORAGE_FILTER_CONF_LOGAPPNAME] = {
        .key = "LogAppName",
        .func = mct_logstorage_check_apids,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_CONTEXTNAME] = {
        .key = "ContextName",
        .func = mct_logstorage_check_ctids,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_EXCLUDED_LOGAPPNAME] = {
        .key = "ExcludedLogAppName",
        .func = mct_logstorage_store_config_excluded_apids,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_EXCLUDED_CONTEXTNAME] = {
        .key = "ExcludedContextName",
        .func = mct_logstorage_store_config_excluded_ctids,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_LOGLEVEL] = {
        .key = "LogLevel",
        .func = mct_logstorage_check_loglevel,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_RESET_LOGLEVEL] = {
        .key = NULL,
        .func = mct_logstorage_check_reset_loglevel,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_FILE] = {
        .key = "File",
        .func = mct_logstorage_check_filename,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_FILESIZE] = {
        .key = "FileSize",
        .func = mct_logstorage_check_filesize,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_NOFILES] = {
        .key = "NOFiles",
        .func = mct_logstorage_check_nofiles,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_SYNCBEHAVIOR] = {
        .key = "SyncBehavior",
        .func = mct_logstorage_check_sync_strategy,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_OVERWRITEBEHAVIOR] = {
        .key = "OverwriteBehavior",
        .func = mct_logstorage_check_overwrite_strategy,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_ECUID] = {
        .key = "EcuID",
        .func = mct_logstorage_check_ecuid,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_SPECIFIC_SIZE] = {
        .key = "SpecificSize",
        .func = mct_logstorage_check_specificsize,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_DISABLE_NETWORK] = {
        .key = "DisableNetwork",
        .func = mct_logstorage_check_disable_network,
        .is_opt = 1
    }
};

/* */
static MctLogstorageFilterConf
    filter_nonverbose_storage_entries[MCT_LOGSTORAGE_FILTER_CONF_COUNT] = {
    [MCT_LOGSTORAGE_FILTER_CONF_LOGAPPNAME] = {
        .key = NULL,
        .func = mct_logstorage_check_apids,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_CONTEXTNAME] = {
        .key = NULL,
        .func = mct_logstorage_check_ctids,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_EXCLUDED_LOGAPPNAME] = {
        .key = NULL,
        .func = mct_logstorage_store_config_excluded_apids,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_EXCLUDED_CONTEXTNAME] = {
        .key = NULL,
        .func = mct_logstorage_store_config_excluded_ctids,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_LOGLEVEL] = {
        .key = NULL,
        .func = mct_logstorage_check_loglevel,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_RESET_LOGLEVEL] = {
        .key = NULL,
        .func = NULL,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_FILE] = {
        .key = "File",
        .func = mct_logstorage_check_filename,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_FILESIZE] = {
        .key = "FileSize",
        .func = mct_logstorage_check_filesize,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_NOFILES] = {
        .key = "NOFiles",
        .func = mct_logstorage_check_nofiles,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_SYNCBEHAVIOR] = {
        .key = NULL,
        .func = mct_logstorage_check_sync_strategy,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_OVERWRITEBEHAVIOR] = {
        .key = NULL,
        .func = mct_logstorage_check_overwrite_strategy,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_ECUID] = {
        .key = "EcuID",
        .func = mct_logstorage_check_ecuid,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_SPECIFIC_SIZE] = {
        .key = NULL,
        .func = mct_logstorage_check_specificsize,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_DISABLE_NETWORK] = {
        .key = NULL,
        .func = mct_logstorage_check_disable_network,
        .is_opt = 1
    }
};

static MctLogstorageFilterConf
    filter_nonverbose_control_entries[MCT_LOGSTORAGE_FILTER_CONF_COUNT] = {
    [MCT_LOGSTORAGE_FILTER_CONF_LOGAPPNAME] = {
        .key = "LogAppName",
        .func = mct_logstorage_check_apids,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_CONTEXTNAME] = {
        .key = "ContextName",
        .func = mct_logstorage_check_ctids,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_EXCLUDED_LOGAPPNAME] = {
        .key = NULL,
        .func = mct_logstorage_store_config_excluded_apids,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_EXCLUDED_CONTEXTNAME] = {
        .key = NULL,
        .func = mct_logstorage_store_config_excluded_ctids,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_LOGLEVEL] = {
        .key = "LogLevel",
        .func = mct_logstorage_check_loglevel,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_RESET_LOGLEVEL] = {
        .key = "ResetLogLevel",
        .func = mct_logstorage_check_reset_loglevel,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_FILE] = {
        .key = NULL,
        .func = mct_logstorage_check_filename,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_FILESIZE] = {
        .key = NULL,
        .func = mct_logstorage_check_filesize,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_NOFILES] = {
        .key = NULL,
        .func = mct_logstorage_check_nofiles,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_SYNCBEHAVIOR] = {
        .key = NULL,
        .func = mct_logstorage_check_sync_strategy,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_OVERWRITEBEHAVIOR] = {
        .key = NULL,
        .func = mct_logstorage_check_overwrite_strategy,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_ECUID] = {
        .key = "EcuID",
        .func = mct_logstorage_check_ecuid,
        .is_opt = 0
    },
    [MCT_LOGSTORAGE_FILTER_CONF_SPECIFIC_SIZE] = {
        .key = NULL,
        .func = mct_logstorage_check_specificsize,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_FILTER_CONF_DISABLE_NETWORK] = {
        .key = NULL,
        .func = mct_logstorage_check_disable_network,
        .is_opt = 1
    }
};

/**
 * Check filter configuration parameter is valid.
 *
 * @param config MctLogStorageFilterConfig
 * @param ctype  MctLogstorageFilterConfType
 * @param value specified property value from configuration file
 * @return 0 on success, -1 otherwise
 */
static int mct_logstorage_check_param(MctLogStorageFilterConfig *config,
                                          MctLogstorageFilterConfType ctype,
                                          char *value)
{
    if ((config == NULL) || (value == NULL))
        return -1;

    if (ctype < MCT_LOGSTORAGE_FILTER_CONF_COUNT)
        return filter_cfg_entries[ctype].func(config, value);

    return -1;
}

static int mct_logstorage_get_filter_section_value(MctConfigFile *config_file,
                                                       char *sec_name,
                                                       MctLogstorageFilterConf entry,
                                                       char *value)
{
    int ret = 0;

    if ((config_file == NULL) || (sec_name == NULL))
        return MCT_OFFLINE_LOGSTORAGE_FILTER_ERROR;

    if (entry.key != NULL) {
        ret = mct_config_file_get_value(config_file, sec_name,
                                        entry.key,
                                        value);

        if ((ret != 0) && (entry.is_opt == 0)) {
            mct_vlog(LOG_WARNING,
                     "Invalid configuration in section: %s -> %s : %s\n",
                     sec_name, entry.key, value);
            return MCT_OFFLINE_LOGSTORAGE_FILTER_ERROR;
        }

        if ((ret != 0) && (entry.is_opt == 1)) {
            mct_vlog(LOG_DEBUG, "Optional parameter %s not specified\n",
                     entry.key);
            return MCT_OFFLINE_LOGSTORAGE_FILTER_CONTINUE;
        }
    }
    else {
        return MCT_OFFLINE_LOGSTORAGE_FILTER_CONTINUE;
    }

    return 0;
}

static int mct_logstorage_get_filter_value(MctConfigFile *config_file,
                                               char *sec_name,
                                               int index,
                                               char *value)
{
    int ret = 0;
    int config_sec_len = strlen(MCT_OFFLINE_LOGSTORAGE_CONFIG_SECTION);
    int storage_sec_len = strlen(MCT_OFFLINE_LOGSTORAGE_NONVERBOSE_STORAGE_SECTION);
    int control_sec_len = strlen(MCT_OFFLINE_LOGSTORAGE_NONVERBOSE_CONTROL_SECTION);

    if ((config_file == NULL) || (sec_name == NULL))
        return MCT_OFFLINE_LOGSTORAGE_FILTER_ERROR;

    /* Branch based on section name, no complete string compare needed */
    if (strncmp(sec_name,
                MCT_OFFLINE_LOGSTORAGE_CONFIG_SECTION,
                config_sec_len) == 0) {
        ret = mct_logstorage_get_filter_section_value(config_file, sec_name,
                                                      filter_cfg_entries[index],
                                                      value);
    }
    else if (strncmp(sec_name,
                     MCT_OFFLINE_LOGSTORAGE_NONVERBOSE_STORAGE_SECTION,
                     storage_sec_len) == 0) {
        ret = mct_logstorage_get_filter_section_value(config_file, sec_name,
                                                      filter_nonverbose_storage_entries[index],
                                                      value);
    }
    else if ((strncmp(sec_name,
                      MCT_OFFLINE_LOGSTORAGE_NONVERBOSE_CONTROL_SECTION,
                      control_sec_len) == 0)) {
        ret = mct_logstorage_get_filter_section_value(config_file, sec_name,
                                                      filter_nonverbose_control_entries[index],
                                                      value);
    }
    else {
        mct_log(LOG_ERR, "Error: Section name not valid \n");
        ret = MCT_OFFLINE_LOGSTORAGE_FILTER_ERROR;
    }

    return ret;
}

static int mct_logstorage_setup_table(MctLogStorage *handle,
                                          MctLogStorageFilterConfig *tmp_data)
{
    int ret = 0;

    /* depending on the specified strategy set function pointers for
     * prepare, write and sync */
    mct_logstorage_filter_set_strategy(tmp_data, tmp_data->sync);

    ret = mct_logstorage_prepare_table(handle, tmp_data);

    if (ret != 0) {
        mct_vlog(LOG_ERR, "%s Error: Storing filter values failed\n", __func__);
        ret = MCT_OFFLINE_LOGSTORAGE_STORE_FILTER_ERROR;
    }

    return ret;
}
/*Return :
 * MCT_OFFLINE_LOGSTORAGE_FILTER_ERROR - On filter properties or value is not valid
 * MCT_OFFLINE_LOGSTORAGE_STORE_FILTER_ERROR - On error while storing in hash table
 */

static int mct_daemon_offline_setup_filter_properties(MctLogStorage *handle,
                                                          MctConfigFile *config_file,
                                                          char *sec_name)
{
    MctLogStorageFilterConfig tmp_data;
    char value[MCT_CONFIG_FILE_ENTRY_MAX_LEN + 1] = { '\0' };
    int i = 0;
    int ret = 0;

    if ((handle == NULL) || (config_file == NULL) || (sec_name == NULL))
        return MCT_OFFLINE_LOGSTORAGE_STORE_FILTER_ERROR;

    memset(&tmp_data, 0, sizeof(MctLogStorageFilterConfig));
    tmp_data.log_level = MCT_LOG_VERBOSE;
    tmp_data.reset_log_level = MCT_LOG_OFF;
    tmp_data.disable_network_routing = MCT_LOGSTORAGE_DISABLE_NW_OFF;

    for (i = 0; i < MCT_LOGSTORAGE_FILTER_CONF_COUNT; i++) {
        ret = mct_logstorage_get_filter_value(config_file, sec_name, i, value);

        if (ret == MCT_OFFLINE_LOGSTORAGE_FILTER_ERROR)
            return ret;

        if (ret == MCT_OFFLINE_LOGSTORAGE_FILTER_CONTINUE)
            continue;

        /* check value and store temporary */
        ret = mct_logstorage_check_param(&tmp_data, i, value);

        if (ret != 0) {
            if (tmp_data.apids != NULL) {
                free(tmp_data.apids);
                tmp_data.apids = NULL;
            }

            if (tmp_data.ctids != NULL) {
                free(tmp_data.ctids);
                tmp_data.ctids = NULL;
            }

            if (tmp_data.excluded_apids != NULL) {
                free(tmp_data.excluded_apids);
                tmp_data.excluded_apids = NULL;
            }

            if (tmp_data.excluded_ctids != NULL) {
                free(tmp_data.excluded_ctids);
                tmp_data.excluded_ctids = NULL;
            }

            if (tmp_data.file_name != NULL) {
                free(tmp_data.file_name);
                tmp_data.file_name = NULL;
            }

            if (tmp_data.working_file_name != NULL) {
                free(tmp_data.working_file_name);
                tmp_data.working_file_name = NULL;
            }

            if (tmp_data.ecuid != NULL) {
                free(tmp_data.ecuid);
                tmp_data.ecuid = NULL;
            }

            return MCT_OFFLINE_LOGSTORAGE_FILTER_ERROR;
        }
    }

    if(mct_logstorage_count_ids(tmp_data.excluded_apids) > 1 && mct_logstorage_count_ids(tmp_data.excluded_ctids) > 1) {
        mct_vlog(LOG_WARNING, "%s: Logstorage does not support both multiple excluded applications and contexts\n", __func__);
        return MCT_OFFLINE_LOGSTORAGE_FILTER_ERROR;
    }

    /* filter configuration is valid */
    ret = mct_logstorage_setup_table(handle, &tmp_data);

    if (ret != 0) {
        mct_vlog(LOG_ERR, "%s Error: Storing filter values failed\n", __func__);
        ret = MCT_OFFLINE_LOGSTORAGE_STORE_FILTER_ERROR;
    }
    else { /* move to next free filter configuration, if no error occurred */
        handle->num_configs += 1;
    }

    /* free tmp_data */
    mct_logstorage_filter_config_free(&tmp_data);

    return ret;
}

/**
 * mct_logstorage_check_ecuid
 *
 * Evaluate BlockMode setting given in config file
 *
 * @param handle       MctLogStorage
 * @param value        string given in config file
 * @return             0 on success, -1 on error
 */
static int mct_logstorage_check_blockmode(MctLogStorage *handle,
                                              char *value)
{
    if ((handle == NULL) || (value == NULL))
    {
        return -1;
    }

    if ((strncmp(value, "ON", 2) == 0) || (strncmp(value, "1", 1) == 0))
    {
        handle->block_mode = MCT_MODE_BLOCKING;
    }
    else if ((strncmp(value, "OFF", 3) == 0) || (strncmp(value, "0", 1) == 0))
    {
        handle->block_mode = MCT_MODE_NON_BLOCKING;
    }
    else
    {
        mct_vlog(LOG_ERR,
                 "Wrong value for BlockMode section name: %s\n", value);
        handle->block_mode = MCT_MODE_NON_BLOCKING;
        return -1;
    }

    return 0;
}

/**
 * mct_logstorage_check_maintain_logstorage_loglevel
 *
 * Evaluate to maintain the logstorage loglevel setting. This is an optional
 * configuration parameter
 * If the given value cannot be associated with an overwrite, the default value
 * will be assigned.
 *
 * @param config       MctLogStorage
 * @param value        string given in config file
 * @return             0 on success, -1 on error
 */
static int mct_logstorage_check_maintain_logstorage_loglevel(MctLogStorage *handle,
                                                  char *value)
{
    if ((handle == NULL) || (value == NULL))
    {
        return -1;
    }

    if ((strncmp(value, "OFF", 3) == 0) || (strncmp(value, "0", 1) == 0))
    {
        handle->maintain_logstorage_loglevel = MCT_MAINTAIN_LOGSTORAGE_LOGLEVEL_OFF;
    }
    else if ((strncmp(value, "ON", 2) == 0) || (strncmp(value, "1", 1) == 0))
    {
        handle->maintain_logstorage_loglevel = MCT_MAINTAIN_LOGSTORAGE_LOGLEVEL_ON;
    }
    else
    {
        mct_vlog(LOG_ERR,
                 "Wrong value for Maintain logstorage loglevel section name: %s\n", value);
        handle->maintain_logstorage_loglevel = MCT_MAINTAIN_LOGSTORAGE_LOGLEVEL_ON;
        return -1;
    }

    return 0;
}

static MctLogstorageGeneralConf
    general_cfg_entries[MCT_LOGSTORAGE_GENERAL_CONF_COUNT] = {
    [MCT_LOGSTORAGE_GENERAL_CONF_BLOCKMODE] = {
        .key = "BlockMode",
        .func = mct_logstorage_check_blockmode,
        .is_opt = 1
    },
    [MCT_LOGSTORAGE_GENERAL_CONF_MAINTAIN_LOGSTORAGE_LOGLEVEL] = {
        .key = "MaintainLogstorageLogLevel",
        .func = mct_logstorage_check_maintain_logstorage_loglevel,
        .is_opt = 1
    }
};

/**
 * Check if MctLogstorage General configuration parameter is valid.
 *
 * @param handle pointer to MctLogstorage structure
 * @param ctype Logstorage general configuration type
 * @param value specified property value from configuration file
 * @return 0 on success, -1 otherwise
 */
static int mct_logstorage_check_general_param(MctLogStorage *handle,
                                              MctLogstorageGeneralConfType ctype,
                                              char *value)
{
    if ((handle == NULL) || (value == NULL))
    {
        return -1;
    }

    if (ctype < MCT_LOGSTORAGE_GENERAL_CONF_COUNT)
    {
        return general_cfg_entries[ctype].func(handle, value);
    }

    return -1;
}

static int mct_daemon_setup_general_properties(MctLogStorage *handle,
                                               MctConfigFile *config_file,
                                               char *sec_name)
{
    MctLogstorageGeneralConfType type = MCT_LOGSTORAGE_GENERAL_CONF_BLOCKMODE;
    char value[MCT_CONFIG_FILE_ENTRY_MAX_LEN] = {0};

    if ((handle == NULL) || (config_file == NULL) || (sec_name == NULL))
    {
        return -1;
    }

    for ( ; type < MCT_LOGSTORAGE_GENERAL_CONF_COUNT ; type++)
    {
        if (mct_config_file_get_value(config_file,
                                      sec_name,
                                      general_cfg_entries[type].key,
                                      value) == 0)
        {
            if (mct_logstorage_check_general_param(handle, type, value) != 0)
            {
                mct_vlog(LOG_WARNING,
                         "General parameter %s [%s] is invalid\n",
                         general_cfg_entries[type].key, value);
            }
        }
        else
        {
            if (general_cfg_entries[type].is_opt == 1)
            {
                mct_vlog(LOG_DEBUG,
                         "Optional General parameter %s not given\n",
                         general_cfg_entries[type].key);
            }
            else
            {
                mct_vlog(LOG_ERR,
                         "General parameter %s not given\n",
                         general_cfg_entries[type].key);
                return -1;
            }
        }
    }

    return 0;
}

/**
 * mct_logstorage_store_filters
 *
 * This function reads the filter keys and values
 * and stores them into the hash map
 *
 * @param handle             MCT Logstorage handle
 * @param config_file_name   Configuration file name
 * @return                   0 on success, -1 on error, 1 on warning
 *
 */
static int mct_logstorage_store_filters(MctLogStorage *handle,
                                            char *config_file_name)
{
    MctConfigFile *config = NULL;
    int sec = 0;
    int num_sec = 0;
    int ret = 0;
    /* we have to make sure that this function returns success if atleast one
     * filter configuration is valid and stored */
    int valid = -1;

    if (config_file_name == NULL) {
        mct_vlog(LOG_ERR, "%s unexpected parameter received\n", __func__);
        return -1;
    }

    config = mct_config_file_init(config_file_name);

    if (config == NULL) {
        mct_log(LOG_CRIT, "Failed to open filter configuration file\n");
        return -1;
    }

    handle->block_mode = MCT_MODE_BLOCKING_UNDEF;
    handle->maintain_logstorage_loglevel = MCT_MAINTAIN_LOGSTORAGE_LOGLEVEL_UNDEF;
    mct_config_file_get_num_sections(config, &num_sec);

    for (sec = 0; sec < num_sec; sec++) {
        char sec_name[MCT_CONFIG_FILE_ENTRY_MAX_LEN + 1];

        if (mct_config_file_get_section_name(config, sec, sec_name) == -1) {
            mct_log(LOG_CRIT, "Failed to read section name\n");
            mct_config_file_release(config);
            return -1;
        }

        if (strstr(sec_name, GENERAL_BASE_NAME) != NULL) {
            if (mct_daemon_setup_general_properties(handle, config, sec_name) == -1)
            {
                mct_log(LOG_CRIT, "General configuration is invalid\n");
                continue;
            }
        }
        else if (mct_logstorage_validate_filter_name(sec_name) == 0)
        {
            ret = mct_daemon_offline_setup_filter_properties(handle, config, sec_name);

            if (ret == MCT_OFFLINE_LOGSTORAGE_STORE_FILTER_ERROR) {
                break;
            }
            else if (ret == MCT_OFFLINE_LOGSTORAGE_FILTER_ERROR)
            {
                valid = 1;
                mct_vlog(LOG_WARNING,
                         "%s filter configuration is invalid \n",
                         sec_name);
                /* Continue reading next filter section */
                continue;
            }
            else
            /* Filter properties read and stored successfuly */
            if (valid != 1)
                valid = 0;
        }
        else { /* unknown section */
            mct_vlog(LOG_WARNING, "Unknown section: %s", sec_name);
        }
    }

    mct_config_file_release(config);

    return valid;
}

/**
 * mct_logstorage_load_config
 *
 * Read mct_logstorage.conf file and setup filters in hash table
 * Hash table key consists of "APID:CTID", e.g "APP1:CTX1". If
 * wildcards used for application id or context id, the hash table
 * key consists of none wildcard value, e.g. apid=.*, cxid=CTX1
 * results in "CTX1".
 *
 * Combination of two wildcards is not allowed if ECUID is not specified.
 *
 * @param handle        MCT Logstorage handle
 * @return              0 on success, -1 on error, 1 on warning
 */
static int mct_logstorage_load_config(MctLogStorage *handle)
{
    char config_file_name[PATH_MAX] = {0};
    int ret = 0;

    /* Check if handle is NULL or already initialized or already configured  */
    if ((handle == NULL) ||
        (handle->connection_type != MCT_OFFLINE_LOGSTORAGE_DEVICE_CONNECTED))
        return -1;

    /* Check if this device config was already setup */
    if (handle->config_status == MCT_OFFLINE_LOGSTORAGE_CONFIG_DONE) {
        mct_vlog(LOG_ERR,
                 "%s: Device already configured. Send disconnect first.\n",
                 __func__);
        return -1;
    }

    if (snprintf(config_file_name,
                 PATH_MAX,
                 "%s/%s",
                 handle->device_mount_point,
                 MCT_OFFLINE_LOGSTORAGE_CONFIG_FILE_NAME) < 0) {
        mct_log(LOG_ERR,
                "Creating configuration file path string failed\n");
        return -1;
    }
    config_file_name[PATH_MAX - 1] = 0;
    ret = mct_logstorage_store_filters(handle, config_file_name);

    if (ret == 1) {
        handle->config_status = MCT_OFFLINE_LOGSTORAGE_CONFIG_DONE;
        return 1;
    }
    else if (ret != 0)
    {
        mct_log(LOG_ERR,
                "mct_logstorage_load_config Error : Storing filters failed\n");
        return -1;
    }

    handle->config_status = MCT_OFFLINE_LOGSTORAGE_CONFIG_DONE;

    return 0;
}

/**
 * mct_logstorage_device_connected
 *
 * Initializes MCT Offline Logstorage with respect to device status
 *
 * @param handle         MCT Logstorage handle
 * @param mount_point    Device mount path
 * @return               0 on success, -1 on error, 1 on warning
 */
int mct_logstorage_device_connected(MctLogStorage *handle, const char *mount_point)
{
    if ((handle == NULL) || (mount_point == NULL)) {
        mct_log(LOG_ERR, "Handle error \n");
        return -1;
    }

    if (handle->connection_type == MCT_OFFLINE_LOGSTORAGE_DEVICE_CONNECTED) {
        mct_log(LOG_WARNING,
                "Device already connected. Send disconnect, connect request\n");

        mct_logstorage_device_disconnected(
            handle,
            MCT_LOGSTORAGE_SYNC_ON_DEVICE_DISCONNECT);
    }

    strncpy(handle->device_mount_point, mount_point, MCT_MOUNT_PATH_MAX);
    handle->device_mount_point[MCT_MOUNT_PATH_MAX] = 0;
    handle->connection_type = MCT_OFFLINE_LOGSTORAGE_DEVICE_CONNECTED;
    handle->config_status = 0;
    handle->write_errors = 0;
    handle->num_configs = 0;
    handle->newest_file_list = NULL;

    switch (handle->config_mode) {
      case MCT_LOGSTORAGE_CONFIG_FILE:
        /* Setup logstorage with config file settings */
        return mct_logstorage_load_config(handle);
      default:
        return -1;
    }
}

/**
 * mct_logstorage_device_disconnected
 *
 * De-Initializes MCT Offline Logstorage with respect to device status
 *
 * @param handle         MCT Logstorage handle
 * @param reason         Reason for disconnect
 * @return               0 on success, -1 on error
 *
 */
int mct_logstorage_device_disconnected(MctLogStorage *handle, int reason)
{
    MctNewestFileName *tmp = NULL;
    if (handle == NULL)
        return -1;

    /* If configuration loading was done, free it */
    if (handle->config_status == MCT_OFFLINE_LOGSTORAGE_CONFIG_DONE)
        mct_logstorage_free(handle, reason);

    /* Reset all device status */
    memset(handle->device_mount_point, 0, sizeof(char) * (MCT_MOUNT_PATH_MAX + 1));
    handle->connection_type = MCT_OFFLINE_LOGSTORAGE_DEVICE_DISCONNECTED;
    handle->config_status = 0;
    handle->write_errors = 0;
    handle->num_configs = 0;

    while (handle->newest_file_list) {
        tmp = handle->newest_file_list;
        handle->newest_file_list = tmp->next;
        if (tmp->file_name) {
            free(tmp->file_name);
            tmp->file_name = NULL;
        }
        if (tmp->newest_file) {
            free(tmp->newest_file);
            tmp->newest_file = NULL;
        }
        free(tmp);
        tmp = NULL;
    }

    return 0;
}

/**
 * mct_logstorage_get_loglevel_by_key
 *
 * Obtain the log level for the provided key
 * This function can be used to obtain log level when the actual
 * key stored in the Hash map is availble with the caller
 *
 * @param handle    MctLogstorage handle
 * @param key       key to search for in Hash MAP
 * @return          log level on success:, -1 on error
 */
int mct_logstorage_get_loglevel_by_key(MctLogStorage *handle, char *key)
{
    MctLogStorageFilterConfig *config[MCT_CONFIG_FILE_SECTIONS_MAX];
    int num_configs = 0;
    int i = 0;
    int log_level = 0;

    /* Check if handle is NULL,already initialized or already configured  */
    if ((handle == NULL) ||
        (key == NULL) ||
        (handle->connection_type != MCT_OFFLINE_LOGSTORAGE_DEVICE_CONNECTED) ||
        (handle->config_status != MCT_OFFLINE_LOGSTORAGE_CONFIG_DONE))
        return -1;

    num_configs = mct_logstorage_list_find(key, &(handle->config_list), config);

    if (num_configs == 0)
    {
        mct_vlog(LOG_WARNING, "Configuration for key [%s] not found!\n", key);
        return -1;
    }
    else if (num_configs == 1)
    {
        if (config[0] != NULL)
        {
            log_level = config[0]->log_level;
        }
    }
    else
    {
        /**
         * Multiple configurations found, raise a warning to the user and go
         * for the more verbose one.
         */
        mct_vlog(LOG_WARNING, "Multiple configuration for key [%s] found,"
                 " return the highest log level!\n", key);

        for (i = 0; i < num_configs; i++)
        {
            if ((config[i] != NULL) && (config[i]->log_level > log_level))
            {
                log_level = config[i]->log_level;
            }
        }
    }

    return log_level;
}

/**
 * mct_logstorage_get_config
 *
 * Obtain the configuration data of all filters for provided apid and ctid
 *
 * @param handle    MctLogStorage handle
 * @param config    [out] Pointer to array of filter configurations
 * @param apid      application id
 * @param ctid      context id
 * @param ecuid     ecu id
 * @return          number of configurations found
 */
int mct_logstorage_get_config(MctLogStorage *handle,
                              MctLogStorageFilterConfig **config,
                              char *apid,
                              char *ctid,
                              char *ecuid)
{
    MctLogStorageFilterConfig **cur_config_ptr = NULL;
    char key[MCT_CONFIG_FILE_SECTIONS_MAX][MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN] =
    { { '\0' }, { '\0' }, { '\0' } };
    int i = 0;
    int apid_len = 0;
    int ctid_len = 0;
    int ecuid_len = 0;
    int num_configs = 0;
    int num = 0;

    /* Check if handle is NULL,already initialized or already configured  */
    if ((handle == NULL) || (config == NULL) ||
        (handle->connection_type != MCT_OFFLINE_LOGSTORAGE_DEVICE_CONNECTED) ||
        (handle->config_status != MCT_OFFLINE_LOGSTORAGE_CONFIG_DONE) ||
        (ecuid == NULL))
        return 0;

    /* Prepare possible keys with
     * Possible combinations are
     * ecu::
     * ecu:apid:ctid
     * :apid:ctid
     * ecu::ctid
     * ecu:apid:
     * ::ctid
     * :apid: */

    ecuid_len = strlen(ecuid);

    if (ecuid_len > MCT_ID_SIZE)
        ecuid_len = MCT_ID_SIZE;

    if ((apid == NULL) && (ctid == NULL)) {
        /* ecu:: */
        strncpy(key[0], ecuid, ecuid_len);
        strncat(key[0], ":", 1);
        strncat(key[0], ":", 1);

        num_configs = mct_logstorage_list_find(key[0], &(handle->config_list),
                                               config);
        return num_configs;
    }

    apid_len = strlen(apid);

    if (apid_len > MCT_ID_SIZE)
        apid_len = MCT_ID_SIZE;

    ctid_len = strlen(ctid);

    if (ctid_len > MCT_ID_SIZE)
        ctid_len = MCT_ID_SIZE;

    /* :apid: */
    strncpy(key[0], ":", 1);
    strncat(key[0], apid, apid_len);
    strncat(key[0], ":", 1);

    /* ::ctid */
    strncpy(key[1], ":", 1);
    strncat(key[1], ":", 1);
    strncat(key[1], ctid, ctid_len);

    /* :apid:ctid */
    strncpy(key[2], ":", 1);
    strncat(key[2], apid, apid_len);
    strncat(key[2], ":", 1);
    strncat(key[2], ctid, ctid_len);

    /* ecu:apid:ctid */
    strncpy(key[3], ecuid, ecuid_len);
    strncat(key[3], ":", 1);
    strncat(key[3], apid, apid_len);
    strncat(key[3], ":", 1);
    strncat(key[3], ctid, ctid_len);

    /* ecu:apid: */
    strncpy(key[4], ecuid, ecuid_len);
    strncat(key[4], ":", 1);
    strncat(key[4], apid, apid_len);
    strncat(key[4], ":", 1);

    /* ecu::ctid */
    strncpy(key[5], ecuid, ecuid_len);
    strncat(key[5], ":", 1);
    strncat(key[5], ":", 1);
    strncat(key[5], ctid, ctid_len);

    /* ecu:: */
    strncpy(key[6], ecuid, ecuid_len);
    strncat(key[6], ":", 1);
    strncat(key[6], ":", 1);

    /* Search the list three times with keys as -apid: , :ctid and apid:ctid */
    for (i = 0; i < MCT_OFFLINE_LOGSTORAGE_MAX_POSSIBLE_KEYS; i++)
    {
        cur_config_ptr = &config[num_configs];
        num = mct_logstorage_list_find(key[i], &(handle->config_list),
                                       cur_config_ptr);
        num_configs += num;
        /* If all filter configurations matched, stop and return */
        if (num_configs == handle->num_configs)
        {
            break;
        }
    }

    return num_configs;
}

/**
 * mct_logstorage_filter
 *
 * Check if log message need to be stored in a certain device based on filter
 * config
 * - get all MctLogStorageFilterConfig from hash table possible by given
 *   apid/ctid (apid:, :ctid, apid:ctid
 * - for each found structure, compare message log level with configured one
 *
 * @param handle    MctLogStorage handle
 * @param config    Pointer to array of filter configurations
 * @param apid      application id
 * @param ctid      context id
 * @param log_level Log level of message
 * @param ecuid     EcuID given in the message
 * @return          number of found configurations
 */
static int mct_logstorage_filter(MctLogStorage *handle,
                                     MctLogStorageFilterConfig **config,
                                     char *apid,
                                     char *ctid,
                                     char *ecuid,
                                     int log_level)
{
    int i = 0;
    int num = 0;

    if ((handle == NULL) || (config == NULL) || (ecuid == NULL))
        return -1;

    /* filter on names: find MctLogStorageFilterConfig structures */
    num = mct_logstorage_get_config(handle, config, apid, ctid, ecuid);

    if (num == 0) {
        mct_vlog(LOG_DEBUG,
                 "%s: No valid filter configuration found for apid=[%.4s] ctid=[%.4s] ecuid=[%.4s]\n",
                 __func__, apid, ctid, ecuid);
        return 0;
    }

    for (i = 0 ; i < num ; i++)
    {
        if (config[i] == NULL)
        {
            mct_vlog(LOG_DEBUG,
                     "%s: config[%d] is NULL, continue the filter loop\n",
                     __func__, i);
            continue;
        }

        /* filter on log level */
        if (log_level > config[i]->log_level) {
            mct_vlog(LOG_DEBUG,
                     "%s: Requested log level (%d) is higher than config[%d]->log_level (%d). Set the config to NULL and continue the filter loop\n",
                     __func__, log_level, i, config[i]->log_level);
            config[i] = NULL;
            continue;
        }

        /* filter on ECU id only if EcuID is set */
        if (config[i]->ecuid != NULL) {
            if (strncmp(ecuid, config[i]->ecuid, MCT_ID_SIZE) != 0)
            {
                mct_vlog(LOG_DEBUG,
                         "%s: ECUID does not match (Requested=%s, config[%d]=%s). Set the config to NULL and continue the filter loop\n",
                         __func__, ecuid, i, config[i]->ecuid);
                config[i] = NULL;
                continue;
            }
        }

        if(config[i]->excluded_apids != NULL && config[i]->excluded_ctids != NULL) {
            /* Filter on excluded application and context */
            if(apid != NULL && ctid != NULL && mct_logstorage_check_excluded_ids(apid, ",", config[i]->excluded_apids)
              && mct_logstorage_check_excluded_ids(ctid, ",", config[i]->excluded_ctids)) {
                mct_vlog(LOG_DEBUG, "%s: %s matches with [%s] and %s matches with [%s]. Set the config to NULL and continue the filter loop\n",
                __func__, apid, config[i]->excluded_apids, ctid, config[i]->excluded_ctids);
                config[i] = NULL;
            }
        }
        else if(config[i]->excluded_apids == NULL) {
            /* Only filter on excluded contexts */
            if(ctid != NULL && config[i]->excluded_ctids != NULL && mct_logstorage_check_excluded_ids(ctid, ",", config[i]->excluded_ctids)) {
                mct_vlog(LOG_DEBUG, "%s: %s matches with [%s]. Set the config to NULL and continue the filter loop\n",
                __func__, ctid, config[i]->excluded_ctids);
                config[i] = NULL;
            }
        }
        else if(config[i]->excluded_ctids == NULL) {
            /* Only filter on excluded applications */
            if(apid != NULL && config[i]->excluded_apids != NULL && mct_logstorage_check_excluded_ids(apid, ",", config[i]->excluded_apids)) {
                mct_vlog(LOG_DEBUG, "%s: %s matches with [%s]. Set the config to NULL and continue the filter loop\n",
                __func__, apid, config[i]->excluded_apids);
                config[i] = NULL;
            }
        }
    }

    return num;
}

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
                         int *disable_nw)
{
    MctLogStorageFilterConfig *config[MCT_CONFIG_FILE_SECTIONS_MAX];

    int i = 0;
    int ret = 0;
    int num = 0;
    int err = 0;
    /* data2 contains MctStandardHeader, MctStandardHeaderExtra and
     * MctExtendedHeader. We are interested in ecuid, apid, ctid and loglevel */
    MctExtendedHeader *extendedHeader = NULL;
    MctStandardHeaderExtra *extraHeader = NULL;
    MctStandardHeader *standardHeader = NULL;
    unsigned int standardHeaderExtraLen = sizeof(MctStandardHeaderExtra);
    unsigned int header_len = 0;
    MctNewestFileName *tmp = NULL;
    int found = 0;

    int log_level = -1;

    if ((handle == NULL) || (uconfig == NULL) ||
        (data1 == NULL) || (data2 == NULL) || (data3 == NULL) ||
        (handle->connection_type != MCT_OFFLINE_LOGSTORAGE_DEVICE_CONNECTED) ||
        (handle->config_status != MCT_OFFLINE_LOGSTORAGE_CONFIG_DONE))
        return 0;

    /* Calculate real length of MctStandardHeaderExtra */
    standardHeader = (MctStandardHeader *)data2;

    if (!MCT_IS_HTYP_WEID(standardHeader->htyp))
        standardHeaderExtraLen -= MCT_ID_SIZE;

    if (!MCT_IS_HTYP_WSID(standardHeader->htyp))
        standardHeaderExtraLen -= MCT_SIZE_WSID;

    if (!MCT_IS_HTYP_WTMS(standardHeader->htyp))
        standardHeaderExtraLen -= MCT_SIZE_WTMS;

    extraHeader = (MctStandardHeaderExtra *)(data2
                                             + sizeof(MctStandardHeader));

    if (MCT_IS_HTYP_UEH(standardHeader->htyp)) {
        header_len = sizeof(MctStandardHeader) + sizeof(MctExtendedHeader) + standardHeaderExtraLen;

        /* check if size2 is big enough to contain expected MCT message header */
        if ((unsigned int)size2 < header_len) {
            mct_vlog(LOG_ERR, "%s: MCT message header is too small\n", __func__);
            return 0;
        }

        extendedHeader = (MctExtendedHeader *)(data2
                                               + sizeof(MctStandardHeader) + standardHeaderExtraLen);

        log_level = MCT_GET_MSIN_MTIN(extendedHeader->msin);

        /* check if log message need to be stored in a certain device based on
         * filter configuration */
        num = mct_logstorage_filter(handle, config, extendedHeader->apid,
                                    extendedHeader->ctid, extraHeader->ecu, log_level);

        if ((num == 0) || (num == -1)) {
            mct_vlog(LOG_DEBUG,
                     "%s: No valid filter configuration found for apid=[%.4s] ctid=[%.4s] ecuid=[%.4s]!\n",
                     __func__, extendedHeader->apid, extendedHeader->ctid, extraHeader->ecu);
            return 0;
        }
    }
    else {
        header_len = sizeof(MctStandardHeader) + standardHeaderExtraLen;

        /* check if size2 is big enough to contain expected MCT message header */
        if ((unsigned int)size2 < header_len) {
            mct_log(LOG_ERR, "MCT message header is too small (without extended header)\n");
            return 0;
        }

        log_level = MCT_LOG_VERBOSE;

        /* check if log message need to be stored in a certain device based on
         * filter configuration */
        num = mct_logstorage_filter(handle, config, NULL,
                                    NULL, extraHeader->ecu, log_level);

        if ((num == 0) || (num == -1)) {
            mct_log(LOG_DEBUG, "No valid filter configuration found!\n");
            return 0;
        }
    }

    /* store log message in every found filter */
    for (i = 0; i < num; i++)
    {
        if (config[i] == NULL)
        {
            mct_vlog(LOG_DEBUG,
                     "%s: config[%d] is NULL. Continue the filter loop\n",
                     __func__, i);
            continue;
        }

        /* If file name is not present, the filter is non verbose control filter
         * hence skip storing */
        if (config[i]->file_name == NULL)
        {
            mct_vlog(LOG_DEBUG,
                     "%s: config[%d]->file_name is NULL, which equals to non verbose control filter. Continue the filter loop\n",
                     __func__, i);
            continue;
        }

        /* Disable network routing */
        if ((config[i]->disable_network_routing & MCT_LOGSTORAGE_DISABLE_NW_ON) > 0) {
            *disable_nw = 1;
            if (config[i]->ecuid == NULL)
                mct_vlog(LOG_DEBUG, "%s: Disable routing to network for ApId-CtId-EcuId [%s]-[%s]-[]\n", __func__,
                         config[i]->apids, config[i]->ctids);
            else
                mct_vlog(LOG_DEBUG, "%s: Disable routing to network for ApId-CtId-EcuId [%s]-[%s]-[%s]\n", __func__,
                         config[i]->apids, config[i]->ctids, config[i]->ecuid);
        }

        if (config[i]->skip == 1)
        {
            mct_vlog(LOG_DEBUG,
                     "%s: config[%d] (filename=%s) is skipped. Continue the filter loop\n",
                     __func__, i, config[i]->file_name);
            continue;
        }

        tmp = handle->newest_file_list;
        while (tmp) {
            if (strcmp(tmp->file_name, config[i]->file_name) == 0) {
                found = 1;
                break;
            }
            else {
                tmp = tmp->next;
            }
        }
        if (!found) {
            mct_vlog(LOG_ERR, "Cannot find out record for filename [%s]\n",
                    config[i]->file_name);
            return -1;
        }

        /* prepare log file (create and/or open)*/
        if (config[i]->ecuid == NULL)
            mct_vlog(LOG_DEBUG, "%s: ApId-CtId-EcuId [%s]-[%s]-[]\n", __func__,
                     config[i]->apids, config[i]->ctids);
        else
            mct_vlog(LOG_DEBUG, "%s: ApId-CtId-EcuId [%s]-[%s]-[%s]\n", __func__,
                     config[i]->apids, config[i]->ctids, config[i]->ecuid);

        ret = config[i]->mct_logstorage_prepare(config[i],
                                                uconfig,
                                                handle->device_mount_point,
                                                size1 + size2 + size3,
                                                tmp);

        if (ret == 0 && config[i]->skip == 1) {
            continue;
        }

        if ((ret == 0) &&
            (config[i]->sync == MCT_LOGSTORAGE_SYNC_UNSET ||
             config[i]->sync == MCT_LOGSTORAGE_SYNC_ON_MSG)) {
            /* It is abnormal if working file is still NULL after preparation. */
            if (!config[i]->working_file_name) {
                mct_vlog(LOG_ERR, "Failed to prepare working file for %s\n",
                        config[i]->file_name);
                return -1;
            }
            else {
                /* After preparation phase, update newest file info
                 * it means there is new file created, newest file info must be updated.
                 */
                if (tmp->newest_file) {
                    free(tmp->newest_file);
                    tmp->newest_file = NULL;
                }
                tmp->newest_file = strdup(config[i]->working_file_name);
                tmp->wrap_id = config[i]->wrap_id;
            }
        }

        if (ret == 0) { /* log data (write) */
            ret = config[i]->mct_logstorage_write(config[i],
                                                  uconfig,
                                                  handle->device_mount_point,
                                                  data1,
                                                  size1,
                                                  data2,
                                                  size2,
                                                  data3,
                                                  size3);

            if (ret == 0) {
                /* In case of behavior CACHED_BASED, the newest file info
                 * must be updated right after writing phase.
                 * That is because in writing phase, it could also perform
                 * sync to file which actions could impact to the log file info.
                 * If both working file name and newest file name are unavailable,
                 * it means the sync to file is not performed yet, wait for next times.
                 */
                if (config[i]->sync != MCT_LOGSTORAGE_SYNC_ON_MSG &&
                        config[i]->sync != MCT_LOGSTORAGE_SYNC_UNSET) {
                    if (config[i]->working_file_name) {
                        if (tmp->newest_file) {
                            free(tmp->newest_file);
                            tmp->newest_file = NULL;
                        }
                        tmp->newest_file = strdup(config[i]->working_file_name);
                        tmp->wrap_id = config[i]->wrap_id;
                    }
                }

                /* flush to be sure log is stored on device */
                ret = config[i]->mct_logstorage_sync(config[i],
                                                     uconfig,
                                                     handle->device_mount_point,
                                                     MCT_LOGSTORAGE_SYNC_ON_MSG);

                if (ret != 0)
                    mct_log(LOG_ERR,
                            "mct_logstorage_write: Unable to sync.\n");
            }
            else {
                handle->write_errors += 1;

                if (handle->write_errors >=
                    MCT_OFFLINE_LOGSTORAGE_MAX_ERRORS)
                    err = -1;

                mct_log(LOG_ERR,
                        "mct_logstorage_write: Unable to write.\n");
            }
        }
        else {
            handle->prepare_errors += 1;

            if (handle->prepare_errors >=
                MCT_OFFLINE_LOGSTORAGE_MAX_ERRORS) {
                config[i]->skip = 1;
                mct_vlog(LOG_WARNING,
                         "%s: Unable to prepare. Skip filename [%s] because maxmimum trial has been reached.\n",
                         __func__, config[i]->file_name);
            } else {
                mct_vlog(LOG_ERR,
                         "%s: Unable to prepare.\n", __func__);
            }
        }
    }

    return err;
}

/**
 * mct_logstorage_sync_caches
 *
 * Write Cache data to file
 *
 * @param handle     MctLogStorage handle
 * @return           0 on success, -1 on error
 */
int mct_logstorage_sync_caches(MctLogStorage *handle)
{
    MctLogStorageFilterList **tmp = NULL;

    if (handle == NULL)
        return -1;

    tmp = &(handle->config_list);

    while (*(tmp) != NULL) {
        if ((*tmp)->data != NULL) {
            if ((*tmp)->data->mct_logstorage_sync((*tmp)->data,
                                                  &handle->uconfig,
                                                  handle->device_mount_point,
                                                  MCT_LOGSTORAGE_SYNC_ON_DEMAND) != 0)
                mct_vlog(LOG_ERR,
                         "%s: Sync failed. Continue with next cache.\n",
                         __func__);
        }

        tmp = &(*tmp)->next;

    }

    return 0;
}
