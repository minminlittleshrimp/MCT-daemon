#define pr_fmt(fmt) "Common control: "fmt

#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "mct_common.h"
#include "mct_protocol.h"
#include "mct_client.h"

#include "mct-control-common.h"

#define MCT_CTRL_APID    "MCTC"
#define MCT_CTRL_CTID    "MCTC"

/** @brief Analyze the daemon answer
 *
 * This function as to be provided by the user of the connection.
 *
 * @param answer  The textual answer of the daemon
 * @param payload The daemons answer payload
 * @param length  The daemons answer payload length
 *
 * @return User defined.
 */
static int (*response_analyzer_cb)(char *, void *, int);

static pthread_t daemon_connect_thread;
static MctClient g_client;
static int callback_return = -1;
static pthread_mutex_t answer_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t answer_cond = PTHREAD_COND_INITIALIZER;

static int local_verbose;
static char local_ecuid[MCT_CTRL_ECUID_LEN]= {0}; /* Name of ECU */
static int local_timeout;
static char local_filename[MCT_MOUNT_PATH_MAX]= {0}; /* Path to mct.conf */

int get_verbosity(void)
{
    return local_verbose;
}

void set_verbosity(int v)
{
    local_verbose = !!v;
}

char *get_ecuid(void)
{
    return local_ecuid;
}

void set_ecuid(char *ecuid)
{
    char *ecuid_conf = NULL;

    if (local_ecuid != ecuid) {
        /* If user pass NULL, read ECUId from mct.conf */
        if (ecuid == NULL) {
            if (mct_parse_config_param("ECUId", &ecuid_conf) == 0) {
                memset(local_ecuid, 0, MCT_CTRL_ECUID_LEN);
                strncpy(local_ecuid, ecuid_conf, MCT_CTRL_ECUID_LEN);
                local_ecuid[MCT_CTRL_ECUID_LEN - 1] = '\0';
            }
            else {
                pr_error("Cannot read ECUid from mct.conf\n");
            }
        }
        else {
            /* Set user passed ECUID */
            memset(local_ecuid, 0, MCT_CTRL_ECUID_LEN);
            strncpy(local_ecuid, ecuid, MCT_CTRL_ECUID_LEN);
            local_ecuid[MCT_CTRL_ECUID_LEN - 1] = '\0';
        }
    }
}

void set_conf(char *file_path)
{
    if (file_path != NULL) {
        memset(local_filename, 0, MCT_MOUNT_PATH_MAX);
        strncpy(local_filename, file_path, MCT_MOUNT_PATH_MAX);
        local_filename[MCT_MOUNT_PATH_MAX - 1] = '\0';
    }
    else {
        pr_error("Argument is NULL\n");
    }
}

int get_timeout(void)
{
    return local_timeout;
}

void set_timeout(int t)
{
    local_timeout = MCT_CTRL_TIMEOUT;

    if (t > 1)
        local_timeout = t;
    else
        pr_error("Timeout to small. Set to default: %d",
                 MCT_CTRL_TIMEOUT);
}

void set_send_serial_header(const int value)
{
    g_client.send_serial_header = value;
}

void set_resync_serial_header(const int value)
{
    g_client.resync_serial_header = value;
}

