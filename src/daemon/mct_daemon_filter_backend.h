#ifndef _MCT_DAEMON_FILTER_BACKEND_H
#define _MCT_DAEMON_FILTER_BACKEND_H

#ifndef HAS_PROPRIETARY_FILTER_BACKEND /* only function stubs */
/**
 * @brief Init proprietary filter backend communication
 *
 * @param daemon_local MctDaemonLocal structure pointer
 * @param curr_filter_level default filter level
 * @param verbose      verbose flag
 * @return 0 on success, -1 otherwise
 */
static inline int mct_daemon_filter_backend_init(MctDaemonLocal *daemon_local,
                                                 int curr_filter_level,
                                                 int verbose)
{
    (void)daemon_local;
    (void)curr_filter_level;
    (void)verbose;
    return 0;
}

/**
 * @brief Deinit proprietary filter backend communication
 *
 * @param daemon_local MctDaemonLocal structure pointer
 * @param verbose      verbose flag
 * @return 0 on success, -1 otherwise
 */
static inline int mct_daemon_filter_backend_deinit(MctDaemonLocal *daemon_local,
                                                   int verbose)
{
    (void)daemon_local;
    (void)verbose;
    return 0;
}

/**
 * @brief proprietary filter backend dispatch wrapper function
 *
 * @param daemon_local  MctDaemonLocal structure pointer
 * @param verbose       verbose flag
 * @return 0 on success, -1 otherwise
 */
static inline int mct_daemon_filter_backend_dispatch(
    MctDaemonLocal *daemon_local,
    int *verbose)
{
    (void)daemon_local;
    (void)verbose;
    return 0;
}
#else
/**
 * @brief Init proprietary filter backend communication
 *
 * @param daemon_local MctDaemonLocal structure pointer
 * @param curr_filter_level default filter level
 * @param verbose      verbose flag
 * @return 0 on success, -1 otherwise
 */
int mct_daemon_filter_backend_init(MctDaemonLocal *daemon_local,
                                   int curr_filter_level,
                                   int verbose);

/**
 * @brief Deinit proprietary filter backend communication
 *
 * @param daemon_local MctDaemonLocal structure pointer
 * @param verbose      verbose flag
 * @return 0 on success, -1 otherwise
 */
int mct_daemon_filter_backend_deinit(MctDaemonLocal *daemon_local, int verbose);

/**
 * @brief proprietary filter backend dispatch wrapper function
 *
 * @param daemon_local  MctDaemonLocal structure pointer
 * @param verbose       verbose flag
 * @return 0 on success, -1 otherwise
 */
int mct_daemon_filter_backend_dispatch(MctDaemonLocal *daemon_local,
                                       int *verbose);
#endif /* HAS_PROPRIETARY_FILTER_BACKEND */

void mct_daemon_filter_backend_level_changed(unsigned int level,
                                             void *ptr1,
                                             void *ptr2);
#endif
