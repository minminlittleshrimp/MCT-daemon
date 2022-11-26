#include <syslog.h>
#include <sys/poll.h>

#include "mct_common.h"
#include "mct-daemon.h"
#include "mct-daemon_cfg.h"
#include "mct_daemon_filter_types.h"
#include "mct_daemon_filter.h"
#include "mct_daemon_event_handler.h"
#include "mct_daemon_connection.h"
#include "mct_daemon_connection_types.h"

/**
 * @brief Callback for security level changed event
 *
 * @param level     new security level
 * @param ptr1      pointer to MctDaemonLocal
 * @param ptr2      pointer to verbose flag
 */
void mct_daemon_filter_backend_level_changed(unsigned int level,
                                             void *ptr1,
                                             void *ptr2)
{
    if ((ptr1 == NULL) || (ptr2 == NULL)) {
        mct_vlog(LOG_CRIT, "%s: has wrong paramters\n", __func__);
        return;
    }

    PRINT_FUNCTION_VERBOSE(*((int *)ptr2));

    MctDaemonLocal *daemon_local = (MctDaemonLocal *)ptr1;
    MctFilterConfiguration *curr = daemon_local->pFilter.current;

    /* Nothing need to be done if the received level is included in
     * the current level range */
    if ((level >= curr->level_min) && (level <= curr->level_max)) {
        return;
    }

    int ret = mct_daemon_filter_change_filter_level(daemon_local,
                                                    level,
                                                    daemon_local->flags.vflag);

    if (ret != 0) {
        mct_log(LOG_CRIT, "Changing filter level failed!\n");
    }
}

/**
 * @brief Callback for backend connection established event
 *
 * @param ptr1 pointer to MctDaemonLocal
 * @param ptr2 pointer to verbose flag
 */
void mct_daemon_filter_backend_connected(void *ptr1, void *ptr2)
{
    if ((ptr1 == NULL) || (ptr2 == NULL)) {
        mct_vlog(LOG_CRIT, "%s: wrong parameters\n", __func__);
        return;
    }

    PRINT_FUNCTION_VERBOSE(*((int *)ptr2));

    /* when connected, retrieve filter level; 1 == true */
    unsigned int updated_filter_level = ald_plugin_get_security_level(1);
    mct_daemon_filter_backend_level_changed(updated_filter_level,
                                            ptr1,
                                            ptr2);
}

/**
 * @brief Callback for backend connection lost event
 *
 * @param ptr1 pointer to MctDaemonLocal
 * @param ptr2 pointer to verbose flag
 */
void mct_daemon_filter_backend_disconnected(void *ptr1, void *ptr2)
{
    if ((ptr1 == NULL) || (ptr2 == NULL)) {
        mct_vlog(LOG_CRIT, "%s: wrong parameters\n", __func__);
        return;
    }

    PRINT_FUNCTION_VERBOSE(*((int *)ptr2));

    /* when connection to ALD is lost, the default level need to be set again
     * for this we need to access the filter pointer */

    MctDaemonLocal *daemon_local = (MctDaemonLocal *)ptr1;
    unsigned int default_filter_level = (unsigned int)
        daemon_local->pFilter.default_level;
    mct_daemon_filter_backend_level_changed(default_filter_level,
                                            ptr1,
                                            ptr2);
}

static const ald_plugin_callbacks_t ald_plugin_callbacks = {
    .connection_clbk = mct_daemon_filter_backend_connected,
    .disConnection_clbk = mct_daemon_filter_backend_disconnected,
    .calculate_log_level_clbk = mct_daemon_filter_backend_level_changed,
    .cmd_received_clbk = NULL /* not interested in receiving commands */
};

int mct_daemon_filter_backend_init(MctDaemonLocal *daemon_local,
                                   int curr_filter_level,
                                   int verbose)
{
    int fd = -1;
    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon_local == NULL) {
        return -1;
    }

    int ret = ald_plugin_init(&ald_plugin_callbacks, curr_filter_level);

    if (ret != 0) {
        mct_log(LOG_ERR, "ALD plugin could not be initialized\n");
        return ret;
    }

    fd = ald_plugin_get_pollfd();

    ret = mct_connection_create(daemon_local,
                                &daemon_local->pEvent,
                                fd,
                                POLLIN,
                                MCT_CONNECTION_FILTER);

    if (ret != 0) {
        mct_log(LOG_ERR, "Filter backend connection creation failed\n");
        return ret;
    }

    return 0;
}

int mct_daemon_filter_backend_deinit(MctDaemonLocal *daemon_local, int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon_local == NULL) {
        return -1;
    }

    return ald_plugin_deinit(daemon_local, &verbose);
}

int mct_daemon_filter_backend_dispatch(MctDaemonLocal *daemon_local,
                                       int *verbose)
{
    if ((daemon_local == NULL) || (verbose == NULL)) {
        return -1;
    }

    PRINT_FUNCTION_VERBOSE(*verbose);

    return ald_plugin_dispatch(daemon_local, verbose);
}