int mct_parse_config_param(char *config_id, char **config_data)
{
    FILE *pFile = NULL;
    int value_length = MCT_LINE_LEN;
    char line[MCT_LINE_LEN - 1] = { 0 };
    char token[MCT_LINE_LEN] = { 0 };
    char value[MCT_LINE_LEN] = { 0 };
    char *pch = NULL;
    const char *filename = NULL;

    if (*config_data != NULL)
        *config_data = NULL;

    /* open configuration file */
    if (local_filename[0] != 0) {
        filename = local_filename;
    } else {
        filename = CONFIGURATION_FILES_DIR "/mct.conf";
    }
    pFile = fopen(filename, "r");

    if (pFile != NULL) {
        while (1) {
            /* fetch line from configuration file */
            if (fgets(line, value_length - 1, pFile) != NULL) {
                if (strncmp(line, config_id, strlen(config_id)) == 0) {
                    pch = strtok(line, " =\r\n");
                    token[0] = 0;
                    value[0] = 0;

                    while (pch != NULL) {
                        if (token[0] == 0) {
                            strncpy(token, pch, sizeof(token) - 1);
                            token[sizeof(token) - 1] = 0;
                        }
                        else {
                            strncpy(value, pch, sizeof(value) - 1);
                            value[sizeof(value) - 1] = 0;
                            break;
                        }

                        pch = strtok(NULL, " =\r\n");
                    }

                    if (token[0] && value[0]) {
                        if (strcmp(token, config_id) == 0) {
                            *(config_data) = (char *)
                                calloc(MCT_DAEMON_FLAG_MAX, sizeof(char));
                            memcpy(*config_data,
                                   value,
                                   MCT_DAEMON_FLAG_MAX - 1);
                        }
                    }
                }
            }
            else {
                break;
            }
        }

        fclose (pFile);
    }
    else {
        fprintf(stderr, "Cannot open configuration file: %s\n", filename);
    }

    if (*config_data == NULL)
        return -1;

    return 0;
}

/** @brief Prepare the extra headers of a MCT message
 *
 * Modifies the extra headers of the message so that it can be sent.
 *
 * @param msg The message to be prepared
 * @param header The base header to be used.
 *
 * @return 0 on success, -1 otherwise.
 */
static int prepare_extra_headers(MctMessage *msg, uint8_t *header)
{
    uint32_t shift = 0;

    pr_verbose("Preparing extra headers.\n");

    if (!msg || !header)
        return -1;

    shift = (uint32_t) (sizeof(MctStorageHeader) +
        sizeof(MctStandardHeader) +
        MCT_STANDARD_HEADER_EXTRA_SIZE(msg->standardheader->htyp));

    /* Set header extra parameters */
    mct_set_id(msg->headerextra.ecu, get_ecuid());

    msg->headerextra.tmsp = mct_uptime();

    /* Copy header extra parameters to header buffer */
    if (mct_message_set_extraparameters(msg, get_verbosity()) == -1) {
        pr_error("Cannot copy header extra parameter\n");
        return -1;
    }

    /* prepare extended header */
    msg->extendedheader = (MctExtendedHeader *)(header + shift);

    msg->extendedheader->msin = MCT_MSIN_CONTROL_REQUEST;

    msg->extendedheader->noar = 1; /* one payload packet */

    /* Dummy values have to be set */
    mct_set_id(msg->extendedheader->apid, MCT_CTRL_APID);
    mct_set_id(msg->extendedheader->ctid, MCT_CTRL_CTID);

    return 0;
}

/** @brief Prepare the headers of a MCT message
 *
 * Modifies the headers of the message so that it can be sent.
 *
 * @param msg The message to be prepared
 * @param header The base header to be used.
 *
 * @return 0 on success, -1 otherwise.
 */
static int prepare_headers(MctMessage *msg, uint8_t *header)
{
    uint32_t len = 0;

    pr_verbose("Preparing headers.\n");

    if (!msg || !header)
        return -1;

    msg->storageheader = (MctStorageHeader *)header;

    if (mct_set_storageheader(msg->storageheader, "") == -1) {
        pr_error("Storage header initialization failed.\n");
        return -1;
    }

    /* prepare standard header */
    msg->standardheader =
        (MctStandardHeader *)(header + sizeof(MctStorageHeader));

    msg->standardheader->htyp = MCT_HTYP_WEID |
        MCT_HTYP_WTMS | MCT_HTYP_UEH | MCT_HTYP_PROTOCOL_VERSION1;

#if (BYTE_ORDER == BIG_ENDIAN)
    msg->standardheader->htyp = (msg->standardheader->htyp | MCT_HTYP_MSBF);
#endif

    msg->standardheader->mcnt = 0;

    /* prepare length information */
    msg->headersize = (uint32_t) (sizeof(MctStorageHeader) +
        sizeof(MctStandardHeader) +
        sizeof(MctExtendedHeader) +
        MCT_STANDARD_HEADER_EXTRA_SIZE(msg->standardheader->htyp));

    len = (uint32_t) (msg->headersize - sizeof(MctStorageHeader) + msg->datasize);

    if (len > UINT16_MAX) {
        pr_error("Message header is too long.\n");
        return -1;
    }

    msg->standardheader->len = MCT_HTOBE_16(len);

    return 0;
}

