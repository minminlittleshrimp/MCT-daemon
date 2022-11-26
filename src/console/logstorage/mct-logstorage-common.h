#ifndef _MCT_LOGSTORAGE_COMMON_H_
#define _MCT_LOGSTORAGE_COMMON_H_

#define CONF_NAME "mct_logstorage.conf"

#define EVENT_UNMOUNTING    0
#define EVENT_MOUNTED       1
#define EVENT_SYNC_CACHE    2

typedef enum
{
    CTRL_NOHANDLER = 0, /**< one shot application */
    CTRL_UDEV,          /**< Handles udev events */
    CTRL_PROPRIETARY    /**< Handles proprietary event */
} MctLogstorageHandler;

MctLogstorageHandler get_handler_type(void);
void set_handler_type(char *);

char *get_default_path(void);
void set_default_path(char *);

int get_default_event_type(void);
void set_default_event_type(long type);

typedef struct {
    int fd;
    int (*callback)(void); /* callback for event handling */
    void *prvt; /* Private data */
} MctLogstorageCtrl;

/* Get a reference to the logstorage control instance */
MctLogstorageCtrl *get_logstorage_control(void);
void *mct_logstorage_get_handler_cb(void);
int mct_logstorage_get_handler_fd(void);
int mct_logstorage_init_handler(void);
int mct_logstorage_deinit_handler(void);

/**
 * Send an event to the mct daemon
 *
 * @param type Event type (EVENT_UNMOUNTING/EVENT_MOUNTED)
 * @param mount_point The mount point path concerned by this event
 *
 * @return  0 on success, -1 on error
 */
int mct_logstorage_send_event(int, char *);

/** @brief Search for config file in given mount point
 *
 * The file is searched at the top directory. The function exits once it
 * founds it.
 *
 * @param mnt_point The mount point to check
 *
 * @return 1 if the file is found, 0 otherwise.
 */
int mct_logstorage_check_config_file(char *);

/** @brief Check if given mount point is writable
 *
 * @param mnt_point The mount point to check
 *
 * @return 1 if the file is writable, 0 otherwise.
 */
int mct_logstorage_check_directory_permission(char *mnt_point);

#endif
