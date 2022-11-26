#ifndef _MCT_LOGSTORAGE_UDEV_H_
#define _MCT_LOGSTORAGE_UDEV_H_

/**
 * Initialize udev connection
 *
 * @return 0 on success, -1 on error
 */
int mct_logstorage_udev_init(void);

/**
 * Clean-up udev connection
 *
 * @return 0 on success, -1 on error
 */
int mct_logstorage_udev_deinit(void);

#endif