/** @brief Prepare a MCT message.
 *
 * The MCT message is built using the data given by the user.
 * The data is basically composed of a buffer and a size.
 *
 * @param data The message body to be used to build the MCT message.
 *
 * @return 0 on success, -1 otherwise.
 */
static MctMessage *mct_control_prepare_message(MctControlMsgBody *data)
{
    MctMessage *msg = NULL;

    pr_verbose("Preparing message.\n");

    if (data == NULL) {
        pr_error("Data for message body is NULL\n");
        return NULL;
    }

    msg = calloc(1, sizeof(MctMessage));

    if (msg == NULL) {
        pr_error("Cannot allocate memory for Mct Message\n");
        return NULL;
    }

    if (mct_message_init(msg, get_verbosity()) == -1) {
        pr_error("Cannot initialize Mct Message\n");
        free(msg);
        return NULL;
    }

    /* prepare payload of data */
    msg->databuffersize = msg->datasize = (uint32_t) data->size;

    /* Allocate memory for Mct Message's buffer */
    msg->databuffer = (uint8_t *)calloc(1, data->size);

    if (msg->databuffer == NULL) {
        pr_error("Cannot allocate memory for data buffer\n");
        free(msg);
        return NULL;
    }

    /* copy data into message */
    memcpy(msg->databuffer, data->data, data->size);

    /* prepare storage header */
    if (prepare_headers(msg, msg->headerbuffer)) {
        mct_message_free(msg, get_verbosity());
        free(msg);
        return NULL;
    }

    /* prepare extra headers */
    if (prepare_extra_headers(msg, msg->headerbuffer)) {
        mct_message_free(msg, get_verbosity());
        free(msg);
        return NULL;
    }

    return msg;
}

/** @brief Initialize the connection with the daemon
 *
 * The connection is initialized using an internal callback. The user's
 * response analyzer will be finally executed by this callback.
 * The client pointer is used to established the connection.
 *
 * @param client A pointer to a valid client structure
 * @param cb The internal callback to be executed while receiving a new message
 *
 * @return 0 on success, -1 otherwise.
 */
static int mct_control_init_connection(MctClient *client, void *cb)
{
    int (*callback)(MctMessage *message, void *data) = cb;

    if (!cb || !client) {
        pr_error("%s: Invalid parameters\n", __func__);
        return -1;
    }

    pr_verbose("Initializing the connection.\n");

    if (mct_client_init(client, get_verbosity()) != 0) {
        pr_error("Failed to register callback (NULL)\n");
        return -1;
    }

    mct_client_register_message_callback(callback);

    client->socketPath = NULL;

    if (mct_parse_config_param("ControlSocketPath", &client->socketPath) != 0) {
        /* Failed to read from conf, copy default */
        if (mct_client_set_socket_path(client, MCT_DAEMON_DEFAULT_CTRL_SOCK_PATH) == -1) {
            pr_error("set socket path didn't succeed\n");
            return -1;
        }
    }

    client->mode = MCT_CLIENT_MODE_UNIX;

    return mct_client_connect(client, get_verbosity());
}

/** @brief Daemon listener function
 *
 * This function will continuously read on the MCT socket, until an error occurs
 * or the thread executing this function is canceled.
 *
 * @param data Thread parameter
 *
 * @return The thread parameter given as argument.
 */
