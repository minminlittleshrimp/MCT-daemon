#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "mct-daemon.h"
#include "mct_daemon_offline_logstorage.h"
#include "mct_daemon_offline_logstorage_internal.h"

/**
 * mct_logstorage_split_ecuid
 *
 * Split keys with ECU ID alone
 *
 * @param key            Key
 * @param len            Key length
 * @param ecuid          ECU ID from key stored here
 * @param apid           Application ID as .* stored here
 * @param ctid           Context id as .* stored here
 * @return               0 on success -1 on error
 */
static MctReturnValue mct_logstorage_split_ecuid(char *key,
                                                     int len,
                                                     char *ecuid,
                                                     char *apid,
                                                     char *ctid)
{
    if ((len > (MCT_ID_SIZE + 2)) || (len < 2)) {
        return MCT_RETURN_ERROR;
    }

    memcpy(ecuid, key, (len - 2));
    memcpy(apid, ".*", 2);
    memcpy(ctid, ".*", 2);

    return MCT_RETURN_OK;
}

unsigned int g_logstorage_cache_max;
/**
 * mct_logstorage_split_ctid
 *
 * Split keys with Context ID alone
 *
 * @param key            Key
 * @param len            Key length
 * @param apid           Application ID as .* stored here
 * @param ctid           Context id from key stored here
 * @return               0 on success -1 on error
 */
static MctReturnValue mct_logstorage_split_ctid(char *key,
                                                    int len,
                                                    char *apid,
                                                    char *ctid)
{
    if ((len > (MCT_ID_SIZE + 2)) || (len < 1)) {
        return MCT_RETURN_ERROR;
    }

    strncpy(ctid, (key + 2), (len - 1));
    memcpy(apid, ".*", 2);

    return MCT_RETURN_OK;
}

/**
 * mct_logstorage_split_apid
 *
 * Split keys with Application ID alone
 *
 * @param key            Key
 * @param len            Key length
 * @param apid           Application ID from key is stored here
 * @param ctid           Context id as .* stored here
 * @return               0 on success -1 on error
 */
static MctReturnValue mct_logstorage_split_apid(char *key,
                                                    int len,
                                                    char *apid,
                                                    char *ctid)
{
    if ((len > (MCT_ID_SIZE + 2)) || (len < 2)) {
        return MCT_RETURN_ERROR;
    }

    strncpy(apid, key + 1, (len - 2));
    memcpy(ctid, ".*", 2);

    return MCT_RETURN_OK;
}

/**
 * mct_logstorage_split_apid_ctid
 *
 * Split keys with Application ID and Context ID
 *
 * @param key            Key
 * @param len            Key length
 * @param apid           Application ID from key is stored here
 * @param ctid           CContext id from key is stored here
 * @return               0 on success -1 on error
 */
static MctReturnValue mct_logstorage_split_apid_ctid(char *key,
                                                         int len,
                                                         char *apid,
                                                         char *ctid)
{
    char *tok = NULL;

    if (len > MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN) {
        return MCT_RETURN_ERROR;
    }

    /* copy apid and ctid */
    tok = strtok(key, ":");

    if (tok != NULL) {
        strncpy(apid, tok, MCT_ID_SIZE);
    } else {
        return MCT_RETURN_ERROR;
    }

    tok = strtok(NULL, ":");

    if (tok != NULL) {
        strncpy(ctid, tok, MCT_ID_SIZE);
    } else {
        return MCT_RETURN_ERROR;
    }

    return MCT_RETURN_OK;
}

/**
 * mct_logstorage_split_ecuid_apid
 *
 * Split keys with ECU ID and Application ID
 *
 * @param key            Key
 * @param len            Key length
 * @param ecuid          ECU ID from key stored here
 * @param apid           Application ID from key is stored here
 * @param ctid           Context id as .* stored here
 * @return               0 on success -1 on error
 */
static MctReturnValue mct_logstorage_split_ecuid_apid(char *key,
                                                          int len,
                                                          char *ecuid,
                                                          char *apid,
                                                          char *ctid)
{
    char *tok = NULL;

    if (len > MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN) {
        return MCT_RETURN_ERROR;
    }

    /* copy apid and ctid */
    tok = strtok(key, ":");

    if (tok != NULL) {
        strncpy(ecuid, tok, MCT_ID_SIZE);
    } else {
        return MCT_RETURN_ERROR;
    }

    tok = strtok(NULL, ":");

    if (tok != NULL) {
        strncpy(apid, tok, MCT_ID_SIZE);
    } else {
        return MCT_RETURN_ERROR;
    }

    memcpy(ctid, ".*", 2);

    return MCT_RETURN_OK;
}

/**
 * mct_logstorage_split_multi
 *
 * Prepares keys with application ID alone, will use ecuid if provided
 * (ecuid\:apid\:\:) or (\:apid\:\:)
 *
 * @param key            Prepared key stored here
 * @param len            Key length
 * @param ecuid          ECU ID
 * @param apid           Application ID
 * @param ctid           Context ID
 * @return               None
 */
