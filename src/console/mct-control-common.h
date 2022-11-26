#ifndef _MCT_CONTROL_COMMON_H_
#define _MCT_CONTROL_COMMON_H_

#include <stdio.h>

#include "mct_common.h"

#define MCT_CTRL_TIMEOUT 10

#define MCT_CTRL_ECUID_LEN 10
#define MCT_DAEMON_FLAG_MAX 256

#define JSON_FILTER_SIZE 200     /* Size in bytes, that the definition of one filter with all parameters needs */

#ifndef pr_fmt
#   define pr_fmt(fmt) fmt
#endif

#ifndef USE_STDOUT
#   define PRINT_OUT stderr
#else
#   define PRINT_OUT stdout
#endif

#define pr_error(fmt, ...) \
    ({ fprintf(PRINT_OUT, pr_fmt(fmt), ## __VA_ARGS__); fflush(PRINT_OUT); })
#define pr_verbose(fmt, ...) \
    ({ if (get_verbosity()) { fprintf(PRINT_OUT, pr_fmt(fmt), ## __VA_ARGS__); fflush(PRINT_OUT); } })

#define MCT_CTRL_DEFAULT_ECUID "ECU1"

#define MCT_DAEMON_DEFAULT_CTRL_SOCK_PATH "/tmp/mct-ctrl.sock"

#define NANOSEC_PER_MILLISEC 1000000
#define NANOSEC_PER_SEC 1000000000

/* To be used as Mct Message body when sending to MCT daemon */
typedef struct
{
    void *data; /**< data to be send to MCT Daemon */
    uint32_t size;   /**< size of that data */
} MctControlMsgBody;

/* As verbosity, ecuid, timeout, send_serial_header, resync_serial_header are
 * needed during the communication, defining getter and setters here.
 * Then there is no need to define them in the control's user application.
 */
int get_verbosity(void);
void set_verbosity(int);

char *get_ecuid(void);
void set_ecuid(char *);

void set_conf(char *);

int get_timeout(void);
void set_timeout(int);

void set_send_serial_header(const int value);
void set_resync_serial_header(const int value);

/* Parse mct.conf file and return the value of requested configuration */
int mct_parse_config_param(char *config_id, char **config_data);

/* Initialize the connection to the daemon */
int mct_control_init(int (*response_analyser)(char *, void *, int),
                     char *ecuid,
                     int verbosity);

/* Send a message to the daemon. The call is not thread safe. */
int mct_control_send_message(MctControlMsgBody *, int);

/* Send injection message to the daemon. The call is not thread safe. */
int mct_control_send_injection_message(MctControlMsgBody *body,
                                       char *apid, char *ctid, int timeout);
/* Destroys the connection to the daemon */
int mct_control_deinit(void);

#endif