static void *mct_control_listen_to_daemon(void *data)
{
    pr_verbose("Ready to receive MCT answers.\n");
    mct_client_main_loop(&g_client, NULL, get_verbosity());
    return data;
}

/** @brief Internal callback for MCT response
 *
 * This function is called by the mct_client_main_loop once a response is read
 * from the MCT socket.
 * After some basic checks, the user's response analyzer is called. The return
 * value of the analyzer is then provided back to the mct_control_send_message
 * function to be given back as a return value.
 * As this function is called in a dedicated thread, the return value is
 * provided using a global variable.
 * Access to this variable is controlled through a dedicated mutex.
 * New values are signaled using a dedicated condition variable.
 *
 * @param message The MCT answer
 * @param data Unused
 *
 * @return The analyzer return value or -1 on early errors.
 */
static int mct_control_callback(MctMessage *message, void *data)
{
    char text[MCT_RECEIVE_BUFSIZE] = { 0 };
    (void)data;

    if (message == NULL) {
        pr_error("Received message is null\n");
        return -1;
    }

    /* prepare storage header */
    if (MCT_IS_HTYP_WEID(message->standardheader->htyp))
        mct_set_storageheader(message->storageheader, message->headerextra.ecu);
    else
        mct_set_storageheader(message->storageheader, "LCTL");

    mct_message_header(message, text, MCT_RECEIVE_BUFSIZE, get_verbosity());

    /* Extracting payload */
    mct_message_payload(message, text,
                        MCT_RECEIVE_BUFSIZE,
                        MCT_OUTPUT_ASCII,
                        get_verbosity());

    /*
     * Checking payload with the provided callback and return the result
     */
    pthread_mutex_lock(&answer_lock);
    callback_return = response_analyzer_cb(text,
                                           message->databuffer,
                                           (int) message->datasize);
    pthread_cond_signal(&answer_cond);
    pthread_mutex_unlock(&answer_lock);

    return callback_return;
}

/** @brief Send a message to the daemon and wait for the asynchronous answer.
 *
 * The answer is received and analyzed by a dedicated thread. Thus we need
 * to wait for the signal from this thread and then read the return value
 * to be provided to the user.
 * In case of timeout, this function fails.
 * The message provided by the user is formated in MCT format before sending.
 *
 * @param body The message provided by the user
 * @param timeout The time to wait before considering that no answer will come
 *
 * @return The user response analyzer return value, -1 in case of early error.
 */
int mct_control_send_message(MctControlMsgBody *body, int timeout)
{
    struct timespec t;
    MctMessage *msg = NULL;

    if (!body) {
        pr_error("%s: Invalid input.\n", __func__);
        return -1;
    }

    if (clock_gettime(CLOCK_REALTIME, &t) == -1) {
        pr_error("Cannot read system time.\n");
        return -1;
    }

    t.tv_sec += timeout;

    /* send command to daemon here */
    msg = mct_control_prepare_message(body);

    if (msg == NULL) {
        pr_error("Control message preparation failed\n");
        return -1;
    }

    pthread_mutex_lock(&answer_lock);

    /* Re-init the return value */
    callback_return = -1;

    if (mct_client_send_message_to_socket(&g_client, msg) != MCT_RETURN_OK) {
        pr_error("Sending message to daemon failed\n");
        mct_message_free(msg, get_verbosity());
        free(msg);
        return -1;
    }

    /* If we timeout the lock is not taken back */
    if (!pthread_cond_timedwait(&answer_cond, &answer_lock, &t))
        pthread_mutex_unlock(&answer_lock);

    /* Destroying the message */
    mct_message_free(msg, get_verbosity());
    free(msg);

    /* At this point either the value is already correct, either it's still -1.
     * Then, we don't care to lock the access.
     */
    return callback_return;
}

