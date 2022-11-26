#ifndef MCT_CLIENT_H
#   define MCT_CLIENT_H

/**
 * \defgroup clientapi MCT Client API
 * \addtogroup clientapi
 \{
 */

#   include "mct_types.h"
#   include "mct_common.h"
#include <stdbool.h>

typedef enum
{
    MCT_CLIENT_MODE_UNDEFINED = -1,
    MCT_CLIENT_MODE_TCP,
    MCT_CLIENT_MODE_SERIAL,
    MCT_CLIENT_MODE_UNIX,
    MCT_CLIENT_MODE_UDP_MULTICAST
} MctClientMode;

typedef struct
{
    MctReceiver receiver;  /**< receiver pointer to mct receiver structure */
    int sock;              /**< sock Connection handle/socket */
    char *servIP;          /**< servIP IP adress/Hostname of interface */
    char *hostip;          /**< hostip IP address of UDP host receiver interface */
    int port;              /**< Port for TCP connections (optional) */
    char *serialDevice;    /**< serialDevice Devicename of serial device */
    char *socketPath;      /**< socketPath Unix socket path */
    char ecuid[4];         /**< ECUiD */
    speed_t baudrate;      /**< baudrate Baudrate of serial interface, as speed_t */
    MctClientMode mode;    /**< mode MctClientMode */
    int send_serial_header;    /**< (Boolean) Send MCT messages with serial header */
    int resync_serial_header;  /**< (Boolean) Resync to serial header on all connection */
} MctClient;