static MctReturnValue mct_logstorage_split_multi(char *key,
                                                     int len,
                                                     char *ecuid,
                                                     char *apid,
                                                     char *ctid)
{
    char *tok = NULL;

    if (len > MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN) {
        return MCT_RETURN_ERROR;
    }

    tok = strtok(key, ":");

    if (tok == NULL) {
        return MCT_RETURN_ERROR;
    }

    len = strlen(tok);

    if (key[len + 1] == ':') {
        strncpy(ecuid, tok, MCT_ID_SIZE);

        tok = strtok(NULL, ":");

        if (tok != NULL) {
            strncpy(ctid, tok, MCT_ID_SIZE);
        }

        memcpy(apid, ".*", 2);
    } else {
        strncpy(ecuid, tok, MCT_ID_SIZE);
        tok = strtok(NULL, ":");
        strncpy(apid, tok, MCT_ID_SIZE);
        tok = strtok(NULL, ":");
        strncpy(ctid, tok, MCT_ID_SIZE);
    }

    return MCT_RETURN_OK;
}

/**
 * mct_logstorage_split_key
 *
 * Split a given key into apid and ctid.
 * If APID\: - apid = APID and ctid = .*
 * If \:CTID - ctid = CTID and apid = .*
 * Else apid = APID and ctid = CTID
 *
 * @param key      Given key of filter hash map
 * @param apid     Application id
 * @param ctid     Context id
 * @param ecuid    ECU id
 * @return         0 on success, -1 on error
 */
