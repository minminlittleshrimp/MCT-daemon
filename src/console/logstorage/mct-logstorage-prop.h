#ifndef _MCT_LOGSTORAGE_PROP_H_
#define _MCT_LOGSTORAGE_PROP_H_

#ifndef HAS_PROPRIETARY_LOGSTORAGE
/** @brief Initialize proprietary connection
 *
 * @return 0
 */
static inline int mct_logstorage_prop_init(void)
{
    return 0;
}

/** @brief Clean-up proprietary connection
 *
 * @return 0
 */
static inline int mct_logstorage_prop_deinit(void)
{
    return 0;
}

/** @brief Check whether user wants to use proprietary handler
 *
 * @return 0
 */
static inline int check_proprietary_handling(char *type)
{
    (void)type; return 0;
}
#else
/**
 * Initialize proprietary connection
 *
 * @return 0 on success, -1 on error
 */
int mct_logstorage_prop_init(void);

/**
 * Clean-up proprietary connection
 *
 * @return 0 on success, -1 on error
 */
int mct_logstorage_prop_deinit(void);

/**
 * Check whether user wants to use proprietary event handler
 *
 * @return 1 if yes, 0 either.
 */
int check_proprietary_handling(char *);
#endif /* HAS_PROPRIETARY_LOGSTORAGE */

#endif /* _MCT_LOGSTORAGE_PROP_H_ */