#   ifdef __cplusplus
extern "C" {
#   endif

void mct_client_register_message_callback(int (*registerd_callback)(MctMessage *message, void *data));
void mct_client_register_fetch_next_message_callback(bool (*registerd_callback)(void *data));

/**
 * Initialising mct client structure with a specific port
 * @param client pointer to mct client structure
 * @param port The port for the tcp connection
 * @param verbose if set to true verbose information is printed out.
 * @return negative value if there was an error
 */
int mct_client_init_port(MctClient *client, int port, int verbose);

/**
 * Initialising mct client structure
 * @param client pointer to mct client structure
 * @param verbose if set to true verbose information is printed out.
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_init(MctClient *client, int verbose);
/**
 * Connect to mct daemon using the information from the mct client structure
 * @param client pointer to mct client structure
 * @param verbose if set to true verbose information is printed out.
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_connect(MctClient *client, int verbose);
/**
 * Cleanup mct client structure
 * @param client pointer to mct client structure
 * @param verbose if set to true verbose information is printed out.
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_cleanup(MctClient *client, int verbose);
/**
 * Main Loop of mct client application
 * @param client pointer to mct client structure
 * @param data pointer to data to be provided to the main loop
 * @param verbose if set to true verbose information is printed out.
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_main_loop(MctClient *client, void *data, int verbose);

/**
 * Send a message to the daemon through the socket.
 * @param client pointer to mct client structure.
 * @param msg The message to be send in MCT format.
 * @return Value from MctReturnValue enum.
 */
MctReturnValue mct_client_send_message_to_socket(MctClient *client, MctMessage *msg);

/**
 * Send ancontrol message to the mct daemon
 * @param client pointer to mct client structure
 * @param apid application id
 * @param ctid context id
 * @param payload Buffer filled with control message data
 * @param size Size of control message data
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_send_ctrl_msg(MctClient *client, char *apid, char *ctid, uint8_t *payload, uint32_t size);
/**
 * Send an injection message to the mct daemon
 * @param client pointer to mct client structure
 * @param apid application id
 * @param ctid context id
 * @param serviceID service id
 * @param buffer Buffer filled with injection message data
 * @param size Size of injection data within buffer
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_send_inject_msg(MctClient *client,
                                          char *apid,
                                          char *ctid,
                                          uint32_t serviceID,
                                          uint8_t *buffer,
                                          uint32_t size);
/**
 * Send an set  log level message to the mct daemon
 * @param client pointer to mct client structure
 * @param apid application id
 * @param ctid context id
 * @param logLevel Log Level
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_send_log_level(MctClient *client, char *apid, char *ctid, uint8_t logLevel);
/**
 * Send an request to get log info message to the mct daemon
 * @param client pointer to mct client structure
 * @return negative value if there was an error
 */
int mct_client_get_log_info(MctClient *client);
/**
 * Send an request to get default log level to the mct daemon
 * @param client pointer to mct client structure
 * @return negative value if there was an error
 */
MctReturnValue mct_client_get_default_log_level(MctClient *client);
/**
 * Send an request to get software version to the mct daemon
 * @param client pointer to mct client structure
 * @return negative value if there was an error
 */
int mct_client_get_software_version(MctClient *client);
/**
 * Initialise get log info structure
 * @return void
 */
void mct_getloginfo_init(void);
/**
 * To free the memory allocated for app description in get log info
 * @return void
 */
void mct_getloginfo_free(void);
/**
 * Send a set trace status message to the mct daemon
 * @param client pointer to mct client structure
 * @param apid application id
 * @param ctid context id
 * @param traceStatus Default Trace Status
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_send_trace_status(MctClient *client, char *apid, char *ctid, uint8_t traceStatus);
/**
 * Send the default log level to the mct daemon
 * @param client pointer to mct client structure
 * @param defaultLogLevel Default Log Level
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_send_default_log_level(MctClient *client, uint8_t defaultLogLevel);
/**
 * Send the log level to all contexts registered with mct daemon
 * @param client pointer to mct client structure
 * @param LogLevel Log Level to be set
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_send_all_log_level(MctClient *client, uint8_t LogLevel);
/**
 * Send the default trace status to the mct daemon
 * @param client pointer to mct client structure
 * @param defaultTraceStatus Default Trace Status
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_send_default_trace_status(MctClient *client, uint8_t defaultTraceStatus);
/**
 * Send the trace status to all contexts registered with mct daemon
 * @param client pointer to mct client structure
 * @param traceStatus trace status to be set
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_send_all_trace_status(MctClient *client, uint8_t traceStatus);
/**
 * Send the timing pakets status to the mct daemon
 * @param client pointer to mct client structure
 * @param timingPakets Timing pakets enabled
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_send_timing_pakets(MctClient *client, uint8_t timingPakets);
/**
 * Send the store config command to the mct daemon
 * @param client pointer to mct client structure
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_send_store_config(MctClient *client);
/**
 * Send the reset to factory default command to the mct daemon
 * @param client pointer to mct client structure
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_send_reset_to_factory_default(MctClient *client);

/**
 * Set baudrate within mct client structure
 * @param client pointer to mct client structure
 * @param baudrate Baudrate
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_setbaudrate(MctClient *client, int baudrate);

/**
 * Send set Blockmode command to the mct daemon
 * @param client pointer to mct client structure
 * @param block_mode Blockmode state (1 = BLOCKING/0 = NONBLOCKING)
 * @return negative value if there was an error
 */
int mct_client_send_set_blockmode(MctClient *client, int block_mode);
/**
 * Send get Blockmode command to the mct daemon
 * @param client pointer to mct client structure
 * @return negative value if there was an error
 */
int mct_client_send_get_blockmode(MctClient *client);
/**
 * Set mode within mct client structure
 * @param client pointer to mct client structure
 * @param mode MctClientMode
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_set_mode(MctClient *client, MctClientMode mode);

/**
 * Set server ip
 * @param client pointer to mct client structure
 * @param ipaddr pointer to command line argument
 * @return negative value if there was an error
 */
int mct_client_set_server_ip(MctClient *client, char *ipaddr);

/**
 * Set server UDP host receiver interface address
 * @param client pointer to mct client structure
 * @param hostip pointer to multicast group address
 * @return negative value if there was an error
 */
int mct_client_set_host_if_address(MctClient *client, char *hostip);

/**
 * Set serial device
 * @param client pointer to mct client structure
 * @param serial_device pointer to command line argument
 * @return negative value if there was an error
 */
int mct_client_set_serial_device(MctClient *client, char *serial_device);

/**
 * Set socket path
 * @param client pointer to mct client structure
 * @param socket_path pointer to socket path string
 * @return negative value if there was an error
 */
int mct_client_set_socket_path(MctClient *client, char *socket_path);

/**
 * Parse GET_LOG_INFO response text
 * @param resp      GET_LOG_INFO response
 * @param resp_text response text represented by ASCII
 * @return Value from MctReturnValue enum
 */
MctReturnValue mct_client_parse_get_log_info_resp_text(MctServiceGetLogInfoResponse *resp,
                                                       char *resp_text);

/**
 * Free memory allocated for get log info message
 * @param resp response
 * @return 0 on success, -1 otherwise
 */
int mct_client_cleanup_get_log_info(MctServiceGetLogInfoResponse *resp);
#   ifdef __cplusplus
}
#   endif

/**
 \}
 */

#endif /* MCT_CLIENT_H */