static MctReturnValue mct_logstorage_split_key(char *key, char *apid,
                                                   char *ctid, char *ecuid)
{
    int len = 0;
    char *sep = NULL;

    if ((key == NULL) || (apid == NULL) || (ctid == NULL) || (ecuid == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    len = strlen(key);

    sep = strchr (key, ':');

    if (sep == NULL) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* key is ecuid only ecuid::*/
    if ((key[len - 1] == ':') && (key[len - 2] == ':')) {
        return mct_logstorage_split_ecuid(key, len, ecuid, apid, ctid);
    }
    /* key is context id only  ::apid*/
    else if ((key[0] == ':') && (key[1] == ':')) {
        return mct_logstorage_split_ctid(key, len, apid, ctid);
    }
    /* key is application id only :apid: */
    else if ((key[0] == ':') && (key[len - 1] == ':')) {
        return mct_logstorage_split_apid(key, len, apid, ctid);
    }
    /* key is :apid:ctid */
    else if ((key[0] == ':') && (key[len - 1] != ':')) {
        return mct_logstorage_split_apid_ctid(key, len, apid, ctid);
    }
    /* key is ecuid:apid: */
    else if ((key[0] != ':') && (key[len - 1] == ':')) {
        return mct_logstorage_split_ecuid_apid(key, len, ecuid, apid, ctid);
    }
    /* key is either ecuid::ctid or ecuid:apid:ctid */
    else {
        return mct_logstorage_split_multi(key, len, ecuid, apid, ctid);
    }
}

/**
 * Forward SET_LOG_LEVEL request to passive node
 *
 * @param daemon_local  pointer to MctDaemonLocal structure
 * @param apid          Application ID
 * @param ctid          Context ID
 * @param ecuid         ECU ID
 * @param loglevel      requested log level
 * @param verbose       verbosity flag
 */
static MctReturnValue mct_daemon_logstorage_update_passive_node_context(
    MctDaemonLocal *daemon_local,
    char *apid,
    char *ctid,
    char *ecuid,
    int loglevel,
    int verbose)
{
    MctServiceSetLogLevel req = {0};

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon_local == NULL) || (apid == NULL) || (ctid == NULL) || (ecuid == NULL) ||
        (loglevel > MCT_LOG_VERBOSE) || (loglevel < MCT_LOG_DEFAULT)) {
        mct_vlog(LOG_ERR, "%s: Wrong parameter\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    mct_set_id(req.apid, apid);
    mct_set_id(req.ctid, ctid);

    req.log_level = loglevel;
    return MCT_RETURN_OK;
}

/**
 * mct_daemon_logstorage_send_log_level
 *
 * Send new log level for the provided context, if ecuid is not daemon ecuid
 * update log level of passive node
 *
 * @param daemon            MctDaemon structure
 * @param daemon_local      MctDaemonLocal structure
 * @param context           MctDaemonContext structure
 * @param ecuid             ECU id
 * @param loglevel          log level to be set to context
 * @param verbose           If set to true verbose information is printed out
 * @return                  0 on success, -1 on error
 */
static MctReturnValue mct_daemon_logstorage_send_log_level(MctDaemon *daemon,
                                                               MctDaemonLocal *daemon_local,
                                                               MctDaemonContext *context,
                                                               char *ecuid,
                                                               int loglevel,
                                                               int verbose)
{
    int old_log_level = -1;
    int ll = MCT_LOG_DEFAULT;

    if ((daemon == NULL) || (daemon_local == NULL) || (ecuid == NULL) ||
        (context == NULL) || (loglevel > MCT_LOG_VERBOSE) || (loglevel < MCT_LOG_DEFAULT)) {
        mct_vlog(LOG_ERR, "%s: Wrong parameter\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (strncmp(ecuid, daemon->ecuid, MCT_ID_SIZE) == 0) {
        old_log_level = context->storage_log_level;

        context->storage_log_level = MCT_OFFLINE_LOGSTORAGE_MAX(loglevel,
                                                                context->storage_log_level);

        if (context->storage_log_level > old_log_level) {
            if (mct_daemon_user_send_log_level(daemon, context, verbose) == -1) {
                mct_log(LOG_ERR, "Unable to update log level\n");
                return MCT_RETURN_ERROR;
            }
        }
    } else {

        old_log_level = context->log_level;

        ll = MCT_OFFLINE_LOGSTORAGE_MAX(loglevel, context->log_level);

        if (ll > old_log_level) {
            return mct_daemon_logstorage_update_passive_node_context(daemon_local,
                                                                     context->apid,
                                                                     context->ctid,
                                                                     ecuid,
                                                                     ll,
                                                                     verbose);
        }
    }

    return MCT_RETURN_OK;
}

/**
 * mct_daemon_logstorage_reset_log_level
 *
 * The log levels are reset if log level provided is -1 (not sent to
 * application in this case). Reset and sent to application if current log level
 * provided is 0.
 *
 * @param daemon            MctDaemon structure
 * @param daemon_local      MctDaemonLocal structure
 * @param context           MctDaemonContext structure
 * @param ecuid             ECU ID
 * @param loglevel          log level to be set to context
 * @param verbose           If set to true verbose information is printed out
 * @return                  0 on success, -1 on error
 */
static MctReturnValue mct_daemon_logstorage_reset_log_level(MctDaemon *daemon,
                                                                MctDaemonLocal *daemon_local,
                                                                MctDaemonContext *context,
                                                                char *ecuid,
                                                                int loglevel,
                                                                int verbose)
{
    if ((daemon == NULL) || (daemon_local == NULL) || (ecuid == NULL) ||
        (context == NULL) || (loglevel > MCT_LOG_VERBOSE) || (loglevel < MCT_LOG_DEFAULT)) {
        mct_vlog(LOG_ERR, "%s: Wrong parameter\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* Set storage level to -1, to clear log levels */
    context->storage_log_level = MCT_LOG_DEFAULT;

    if (loglevel == MCT_DAEMON_LOGSTORAGE_RESET_SEND_LOGLEVEL) {
        if (strncmp(ecuid, daemon->ecuid, MCT_ID_SIZE) == 0) {
            if (mct_daemon_user_send_log_level(daemon,
                                               context,
                                               verbose) == MCT_RETURN_ERROR) {
                mct_log(LOG_ERR, "Unable to update log level\n");
                return MCT_RETURN_ERROR;
            }
        } else { /* forward set log level to passive node */
            return mct_daemon_logstorage_update_passive_node_context(daemon_local,
                                                                     context->apid,
                                                                     context->ctid,
                                                                     ecuid,
                                                                     MCT_LOG_DEFAULT,
                                                                     verbose);
        }
    }

    return MCT_RETURN_OK;
}

/**
 * mct_daemon_logstorage_force_reset_level
 *
 * Force resetting of log level since have no data provided by passive node.
 *
 * @param daemon            MctDaemon structure
 * @param daemon_local      MctDaemonLocal structure
 * @param apid              Application ID
 * @param ctid              Context ID
 * @param ecuid             ECU ID
 * @param loglevel          log level to be set to context
 * @param verbose           If set to true verbose information is printed out
 * @return                  0 on success, -1 on error
 */
static MctReturnValue mct_daemon_logstorage_force_reset_level(MctDaemon *daemon,
                                                                  MctDaemonLocal *daemon_local,
                                                                  char *apid,
                                                                  char *ctid,
                                                                  char *ecuid,
                                                                  int loglevel,
                                                                  int verbose)
{
    int ll = MCT_LOG_DEFAULT;
    int num = 0;
    int i = 0;
    MctLogStorageFilterConfig *config[MCT_CONFIG_FILE_SECTIONS_MAX] = {0};

    if ((daemon == NULL) || (daemon_local == NULL) || (ecuid == NULL) ||
        (apid == NULL) || (ctid == NULL) || (loglevel > MCT_LOG_VERBOSE) ||
        (loglevel < MCT_LOG_DEFAULT)) {
        mct_vlog(LOG_ERR, "%s: Wrong parameter\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    for (i = 0; i < daemon_local->flags.offlineLogstorageMaxDevices; i++) {
        num = mct_logstorage_get_config(&(daemon->storage_handle[i]), config, apid, ctid, ecuid);

        if (num > 0) {
            break; /* found config */
        }
    }

    if ((num == 0) || (config[0] == NULL)) {
        mct_vlog(
            LOG_ERR,
            "%s: No information about APID: %s, CTID: %s, ECU: %s in Logstorage configuration\n",
            __func__,
            apid,
            ctid,
            ecuid);
        return MCT_RETURN_ERROR;
    }

    if (loglevel == MCT_DAEMON_LOGSTORAGE_RESET_SEND_LOGLEVEL) {
        ll = config[0]->reset_log_level;
    } else {
        ll = config[0]->log_level;
    }

    return mct_daemon_logstorage_update_passive_node_context(daemon_local, apid,
                                                             ctid, ecuid, ll, verbose);
}

/**
 * mct_logstorage_update_all_contexts
 *
 * Update log level of all contexts of the application by updating the daemon
 * internal table. The compare flags (cmp_flag) indicates if Id has to be
 * compared with application id or Context id of the daemon internal table.
 * The log levels are reset if current log level provided is -1 (not sent to
 * application in this case). Reset and sent to application if current log level
 * provided is 0.
 *
 * @param daemon            MctDaemon structure
 * @param daemon_local      MctDaemonLocal structure
 * @param id                application id or context id
 * @param curr_log_level    log level to be set to context
 * @param cmp_flag          compare flag
 * @param ecuid             ecu id where application runs
 * @param verbose           If set to true verbose information is printed out
 * @return                  0 on success, -1 on error
 */
MctReturnValue mct_logstorage_update_all_contexts(MctDaemon *daemon,
                                                  MctDaemonLocal *daemon_local,
                                                  char *id,
                                                  int curr_log_level,
                                                  int cmp_flag,
                                                  char *ecuid,
                                                  int verbose)
{
    MctDaemonRegisteredUsers *user_list = NULL;
    int i = 0;
    char tmp_id[MCT_ID_SIZE + 1] = {'\0'};

    if ((daemon == NULL) || (daemon_local == NULL) || (id == NULL) ||
        (ecuid == NULL) || (cmp_flag <= MCT_DAEMON_LOGSTORAGE_CMP_MIN) ||
        (cmp_flag >= MCT_DAEMON_LOGSTORAGE_CMP_MAX)) {
        mct_vlog(LOG_ERR, "Wrong parameter in function %s\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    user_list = mct_daemon_find_users_list(daemon, ecuid, verbose);

    if (user_list == NULL) {
        return MCT_RETURN_ERROR;
    }

    for (i = 0; i < user_list->num_contexts; i++) {
        if (cmp_flag == MCT_DAEMON_LOGSTORAGE_CMP_APID) {
            mct_set_id(tmp_id, user_list->contexts[i].apid);
        } else if (cmp_flag == MCT_DAEMON_LOGSTORAGE_CMP_CTID) {
            mct_set_id(tmp_id, user_list->contexts[i].ctid);
        } else {
            /* this is for the case when both apid and ctid are wildcard */
            mct_set_id(tmp_id, ".*");
        }

        if (strncmp(id, tmp_id, MCT_ID_SIZE) == 0) {
            if (curr_log_level > 0) {
                mct_daemon_logstorage_send_log_level(daemon,
                                                     daemon_local,
                                                     &user_list->contexts[i],
                                                     ecuid,
                                                     curr_log_level,
                                                     verbose);
            } else { /* The request is to reset log levels */
                mct_daemon_logstorage_reset_log_level(daemon,
                                                      daemon_local,
                                                      &user_list->contexts[i],
                                                      ecuid,
                                                      curr_log_level,
                                                      verbose);
            }
        }
    }

    return MCT_RETURN_OK;
}

/**
 * mct_logstorage_update_context
 *
 * Update log level of a context by updating the daemon internal table
 * The log levels are reset if current log level provided is -1 (not sent to
 * application in this case)
 * Reset and sent to application if current log level provided is 0
 *
 * @param daemon            MctDaemon structure
 * @param daemon_local      MctDaemonLocal structure
 * @param apid              application id
 * @param ctid              context id
 * @param ecuid             ecu id
 * @param curr_log_level    log level to be set to context
 * @param verbose           If set to true verbose information is printed out
 * @return                  0 on success, -1 on error
 */
MctReturnValue mct_logstorage_update_context(MctDaemon *daemon,
                                             MctDaemonLocal *daemon_local,
                                             char *apid,
                                             char *ctid,
                                             char *ecuid,
                                             int curr_log_level,
                                             int verbose)
{
    MctDaemonContext *context = NULL;

    if ((daemon == NULL) || (daemon_local == NULL) || (apid == NULL)
        || (ctid == NULL) || (ecuid == NULL)) {
        mct_vlog(LOG_ERR, "Wrong parameter in function %s\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    context = mct_daemon_context_find(daemon, apid, ctid, ecuid, verbose);

    if (context != NULL) {
        if (curr_log_level > 0) {
            return mct_daemon_logstorage_send_log_level(daemon,
                                                        daemon_local,
                                                        context,
                                                        ecuid,
                                                        curr_log_level,
                                                        verbose);
        } else { /* The request is to reset log levels */
            return mct_daemon_logstorage_reset_log_level(daemon,
                                                         daemon_local,
                                                         context,
                                                         ecuid,
                                                         curr_log_level,
                                                         verbose);
        }
    } else {
        if (strncmp(ecuid, daemon->ecuid, MCT_ID_SIZE) != 0) {
            /* we intentionally have no data provided by passive node. */
            /* We blindly send the log level or reset log level */
            return mct_daemon_logstorage_force_reset_level(daemon,
                                                           daemon_local,
                                                           apid,
                                                           ctid,
                                                           ecuid,
                                                           curr_log_level,
                                                           verbose);
        } else {
            mct_vlog(LOG_WARNING,
                     "%s: No information about APID: %s, CTID: %s, ECU: %s\n",
                     __func__,
                     apid,
                     ctid,
                     ecuid);
            return MCT_RETURN_ERROR;
        }
    }

    return MCT_RETURN_OK;
}

/**
 * mct_logstorage_update_context_loglevel
 *
 * Update all contexts or particular context depending provided key
 *
 * @param daemon            Pointer to MCT Daemon structure
 * @param daemon_local      Pointer to MCT Daemon Local structure
 * @param key               Filter key stored in Hash Map
 * @param curr_log_level    log level to be set to context
 * @param verbose           If set to true verbose information is printed out
 * @return                  0 on success, -1 on error
 */
MctReturnValue mct_logstorage_update_context_loglevel(MctDaemon *daemon,
                                                      MctDaemonLocal *daemon_local,
                                                      char *key,
                                                      int curr_log_level,
                                                      int verbose)
{
    int cmp_flag = 0;
    char apid[MCT_ID_SIZE + 1] = {'\0'};
    char ctid[MCT_ID_SIZE + 1] = {'\0'};
    char ecuid[MCT_ID_SIZE + 1] = {'\0'};

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (key == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (mct_logstorage_split_key(key, apid, ctid, ecuid) != 0) {
        mct_log(LOG_ERR,
                "Error while updating application log levels (split key)\n");
        return MCT_RETURN_ERROR;
    }

    if (ecuid[0] == '\0') { /* ECU id was not specified in filter configuration */
        mct_set_id(ecuid, daemon->ecuid);
    }

    /* check wildcard for both apid and ctid first of all */
    if ((strcmp(ctid, ".*") == 0) && (strcmp(apid, ".*") == 0)) {
        cmp_flag = MCT_DAEMON_LOGSTORAGE_CMP_ECID;

        if (mct_logstorage_update_all_contexts(daemon,
                                               daemon_local,
                                               apid,
                                               curr_log_level,
                                               cmp_flag,
                                               ecuid,
                                               verbose) != 0) {
            return MCT_RETURN_ERROR;
        }
    } else if (strcmp(ctid, ".*") == 0) {
        cmp_flag = MCT_DAEMON_LOGSTORAGE_CMP_APID;

        if (mct_logstorage_update_all_contexts(daemon,
                                               daemon_local,
                                               apid,
                                               curr_log_level,
                                               cmp_flag,
                                               ecuid,
                                               verbose) != 0) {
            return MCT_RETURN_ERROR;
        }
    }
    /* wildcard for application id, find all contexts with context id */
    else if (strcmp(apid, ".*") == 0) {
        cmp_flag = MCT_DAEMON_LOGSTORAGE_CMP_CTID;

        if (mct_logstorage_update_all_contexts(daemon,
                                               daemon_local,
                                               ctid,
                                               curr_log_level,
                                               cmp_flag,
                                               ecuid,
                                               verbose) != 0) {
            return MCT_RETURN_ERROR;
        }
    }
    /* In case of given application id, context id pair, call available context
     * find function */
    else if (mct_logstorage_update_context(daemon,
                                           daemon_local,
                                           apid,
                                           ctid,
                                           ecuid,
                                           curr_log_level,
                                           verbose) != 0) {
        return MCT_RETURN_ERROR;
    }

    return MCT_RETURN_OK;
}

/**
 * mct_daemon_logstorage_reset_application_loglevel
 *
 * Reset storage log level of all running applications
 * 2 steps for resetting
 * 1. Setup storage_loglevel of all contexts configured for the requested device
 *    to -1
 * 2. Re-run update log level for all other configured devices
 *
 * @param daemon        Pointer to MCT Daemon structure
 * @param daemon_local  Pointer to MCT Daemon local structure
 * @param dev_num       Number of attached MCT Logstorage device
 * @param max_device    Maximum storage devices setup by the daemon
 * @param verbose       If set to true verbose information is printed out
 */
void mct_daemon_logstorage_reset_application_loglevel(MctDaemon *daemon,
                                                      MctDaemonLocal *daemon_local,
                                                      int dev_num,
                                                      int max_device,
                                                      int verbose)
{
    MctLogStorage *handle = NULL;
    MctLogStorageFilterList **tmp = NULL;
    int i = 0;
    char key[MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN + 1] = {'\0'};
    unsigned int status;
    int log_level = 0;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) ||
        (daemon->storage_handle == NULL) || (dev_num < 0)) {
        mct_vlog(LOG_ERR,
                 "Invalid function parameters used for %s\n",
                 __func__);
        return;
    }

    handle = &(daemon->storage_handle[dev_num]);

    if ((handle->connection_type != MCT_OFFLINE_LOGSTORAGE_DEVICE_CONNECTED) ||
        (handle->config_status != MCT_OFFLINE_LOGSTORAGE_CONFIG_DONE)) {
        return;
    }

    /* for all filters (keys) check if application context are already running
     * and log level need to be reset*/
    tmp = &(handle->config_list);

    while (*(tmp) != NULL) {
        for (i = 0; i < (*tmp)->num_keys; i++) {
            memset(key, 0, sizeof(key));

            strncpy(key, ((*tmp)->key_list
                          + (i * MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN)),
                    MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN);

            /* mct-daemon wants to reset loglevel if
             * a logstorage device is disconnected.
             */
            log_level = MCT_DAEMON_LOGSTORAGE_RESET_LOGLEVEL;

            mct_logstorage_update_context_loglevel(
                daemon,
                daemon_local,
                key,
                log_level,
                verbose);
        }

        tmp = &(*tmp)->next;
    }

    /* Re-run update log level for all other configured devices */
    for (i = 0; i < max_device; i++) {
        status = daemon->storage_handle[i].config_status;

        if (i == dev_num) {
            continue;
        }

        if (status == MCT_OFFLINE_LOGSTORAGE_CONFIG_DONE) {
            mct_daemon_logstorage_update_application_loglevel(daemon,
                                                              daemon_local,
                                                              i,
                                                              verbose);
        }
    }

    return;
}

/**
 * mct_daemon_logstorage_update_application_loglevel
 *
 * Update log level of all running applications with new filter configuration
 * available due to newly attached MctLogstorage device. The log level is only
 * updated when the current application log level is less than the log level
 * obtained from the storage configuration file
 *
 * @param daemon        Pointer to MCT Daemon structure
 * @param daemon_local  Pointer to MCT Daemon local structure
 * @param dev_num       Number of attached MCT Logstorage device
 * @param verbose       If set to true verbose information is printed out
 */
void mct_daemon_logstorage_update_application_loglevel(MctDaemon *daemon,
                                                       MctDaemonLocal *daemon_local,
                                                       int dev_num,
                                                       int verbose)
{
    MctLogStorage *handle = NULL;
    MctLogStorageFilterList **tmp = NULL;
    char key[MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN + 1] = {'\0'};
    int i = 0;
    int log_level = 0;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (dev_num < 0)) {
        mct_vlog(LOG_ERR,
                 "Invalid function parameters used for %s\n",
                 __func__);
        return;
    }

    handle = &(daemon->storage_handle[dev_num]);

    if ((handle->connection_type != MCT_OFFLINE_LOGSTORAGE_DEVICE_CONNECTED) ||
        (handle->config_status != MCT_OFFLINE_LOGSTORAGE_CONFIG_DONE)) {
        return;
    }

    /* for all filters (keys) check if application or context already running
     * and log level need to be updated*/
    tmp = &(handle->config_list);

    while (*(tmp) != NULL) {
        for (i = 0; i < (*tmp)->num_keys; i++) {
            memset(key, 0, sizeof(key));

            strncpy(key, ((*tmp)->key_list
                          + (i * MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN)),
                    MCT_OFFLINE_LOGSTORAGE_MAX_KEY_LEN);

            /* Obtain storage configuration data */
            log_level = mct_logstorage_get_loglevel_by_key(handle, key);

            if (log_level < 0) {
                mct_log(LOG_ERR, "Failed to get log level by key \n");
                return;
            }

            /* Update context log level with storage configuration log level */
            mct_logstorage_update_context_loglevel(daemon,
                                                   daemon_local,
                                                   key,
                                                   log_level,
                                                   verbose);
        }

        tmp = &(*tmp)->next;
    }

    return;
}

/**
 * mct_daemon_logstorage_get_loglevel
 *
 * Obtain log level as a union of all configured storage devices and filters for
 * the provided application id and context id
 *
 * @param daemon        Pointer to MCT Daemon structure
 * @param max_device    Maximum storage devices setup by the daemon
 * @param apid          Application ID
 * @param ctid          Context ID
 * @return              Log level on success, -1 on error
 */
int mct_daemon_logstorage_get_loglevel(MctDaemon *daemon,
                                       int max_device,
                                       char *apid,
                                       char *ctid)
{
    MctLogStorageFilterConfig *config[MCT_CONFIG_FILE_SECTIONS_MAX] = {0};
    int i = 0;
    int j = 0;
    int8_t storage_loglevel = -1;
    int8_t configured_loglevel = -1;
    int num_config = 0;

    if ((daemon == NULL) || (max_device == 0) || (apid == NULL) || (ctid == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    for (i = 0; i < max_device; i++) {
        if (daemon->storage_handle[i].config_status ==
            MCT_OFFLINE_LOGSTORAGE_CONFIG_DONE) {
            num_config = mct_logstorage_get_config(&(daemon->storage_handle[i]),
                                                   config,
                                                   apid,
                                                   ctid,
                                                   daemon->ecuid);

            if (num_config == 0) {
                mct_log(LOG_DEBUG, "No valid filter configuration found\n");
                continue;
            }

            for (j = 0; j < num_config; j++) {
                if (config[j] == NULL) {
                    continue;
                }

                /* If logstorage configuration do not contain file name,
                 * then it is non verbose control filter, so return level as in this filter */
                if (config[j]->file_name == NULL) {
                    storage_loglevel = config[j]->log_level;
                    break;
                }

                configured_loglevel = config[j]->log_level;
                storage_loglevel = MCT_OFFLINE_LOGSTORAGE_MAX(
                        configured_loglevel,
                        storage_loglevel);
            }
        }
    }

    return storage_loglevel;
}

/**
 * mct_daemon_logstorage_write
 *
 * Write log message to all attached storage device. If the called
 * mct_logstorage_write function is not able to write to the device, MctDaemon
 * will disconnect this device.
 *
 * @param daemon        Pointer to Mct Daemon structure
 * @param user_config   MctDaemon configuration
 * @param data1         message header buffer
 * @param size1         message header buffer size
 * @param data2         message extended header buffer
 * @param size2         message extended header size
 * @param data3         message data buffer
 * @param size3         message data size
 * @return              0 on success, -1 on error, 1 on disable network routing
 */
int mct_daemon_logstorage_write(MctDaemon *daemon,
                                 MctDaemonFlags *user_config,
                                 unsigned char *data1,
                                 int size1,
                                 unsigned char *data2,
                                 int size2,
                                 unsigned char *data3,
                                 int size3)
{
    int i = 0;
    int ret = 0;
    MctLogStorageUserConfig file_config;

    if ((daemon == NULL) || (user_config == NULL) ||
        (user_config->offlineLogstorageMaxDevices <= 0) || (data1 == NULL) ||
        (data2 == NULL) || (data3 == NULL)) {
        mct_vlog(LOG_DEBUG,
                 "%s: message type is not LOG. Skip storing.\n",
                 __func__);
        return -1;
        /* Log Level changed callback */
    }

    /* Copy user configuration */
    file_config.logfile_timestamp = user_config->offlineLogstorageTimestamp;
    file_config.logfile_delimiter = user_config->offlineLogstorageDelimiter;
    file_config.logfile_maxcounter = user_config->offlineLogstorageMaxCounter;
    file_config.logfile_optional_counter = user_config->offlineLogstorageOptionalCounter;
    file_config.logfile_counteridxlen =
        user_config->offlineLogstorageMaxCounterIdx;

    for (i = 0; i < user_config->offlineLogstorageMaxDevices; i++) {
        if (daemon->storage_handle[i].config_status ==
            MCT_OFFLINE_LOGSTORAGE_CONFIG_DONE) {
            int disable_nw = 0;
            if ((ret = mct_logstorage_write(&(daemon->storage_handle[i]),
                                     &file_config,
                                     data1,
                                     size1,
                                     data2,
                                     size2,
                                     data3,
                                     size3,
                                     &disable_nw)) < 0) {
                mct_log(LOG_ERR,
                        "mct_daemon_logstorage_write: failed. "
                        "Disable storage device\n");
                /* MCT_OFFLINE_LOGSTORAGE_MAX_ERRORS happened,
                 * therefore remove logstorage device */
                mct_logstorage_device_disconnected(
                    &(daemon->storage_handle[i]),
                    MCT_LOGSTORAGE_SYNC_ON_DEVICE_DISCONNECT);
            }
            if (i == 0) {
                if (disable_nw == 1) {
                    ret = 1;
                }
            } else {
                mct_vlog(LOG_WARNING,
                         "%s: DisableNetwork is not supported for more than one device yet\n",
                         __func__);
            }
        }
    }

    return ret;
}

/**
 * mct_daemon_logstorage_setup_internal_storage
 *
 * Setup user defined path as offline log storage device
 *
 * @param daemon        Pointer to Mct Daemon structure
 * @param daemon_local  Pointer to Mct Daemon local structure
 * @param path          User configured internal storage path
 * @param bm_allowed    Flag to indicate blockMode allowance
 * @param verbose       If set to true verbose information is printed out
 * @return 0 on sucess, -1 otherwise
 */
int mct_daemon_logstorage_setup_internal_storage(MctDaemon *daemon,
                                                 MctDaemonLocal *daemon_local,
                                                 char *path,
                                                 int bm_allowed,
                                                 int verbose)
{
    int ret = 0;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((path == NULL) || (daemon == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* connect internal storage device */
    /* Device index always used as 0 as it is setup on MCT daemon startup */
    ret = mct_logstorage_device_connected(&(daemon->storage_handle[0]), path);

    if (ret != 0) {
        mct_vlog(LOG_ERR, "%s: Device connect failed\n", __func__);
        return MCT_RETURN_ERROR;
    }

    /* check if log level of running application need an update */
    mct_daemon_logstorage_update_application_loglevel(daemon, daemon_local, 0, verbose);

    /* update BlockMode if necessary */
    if (bm_allowed == MCT_DAEMON_BLOCK_MODE_ENABLED) {
        if (daemon->storage_handle[0].block_mode != MCT_MODE_BLOCKING_UNDEF) {
            if (mct_daemon_user_update_blockmode(daemon,
                                                 MCT_ALL_APPLICATIONS,
                                                 daemon->storage_handle[0].block_mode,
                                                 verbose) != MCT_RETURN_OK) {
                mct_log(LOG_WARNING,
                        "BlockMode request of internal logstorage device failed\n");
            }
        }
    } else {
        mct_log(LOG_INFO,
                "BlockMode requested by Logstorage device, but not allowed\n");
    }

    if (daemon->storage_handle[0].maintain_logstorage_loglevel !=
        MCT_MAINTAIN_LOGSTORAGE_LOGLEVEL_UNDEF) {
        daemon->maintain_logstorage_loglevel =
            daemon->storage_handle[0].maintain_logstorage_loglevel;

        mct_vlog(LOG_DEBUG, "[%s] Startup with maintain loglevel: [%d]\n",
                 __func__,
                 daemon->storage_handle[0].maintain_logstorage_loglevel);
    }

    return ret;
}

void mct_daemon_logstorage_set_logstorage_cache_size(unsigned int size)
{
    /* store given [KB] size in [Bytes] */
    g_logstorage_cache_max = size * 1024;
}

int mct_daemon_logstorage_cleanup(MctDaemon *daemon,
                                  MctDaemonLocal *daemon_local,
                                  int verbose)
{
    int i = 0;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (daemon->storage_handle == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    for (i = 0; i < daemon_local->flags.offlineLogstorageMaxDevices; i++) {
        /* call disconnect on all currently connected devices */
        if (daemon->storage_handle[i].connection_type ==
            MCT_OFFLINE_LOGSTORAGE_DEVICE_CONNECTED) {
            (&daemon->storage_handle[i])->uconfig.logfile_counteridxlen =
                daemon_local->flags.offlineLogstorageMaxCounterIdx;
            (&daemon->storage_handle[i])->uconfig.logfile_delimiter =
                daemon_local->flags.offlineLogstorageDelimiter;
            (&daemon->storage_handle[i])->uconfig.logfile_maxcounter =
                daemon_local->flags.offlineLogstorageMaxCounter;
            (&daemon->storage_handle[i])->uconfig.logfile_timestamp =
                daemon_local->flags.offlineLogstorageTimestamp;
            (&daemon->storage_handle[i])->uconfig.logfile_optional_counter =
                daemon_local->flags.offlineLogstorageOptionalCounter;

            mct_logstorage_device_disconnected(
                &daemon->storage_handle[i],
                MCT_LOGSTORAGE_SYNC_ON_DAEMON_EXIT);
        }
    }

    return 0;
}

int mct_daemon_logstorage_sync_cache(MctDaemon *daemon,
                                     MctDaemonLocal *daemon_local,
                                     char *mnt_point,
                                     int verbose)
{
    int i = 0;
    MctLogStorage *handle = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (mnt_point == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    if (strlen(mnt_point) > 0) { /* mount point is given */
        handle = mct_daemon_logstorage_get_device(daemon,
                                                  daemon_local,
                                                  mnt_point,
                                                  verbose);

        if (handle == NULL) {
            return MCT_RETURN_ERROR;
        } else {
            handle->uconfig.logfile_counteridxlen =
                daemon_local->flags.offlineLogstorageMaxCounterIdx;
            handle->uconfig.logfile_delimiter =
                daemon_local->flags.offlineLogstorageDelimiter;
            handle->uconfig.logfile_maxcounter =
                daemon_local->flags.offlineLogstorageMaxCounter;
            handle->uconfig.logfile_timestamp =
                daemon_local->flags.offlineLogstorageTimestamp;
            handle->uconfig.logfile_optional_counter =
                daemon_local->flags.offlineLogstorageOptionalCounter;

            if (mct_logstorage_sync_caches(handle) != 0) {
                return MCT_RETURN_ERROR;
            }
        }
    } else { /* sync caches for all connected logstorage devices */

        for (i = 0; i < daemon_local->flags.offlineLogstorageMaxDevices; i++) {
            if (daemon->storage_handle[i].connection_type ==
                MCT_OFFLINE_LOGSTORAGE_DEVICE_CONNECTED) {
                daemon->storage_handle[i].uconfig.logfile_counteridxlen =
                    daemon_local->flags.offlineLogstorageMaxCounterIdx;
                daemon->storage_handle[i].uconfig.logfile_delimiter =
                    daemon_local->flags.offlineLogstorageDelimiter;
                daemon->storage_handle[i].uconfig.logfile_maxcounter =
                    daemon_local->flags.offlineLogstorageMaxCounter;
                daemon->storage_handle[i].uconfig.logfile_timestamp =
                    daemon_local->flags.offlineLogstorageTimestamp;
                daemon->storage_handle[i].uconfig.logfile_optional_counter =
                    daemon_local->flags.offlineLogstorageOptionalCounter;

                if (mct_logstorage_sync_caches(&daemon->storage_handle[i]) != 0) {
                    return MCT_RETURN_ERROR;
                }
            }
        }
    }

    return 0;
}

MctLogStorage *mct_daemon_logstorage_get_device(MctDaemon *daemon,
                                                MctDaemonLocal *daemon_local,
                                                char *mnt_point,
                                                int verbose)
{
    int i = 0;
    int len = 0;
    int len1 = 0;
    int len2 = 0;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (mnt_point == NULL)) {
        return NULL;
    }

    len1 = strlen(mnt_point);

    for (i = 0; i < daemon_local->flags.offlineLogstorageMaxDevices; i++) {
        len2 = strlen(daemon->storage_handle[i].device_mount_point);

        /* Check if the requested device path is already used as log storage
         * device. Check for strlen first, to avoid comparison errors when
         * final '/' is given or not */
        len = len1 > len2 ? len2 : len1;

        if (strncmp(daemon->storage_handle[i].device_mount_point, mnt_point, len) == 0) {
            return &daemon->storage_handle[i];
        }
    }

    return NULL;
}