/** @brief Send a message to the daemon and wait for the asynchronous answer.
 *
 * The answer is received and analyzed by a dedicated thread. Thus we need
 * to wait for the signal from this thread and then read the return value
 * to be provided to the user.
 * In case of timeout, this function fails.
 * The message provided by the user is formated in MCT format before sending.
 *
 * @param body The message provided by the user
 * @param apid Application ID
 * @param ctid Context ID
 * @param timeout The time to wait before considering that no answer will come
 *
 * @return The user response analyzer return value, -1 in case of early error.
 */
int mct_control_send_injection_message(MctControlMsgBody *body, char *apid, char *ctid, int timeout)
{
    struct timespec t;
    MctMessage *msg = NULL;

    if (!body)
    {
        pr_error("%s: Invalid input.\n", __func__);
        return -1;
    }

    if (clock_gettime(CLOCK_REALTIME, &t) == -1)
    {
        pr_error("Cannot read system time.\n");
        return -1;
    }

    t.tv_sec += timeout;

    /* send command to daemon here */
    msg = mct_control_prepare_message(body);

    if (msg == NULL)
    {
        pr_error("Control message preparation failed\n");
        return -1;
    }

    /* Overwrite appid and ctid in extended header required for injection message*/
    mct_set_id(msg->extendedheader->apid, apid);
    mct_set_id(msg->extendedheader->ctid, ctid);

    pthread_mutex_lock(&answer_lock);

    /* Re-init the return value */
    callback_return = -1;

    if (mct_client_send_message_to_socket(&g_client, msg) != MCT_RETURN_OK) {
        pr_error("Sending message to daemon failed\n");
        mct_message_free(msg, get_verbosity());
        free(msg);
        return -1;
    }

    /* If we timeout the lock is not taken back */
    if (!pthread_cond_timedwait(&answer_cond, &answer_lock, &t))
    {
        pthread_mutex_unlock(&answer_lock);
    }

    /* Destroying the message */
    mct_message_free(msg, get_verbosity());
    free(msg);

    /* At this point either the value is already correct, either it's still -1.
     * Then, we don't care to lock the access.
     */
    return callback_return;
}

/** @brief Control communication initialization
 *
 * This will prepare the MCT connection and the thread dedicated to the
 * response listening.
 *
 * @param response_analyzer User defined function used to analyze the response
 * @param ecuid The ECUID to provide to the daemon
 * @param verbosity The verbosity level
 *
 * @return 0 on success, -1 otherwise.
 */
int mct_control_init(int (*response_analyzer)(char *, void *, int),
                     char *ecuid,
                     int verbosity)
{
    if (!response_analyzer || !ecuid) {
        pr_error("%s: Invalid input.\n", __func__);
        return -1;
    }

    response_analyzer_cb = response_analyzer;
    set_ecuid(ecuid);
    set_verbosity(verbosity);

    if (mct_control_init_connection(&g_client, mct_control_callback) != 0) {
        pr_error("Connection initialization failed\n");
        mct_client_cleanup(&g_client, get_verbosity());
        return -1;
    }

    /* Contact MCT daemon */
    if (pthread_create(&daemon_connect_thread,
                       NULL,
                       mct_control_listen_to_daemon,
                       NULL) != 0) {
        pr_error("Cannot create thread to communicate with MCT daemon.\n");
        return -1;
    }

    return 0;
}

/** @brief Control communication clean-up
 *
 * Cancels the listener thread and clean=up the mct client structure.
 *
 * @return 0 on success, -1 otherwise.
 */
int mct_control_deinit(void)
{
    /* At this stage, we want to stop sending/receiving
     * from mct-daemon. So in order to avoid cancellation
     * at recv(), shutdown and close the socket
     */
    if (g_client.receiver.fd) {
        shutdown(g_client.receiver.fd, SHUT_RDWR);
        close(g_client.receiver.fd);
        g_client.receiver.fd = -1;
    }
    /* Waiting for thread to complete */
    pthread_join(daemon_connect_thread, NULL);
    /* Closing the socket */
    return mct_client_cleanup(&g_client, get_verbosity());
}

