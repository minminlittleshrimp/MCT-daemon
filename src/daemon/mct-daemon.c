#include <netdb.h>
#include <ctype.h>
#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), (), and recv() */
#include <sys/un.h>
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>
#include <grp.h>

#ifdef linux
#include <sys/timerfd.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>
#if defined(linux) && defined(__NR_statx)
#include <linux/stat.h>
#endif

#include "mct_types.h"
#include "mct-daemon.h"
#include "mct-daemon_cfg.h"
#include "mct_daemon_common_cfg.h"

#include "mct_daemon_socket.h"
#include "mct_daemon_unix_socket.h"
#include "mct_daemon_serial.h"

#include "mct_daemon_client.h"
#include "mct_daemon_connection.h"
#include "mct_daemon_event_handler.h"
#include "mct_daemon_offline_logstorage.h"
#include "mct_daemon_filter.h"

/**
 * \defgroup daemon MCT Daemon
 * \addtogroup daemon
 * \{
 */

static int mct_daemon_log_internal(MctDaemon *daemon,
                                   MctDaemonLocal *daemon_local,
                                   char *str,
                                   int verbose);

static int mct_daemon_check_numeric_setting(char *token,
                                            char *value,
                                            unsigned long *data);

/* used in main event loop and signal handler */
int g_exit = 0;

int g_signo = 0;

/* used for value from conf file */
static int value_length = 1024;

static char mct_timer_conn_types[MCT_TIMER_UNKNOWN + 1] = {
    [MCT_TIMER_PACKET] = MCT_CONNECTION_ONE_S_TIMER,
    [MCT_TIMER_ECU] = MCT_CONNECTION_SIXTY_S_TIMER,
    [MCT_TIMER_GATEWAY] = MCT_CONNECTION_GATEWAY_TIMER,
    [MCT_TIMER_UNKNOWN] = MCT_CONNECTION_TYPE_MAX
};

static char mct_timer_names[MCT_TIMER_UNKNOWN + 1][32] = {
    [MCT_TIMER_PACKET] = "Timing packet",
    [MCT_TIMER_ECU] = "ECU version",
    [MCT_TIMER_GATEWAY] = "Gateway",
    [MCT_TIMER_UNKNOWN] = "Unknown timer"
};
/**
 * Print usage information of tool.
 */
void usage()
{
    char version[MCT_DAEMON_TEXTBUFSIZE];
    mct_get_version(version, MCT_DAEMON_TEXTBUFSIZE);
    printf("%s", version);
    printf("Usage: mct-daemon [options]\n");
    printf("Options:\n");
    printf("  -d            Daemonize\n");
    printf("  -h            Usage\n");
    printf(
        "  -c filename   MCT daemon configuration file (Default: " CONFIGURATION_FILES_DIR
        "/mct.conf)\n");

#ifdef MCT_DAEMON_USE_FIFO_IPC
    printf("  -t directory  Directory for local fifo and user-pipes (Default: /tmp)\n");
    printf("                (Applications wanting to connect to a daemon using a\n");
    printf("                custom directory need to be started with the environment \n");
    printf("                variable MCT_PIPE_DIR set appropriately)\n");
#endif
    printf("  -p port       port to monitor for incoming requests (Default: 3490)\n");
    printf("                (Applications wanting to connect to a daemon using a custom\n");
    printf("                port need to be started with the environment variable\n");
    printf("                MCT_DAEMON_TCP_PORT set appropriately)\n");
} /* usage() */

/**
 * Option handling
 */
int option_handling(MctDaemonLocal *daemon_local, int argc, char *argv[])
{
    int c;

    if (daemon_local == 0) {
        fprintf (stderr, "Invalid parameter passed to option_handling()\n");
        return -1;
    }

    /* Initialize flags */
    memset(daemon_local, 0, sizeof(MctDaemonLocal));

    /* default values */
    daemon_local->flags.port = MCT_DAEMON_TCP_PORT;

#ifdef MCT_DAEMON_USE_FIFO_IPC
    mct_log_set_fifo_basedir(MCT_USER_IPC_PATH);
#endif
    opterr = 0;
    while ((c = getopt (argc, argv, "hdc:t:p:")) != -1)
        switch (c) {
            case 'd':
            {
                daemon_local->flags.dflag = 1;
                break;
            }
            case 'c':
            {
                strncpy(daemon_local->flags.cvalue, optarg, NAME_MAX);
                break;
            }
#ifdef MCT_DAEMON_USE_FIFO_IPC
            case 't':
            {
                mct_log_set_fifo_basedir(optarg);
                break;
            }
#endif
            case 'p':
            {
                daemon_local->flags.port = atoi(optarg);

                if (daemon_local->flags.port == 0) {
                    fprintf (stderr, "Invalid port `%s' specified.\n", optarg);
                    return -1;
                }

                break;
            }
            case 'h':
            {
                usage();
                return -2; /* return no error */
            }
            case '?':
            {
                if ((optopt == 'c') || (optopt == 't') || (optopt == 'p')) {
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                } else if (isprint (optopt)) {
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                } else {
                    fprintf (stderr, "Unknown option character `\\x%x'.\n", (uint32_t)optopt);
                }

                /* unknown or wrong option used, show usage information and terminate */
                usage();
                return -1;
            }
            default:
            {
                fprintf (stderr, "Invalid option, this should never occur!\n");
                return -1;
            }
        }

    /* switch() */

#ifdef MCT_DAEMON_USE_FIFO_IPC
    snprintf(daemon_local->flags.userPipesDir, MCT_PATH_MAX,
             "%s/mctpipes", mctFifoBaseDir);
    snprintf(daemon_local->flags.daemonFifoName, MCT_PATH_MAX,
             "%s/mct", mctFifoBaseDir);
#endif

    return 0;
}  /* option_handling() */

/**
 * Option file parser
 */
int option_file_parser(MctDaemonLocal *daemon_local)
{
    FILE *pFile = NULL;
    char line[value_length - 1];
    char token[value_length];
    char value[value_length];
    char *pch = NULL;
    const char *filename = NULL;

    /* set default values for configuration */
    daemon_local->flags.sharedMemorySize = 0;
    daemon_local->flags.sendMessageTime = 0;
    daemon_local->flags.offlineTraceDirectory[0] = 0;
    daemon_local->flags.offlineTraceFileSize = 1000000;
    daemon_local->flags.offlineTraceMaxSize = 4000000;
    daemon_local->flags.offlineTraceFilenameTimestampBased = 1;
    daemon_local->flags.loggingMode = MCT_LOG_TO_CONSOLE;
    daemon_local->flags.loggingLevel = LOG_INFO;

    ssize_t n;

#ifdef MCT_DAEMON_USE_UNIX_SOCKET_IPC
    n = snprintf(daemon_local->flags.loggingFilename,
                 sizeof(daemon_local->flags.loggingFilename),
                 "%s/mct.log", MCT_USER_IPC_PATH);
#else /* MCT_DAEMON_USE_FIFO_IPC */
    n = snprintf(daemon_local->flags.loggingFilename,
                 sizeof(daemon_local->flags.loggingFilename),
                 "%s/mct.log", mctFifoBaseDir);
#endif

    if ((n < 0) || ((size_t)n > sizeof(daemon_local->flags.loggingFilename))) {
        mct_vlog(LOG_WARNING, "%s: snprintf truncation/error(%ld) %s\n",
                 __func__, n, daemon_local->flags.loggingFilename);
    }

    daemon_local->timeoutOnSend = 4;
    daemon_local->RingbufferMinSize = MCT_DAEMON_RINGBUFFER_MIN_SIZE;
    daemon_local->RingbufferMaxSize = MCT_DAEMON_RINGBUFFER_MAX_SIZE;
    daemon_local->RingbufferStepSize = MCT_DAEMON_RINGBUFFER_STEP_SIZE;
    daemon_local->daemonFifoSize = 0;
    daemon_local->flags.sendECUSoftwareVersion = 0;
    memset(daemon_local->flags.pathToECUSoftwareVersion, 0,
           sizeof(daemon_local->flags.pathToECUSoftwareVersion));
    daemon_local->flags.sendTimezone = 0;
    daemon_local->flags.offlineLogstorageMaxDevices = 0;
    daemon_local->flags.offlineLogstorageDirPath[0] = 0;
    daemon_local->flags.offlineLogstorageTimestamp = 1;
    daemon_local->flags.offlineLogstorageDelimiter = '_';
    daemon_local->flags.offlineLogstorageMaxCounter = UINT_MAX;
    daemon_local->flags.offlineLogstorageMaxCounterIdx = 0;
    daemon_local->flags.offlineLogstorageOptionalCounter = false;
    daemon_local->flags.blockModeAllowed = MCT_DAEMON_BLOCK_MODE_DISABLED;
    daemon_local->flags.offlineLogstorageCacheSize = 30000; /* 30MB */
    mct_daemon_logstorage_set_logstorage_cache_size(
        daemon_local->flags.offlineLogstorageCacheSize);
    strncpy(daemon_local->flags.ctrlSockPath,
            MCT_DAEMON_DEFAULT_CTRL_SOCK_PATH,
            sizeof(daemon_local->flags.ctrlSockPath));
    strncpy(daemon_local->flags.msgFilterConfFile,
            MCT_FILTER_CONFIG_FILE,
            MCT_DAEMON_FLAG_MAX - 1);
#ifdef MCT_DAEMON_USE_UNIX_SOCKET_IPC
    snprintf(daemon_local->flags.appSockPath, MCT_IPC_PATH_MAX, "%s/mct", MCT_USER_IPC_PATH);

    if (strlen(MCT_USER_IPC_PATH) > MCT_IPC_PATH_MAX) {
        fprintf(stderr, "Provided path too long...trimming it to path[%s]\n",
                daemon_local->flags.appSockPath);
    }

#else /* MCT_DAEMON_USE_FIFO_IPC */
    memset(daemon_local->flags.daemonFifoGroup, 0, sizeof(daemon_local->flags.daemonFifoGroup));
#endif
    daemon_local->flags.autoResponseGetLogInfoOption = 7;
    daemon_local->flags.contextLogLevel = MCT_LOG_INFO;
    daemon_local->flags.contextTraceStatus = MCT_TRACE_STATUS_OFF;
    daemon_local->flags.enforceContextLLAndTS = 0; /* default is off */
    daemon_local->flags.ipNodes = NULL;
    daemon_local->flags.injectionMode = 1;

    /* open configuration file */
    if (daemon_local->flags.cvalue[0]) {
        filename = daemon_local->flags.cvalue;
    } else {
        filename = CONFIGURATION_FILES_DIR "/mct.conf";
    }

    /*printf("Load configuration from file: %s\n",filename); */
    pFile = fopen (filename, "r");

    if (pFile != NULL) {
        while (1) {
            /* fetch line from configuration file */
            if (fgets (line, value_length - 1, pFile) != NULL) {
                pch = strtok (line, " =\r\n");
                token[0] = 0;
                value[0] = 0;

                while (pch != NULL) {
                    if (strcmp(pch, "#") == 0) {
                        break;
                    }

                    if (token[0] == 0) {
                        strncpy(token, pch, sizeof(token) - 1);
                        token[sizeof(token) - 1] = 0;
                    } else {
                        strncpy(value, pch, sizeof(value) - 1);
                        value[sizeof(value) - 1] = 0;
                        break;
                    }

                    pch = strtok (NULL, " =\r\n");
                }

                if (token[0] && value[0]) {
                    /* parse arguments here */
                    if (strcmp(token, "Verbose") == 0) {
                        daemon_local->flags.vflag = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "PrintASCII") == 0) {
                        daemon_local->flags.aflag = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "PrintHex") == 0) {
                        daemon_local->flags.xflag = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "PrintHeadersOnly") == 0) {
                        daemon_local->flags.sflag = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "SendSerialHeader") == 0) {
                        daemon_local->flags.lflag = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "SendContextRegistration") == 0) {
                        daemon_local->flags.rflag = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "SendContextRegistrationOption") == 0) {
                        daemon_local->flags.autoResponseGetLogInfoOption = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "SendMessageTime") == 0) {
                        daemon_local->flags.sendMessageTime = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "RS232SyncSerialHeader") == 0) {
                        daemon_local->flags.mflag = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "TCPSyncSerialHeader") == 0) {
                        daemon_local->flags.nflag = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "RS232DeviceName") == 0) {
                        strncpy(daemon_local->flags.yvalue, value, NAME_MAX);
                        daemon_local->flags.yvalue[NAME_MAX] = 0;
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "RS232Baudrate") == 0) {
                        strncpy(daemon_local->flags.bvalue, value, NAME_MAX);
                        daemon_local->flags.bvalue[NAME_MAX] = 0;
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "ECUId") == 0) {
                        strncpy(daemon_local->flags.evalue, value, NAME_MAX);
                        daemon_local->flags.evalue[NAME_MAX] = 0;
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "PersistanceStoragePath") == 0) {
                        strncpy(daemon_local->flags.ivalue, value, NAME_MAX);
                        daemon_local->flags.ivalue[NAME_MAX] = 0;
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "LoggingMode") == 0) {
                        daemon_local->flags.loggingMode = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "LoggingLevel") == 0) {
                        daemon_local->flags.loggingLevel = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "LoggingFilename") == 0) {
                        strncpy(daemon_local->flags.loggingFilename,
                                value,
                                sizeof(daemon_local->flags.loggingFilename) - 1);
                        daemon_local->flags.loggingFilename[sizeof(daemon_local->flags.
                                                                   loggingFilename) - 1] = 0;
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "TimeOutOnSend") == 0) {
                        daemon_local->timeoutOnSend = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "RingbufferMinSize") == 0) {
                        if (mct_daemon_check_numeric_setting(token,
                                value, &(daemon_local->RingbufferMinSize)) < 0) {
                            fclose(pFile);
                            return -1;
                       }
                    } else if (strcmp(token, "RingbufferMaxSize") == 0) {
                        if (mct_daemon_check_numeric_setting(token,
                                value, &(daemon_local->RingbufferMaxSize)) < 0) {
                            fclose(pFile);
                            return -1;
                       }
                    } else if (strcmp(token, "RingbufferStepSize") == 0) {
                        if (mct_daemon_check_numeric_setting(token,
                                value, &(daemon_local->RingbufferStepSize)) < 0) {
                            fclose(pFile);
                            return -1;
                       }
                    } else if (strcmp(token, "SharedMemorySize") == 0) {
                        daemon_local->flags.sharedMemorySize = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "OfflineTraceDirectory") == 0) {
                        strncpy(daemon_local->flags.offlineTraceDirectory, value,
                                sizeof(daemon_local->flags.offlineTraceDirectory) - 1);
                        daemon_local->flags.offlineTraceDirectory[sizeof(daemon_local->flags.
                                                                         offlineTraceDirectory) -
                                                                  1] = 0;
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "OfflineTraceFileSize") == 0) {
                        daemon_local->flags.offlineTraceFileSize = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "OfflineTraceMaxSize") == 0) {
                        daemon_local->flags.offlineTraceMaxSize = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "OfflineTraceFileNameTimestampBased") == 0) {
                        daemon_local->flags.offlineTraceFilenameTimestampBased = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "SendECUSoftwareVersion") == 0) {
                        daemon_local->flags.sendECUSoftwareVersion = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "PathToECUSoftwareVersion") == 0) {
                        strncpy(daemon_local->flags.pathToECUSoftwareVersion, value,
                                sizeof(daemon_local->flags.pathToECUSoftwareVersion) - 1);
                        daemon_local->flags.pathToECUSoftwareVersion[sizeof(daemon_local->flags.
                                                                            pathToECUSoftwareVersion)
                                                                     - 1] = 0;
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "SendTimezone") == 0) {
                        daemon_local->flags.sendTimezone = atoi(value);
                        /*printf("Option: %s=%s\n",token,value); */
                    } else if (strcmp(token, "OfflineLogstorageMaxDevices") == 0) {
                        daemon_local->flags.offlineLogstorageMaxDevices = atoi(value);
                    } else if (strcmp(token, "OfflineLogstorageDirPath") == 0) {
                        strncpy(daemon_local->flags.offlineLogstorageDirPath,
                                value,
                                sizeof(daemon_local->flags.offlineLogstorageDirPath) - 1);
                    } else if (strcmp(token, "OfflineLogstorageTimestamp") == 0) {
                        /* Check if set to 0, default otherwise */
                        if (atoi(value) == 0) {
                            daemon_local->flags.offlineLogstorageTimestamp = 0;
                        }
                    } else if (strcmp(token, "OfflineLogstorageDelimiter") == 0) {
                        /* Check if valid punctuation, default otherwise*/
                        if (ispunct((char)value[0])) {
                            daemon_local->flags.offlineLogstorageDelimiter = (char)value[0];
                        }
                    } else if (strcmp(token, "OfflineLogstorageMaxCounter") == 0) {
                        daemon_local->flags.offlineLogstorageMaxCounter = atoi(value);
                        daemon_local->flags.offlineLogstorageMaxCounterIdx = strlen(value);
                    } else if (strcmp(token, "OfflineLogstorageOptionalIndex") == 0) {
                        daemon_local->flags.offlineLogstorageOptionalCounter = atoi(value);
                    } else if (strcmp(token, "OfflineLogstorageCacheSize") == 0) {
                        daemon_local->flags.offlineLogstorageCacheSize =
                            (unsigned int)atoi(value);
                        mct_daemon_logstorage_set_logstorage_cache_size(
                            daemon_local->flags.offlineLogstorageCacheSize);
                    } else if (strcmp(token, "ControlSocketPath") == 0) {
                        memset(
                            daemon_local->flags.ctrlSockPath,
                            0,
                            MCT_DAEMON_FLAG_MAX);
                        strncpy(
                            daemon_local->flags.ctrlSockPath,
                            value,
                            MCT_DAEMON_FLAG_MAX - 1);
                    } else if (strcmp(token, "MessageFilterConfigFile") == 0) {
                        memset(daemon_local->flags.msgFilterConfFile,
                               0, MCT_DAEMON_FLAG_MAX);
                        strncpy(daemon_local->flags.msgFilterConfFile,
                                value, MCT_DAEMON_FLAG_MAX - 1);
                    } else if (strcmp(token, "ContextLogLevel") == 0) {
                        int const intval = atoi(value);

                        if ((intval >= MCT_LOG_OFF) && (intval <= MCT_LOG_VERBOSE)) {
                            daemon_local->flags.contextLogLevel = intval;
                            printf("Option: %s=%s\n", token, value);
                        } else {
                            fprintf(
                                stderr,
                                "Invalid value for ContextLogLevel: %i. Must be in range [%i..%i]\n",
                                intval,
                                MCT_LOG_OFF,
                                MCT_LOG_VERBOSE);
                        }
                    } else if (strcmp(token, "ContextTraceStatus") == 0) {
                        int const intval = atoi(value);

                        if ((intval >= MCT_TRACE_STATUS_OFF) && (intval <= MCT_TRACE_STATUS_ON)) {
                            daemon_local->flags.contextTraceStatus = intval;
                            printf("Option: %s=%s\n", token, value);
                        } else {
                            fprintf(
                                stderr,
                                "Invalid value for ContextTraceStatus: %i. Must be in range [%i..%i]\n",
                                intval,
                                MCT_TRACE_STATUS_OFF,
                                MCT_TRACE_STATUS_ON);
                        }
                    } else if (strcmp(token, "ForceContextLogLevelAndTraceStatus") == 0) {
                        int const intval = atoi(value);

                        if ((intval >= 0) && (intval <= 1)) {
                            daemon_local->flags.enforceContextLLAndTS = intval;
                            printf("Option: %s=%s\n", token, value);
                        } else {
                            fprintf(
                                stderr,
                                "Invalid value for ForceContextLogLevelAndTraceStatus: %i. Must be 0, 1\n",
                                intval);
                        }
                    }

#ifdef MCT_DAEMON_USE_FIFO_IPC
                    else if (strcmp(token, "DaemonFIFOSize") == 0) {
                        if (mct_daemon_check_numeric_setting(token,
                                value, &(daemon_local->daemonFifoSize)) < 0) {
                            fclose(pFile);
                            return -1;
                        }
#ifndef __linux__
                            printf("Option DaemonFIFOSize is set but only supported on Linux. Ignored.\n");
#endif
                    } else if (strcmp(token, "DaemonFifoGroup") == 0) {
                        strncpy(daemon_local->flags.daemonFifoGroup, value, NAME_MAX);
                        daemon_local->flags.daemonFifoGroup[NAME_MAX] = 0;
                    }
#endif
                    else if (strcmp(token, "BindAddress") == 0) {
                        MctBindAddress_t *newNode = NULL;
                        MctBindAddress_t *temp = NULL;

                        char *tok = strtok(value, ",;");

                        if (tok != NULL) {
                            daemon_local->flags.ipNodes = calloc(1, sizeof(MctBindAddress_t));

                            if (daemon_local->flags.ipNodes == NULL) {
                                mct_vlog(LOG_ERR, "Could not allocate for IP list\n");
                                fclose(pFile);
                                return -1;
                            } else {
                                strncpy(daemon_local->flags.ipNodes->ip,
                                        tok,
                                        sizeof(daemon_local->flags.ipNodes->ip) - 1);
                                daemon_local->flags.ipNodes->next = NULL;
                                temp = daemon_local->flags.ipNodes;

                                tok = strtok(NULL, ",;");

                                while (tok != NULL) {
                                    newNode = calloc(1, sizeof(MctBindAddress_t));

                                    if (newNode == NULL) {
                                        mct_vlog(LOG_ERR, "Could not allocate for IP list\n");
                                        fclose(pFile);
                                        return -1;
                                    } else {
                                        strncpy(newNode->ip, tok, sizeof(newNode->ip) - 1);
                                    }

                                    temp->next = newNode;
                                    temp = temp->next;
                                    tok = strtok(NULL, ",;");
                                }
                            }
                        } else {
                            mct_vlog(LOG_WARNING, "BindAddress option is empty\n");
                        }
                    } else if (strcmp(token, "AllowBlockMode") == 0) {
                        int mode = atoi(value);

                        if ((mode < MCT_DAEMON_BLOCK_MODE_DISABLED) ||
                            (mode > MCT_DAEMON_BLOCK_MODE_ENABLED)) {
                            fprintf(stderr,
                                    "Invalid value for AllowBlockMode: %i."
                                    " Must be 0, 1. Set to NON-BLOCKING\n",
                                    mode);
                            daemon_local->flags.blockModeAllowed =
                                MCT_DAEMON_BLOCK_MODE_DISABLED;
                        } else {
                            daemon_local->flags.blockModeAllowed = mode;
                        }
                    } else if (strcmp(token, "InjectionMode") == 0) {
                        daemon_local->flags.injectionMode = atoi(value);
                    } else {
                        fprintf(stderr, "Unknown option: %s=%s\n", token, value);
                    }
                }
            } else {
                break;
            }
        }

        fclose (pFile);
    } else {
        fprintf(stderr, "Cannot open configuration file: %s\n", filename);
    }

    return 0;
}

static int mct_mkdir_recursive(const char *dir)
{
    int ret = 0;
    char tmp[PATH_MAX + 1];
    char *p = NULL;
    char *end = NULL;
    size_t len;

    strncpy(tmp, dir, PATH_MAX);
    len = strlen(tmp);

    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    end = tmp + len;

    for (p = tmp + 1; ((*p) && (ret == 0)) || ((ret == -1 && errno == EEXIST) && (p != end));
         p++) {
        if (*p == '/') {
            *p = 0;
            ret = mkdir(tmp,
            #ifdef MCT_DAEMON_USE_FIFO_IPC
                        S_IRWXU);
            #else
                        S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IROTH  | S_IWOTH /*S_IRWXU*/);
            #endif
            *p = '/';
        }
    }

    if ((ret == 0) || ((ret == -1) && (errno == EEXIST))) {
        ret = mkdir(tmp,
        #ifdef MCT_DAEMON_USE_FIFO_IPC
                    S_IRWXU);
        #else
                    S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IROTH  | S_IWOTH /*S_IRWXU*/);
        #endif
    }

    if ((ret == -1) && (errno == EEXIST)) {
        ret = 0;
    }

    return ret;
}

#ifdef MCT_DAEMON_USE_FIFO_IPC
static MctReturnValue mct_daemon_create_pipes_dir(char *dir)
{
    int ret = MCT_RETURN_OK;

    if (dir == NULL) {
        mct_vlog(LOG_ERR, "%s: Invalid parameter\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* create mct pipes directory */
    ret = mkdir(dir,
                S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH | S_ISVTX);

    if ((ret == -1) && (errno != EEXIST)) {
        mct_vlog(LOG_ERR,
                 "FIFO user dir %s cannot be created (%s)!\n",
                 dir,
                 strerror(errno));

        return MCT_RETURN_ERROR;
    }

    /* S_ISGID cannot be set by mkdir, let's reassign right bits */
    ret = chmod(
            dir,
            S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH |
            S_IXOTH | S_ISGID |
            S_ISVTX);

    if (ret == -1) {
        mct_vlog(LOG_ERR,
                 "FIFO user dir %s cannot be chmoded (%s)!\n",
                 dir,
                 strerror(errno));

        return MCT_RETURN_ERROR;
    }

    return ret;
}
#endif

/**
 * Main function of tool.
 */
int main(int argc, char *argv[])
{
    char version[MCT_DAEMON_TEXTBUFSIZE];
    char local_str[MCT_DAEMON_TEXTBUFSIZE];
    MctDaemonLocal daemon_local;
    MctDaemon daemon;
    int back = 0;

    memset(&daemon_local, 0, sizeof(MctDaemonLocal));
    memset(&daemon, 0, sizeof(MctDaemon));

    /* Command line option handling */
    if ((back = option_handling(&daemon_local, argc, argv)) < 0) {
        if (back != -2) {
            fprintf (stderr, "option_handling() failed!\n");
        }

        return -1;
    }

    /* Configuration file option handling */
    if ((back = option_file_parser(&daemon_local)) < 0) {
        if (back != -2) {
            fprintf (stderr, "option_file_parser() failed!\n");
        }

        return -1;
    }

    /* Initialize internal logging facility */
    mct_log_set_filename(daemon_local.flags.loggingFilename);
    mct_log_set_level(daemon_local.flags.loggingLevel);
    mct_log_init(daemon_local.flags.loggingMode);

    /* Print version information */
    mct_get_version(version, MCT_DAEMON_TEXTBUFSIZE);

    mct_vlog(LOG_NOTICE, "Starting MCT Daemon; %s\n", version);

    PRINT_FUNCTION_VERBOSE(daemon_local.flags.vflag);

/* Make sure the parent user directory is created */
#ifdef MCT_DAEMON_USE_FIFO_IPC

    if (mct_mkdir_recursive(mctFifoBaseDir) != 0) {
        mct_vlog(LOG_ERR, "Base dir %s cannot be created!\n", mctFifoBaseDir);
        return -1;
    }

#else
    if (mct_mkdir_recursive(MCT_USER_IPC_PATH) != 0) {
        mct_vlog(LOG_ERR, "Base dir %s cannot be created!\n", daemon_local.flags.appSockPath);
        return -1;
    }

#endif

    /* --- Daemon init phase 1 begin --- */
    if (mct_daemon_local_init_p1(&daemon, &daemon_local, daemon_local.flags.vflag) == -1) {
        mct_log(LOG_CRIT, "Initialization of phase 1 failed!\n");
        return -1;
    }

    /* --- Daemon init phase 1 end --- */

    if (mct_daemon_prepare_event_handling(&daemon_local.pEvent)) {
        /* TODO: Perform clean-up */
        mct_log(LOG_CRIT, "Initialization of event handling failed!\n");
        return -1;
    }

    if (mct_daemon_prepare_message_filter(&daemon_local,
                                          daemon_local.flags.vflag) == -1) {
        mct_log(LOG_CRIT, "Initialization of message filter failed!\n");
        mct_daemon_local_cleanup(&daemon, &daemon_local, daemon_local.flags.vflag);
        mct_daemon_free(&daemon, daemon_local.flags.vflag);
        return -1;
    }

    /* --- Daemon connection init begin */
    if (mct_daemon_local_connection_init(&daemon, &daemon_local, daemon_local.flags.vflag) == -1) {
        mct_log(LOG_CRIT, "Initialization of local connections failed!\n");
        return -1;
    }

    /* --- Daemon connection init end */

    if (mct_daemon_init_runtime_configuration(&daemon, daemon_local.flags.ivalue,
                                              daemon_local.flags.vflag) == -1) {
        mct_log(LOG_ERR, "Could not load runtime config\n");
        return -1;
    }

    /*
     * Load mct-runtime.cfg if available.
     * This must be loaded before offline setup
     */
    mct_daemon_configuration_load(&daemon, daemon.runtime_configuration, daemon_local.flags.vflag);

    /* --- Daemon init phase 2 begin --- */
    if (mct_daemon_local_init_p2(&daemon, &daemon_local, daemon_local.flags.vflag) == -1) {
        mct_log(LOG_CRIT, "Initialization of phase 2 failed!\n");
        return -1;
    }

    /* --- Daemon init phase 2 end --- */

    if (daemon_local.flags.offlineLogstorageDirPath[0]) {
        if (mct_daemon_logstorage_setup_internal_storage(
                &daemon,
                &daemon_local,
                daemon_local.flags.offlineLogstorageDirPath,
                daemon_local.flags.blockModeAllowed,
                daemon_local.flags.vflag) == -1) {
            mct_log(LOG_INFO,
                    "Setting up internal offline log storage failed!\n");
        }

        daemon_local.internal_client_connections++;
    }

    /* create fd for timer timing packets */
    create_timer_fd(&daemon_local, 1, 1, MCT_TIMER_PACKET);

    /* create fd for timer ecu version */
    if ((daemon_local.flags.sendECUSoftwareVersion > 0) ||
        (daemon_local.flags.sendTimezone > 0)) {
        create_timer_fd(&daemon_local, 60, 60, MCT_TIMER_ECU);
    }

    /* For offline tracing we still can use the same states */
    /* as for socket sending. Using this trick we see the traces */
    /* In the offline trace AND in the socket stream. */
    if (daemon_local.flags.yvalue[0]) {
        mct_daemon_change_state(&daemon, MCT_DAEMON_STATE_SEND_DIRECT);
    } else {
        mct_daemon_change_state(&daemon, MCT_DAEMON_STATE_BUFFER);
    }

    /*
     * Check for app and ctx runtime cfg.
     * These cfg must be loaded after ecuId and num_user_lists are available
     */
    if ((mct_daemon_applications_load(&daemon, daemon.runtime_application_cfg,
                                      daemon_local.flags.vflag) == 0) &&
        (mct_daemon_contexts_load(&daemon, daemon.runtime_context_cfg,
                                  daemon_local.flags.vflag) == 0)) {
        daemon.runtime_context_cfg_loaded = 1;
    }

    mct_daemon_log_internal(&daemon,
                            &daemon_local,
                            "Daemon launched. Starting to output traces...",
                            daemon_local.flags.vflag);

    /* Even handling loop. */
    while ((back >= 0) && (g_exit >= 0))
        back = mct_daemon_handle_event(&daemon_local.pEvent,
                                       &daemon,
                                       &daemon_local);

    snprintf(local_str, MCT_DAEMON_TEXTBUFSIZE, "Exiting MCT daemon... [%d]",
             g_signo);
    mct_daemon_log_internal(&daemon, &daemon_local, local_str,
                            daemon_local.flags.vflag);
    mct_vlog(LOG_NOTICE, "%s%s", local_str, "\n");

    mct_daemon_cleanup_message_filter(&daemon_local, daemon_local.flags.vflag);

    mct_daemon_local_cleanup(&daemon, &daemon_local, daemon_local.flags.vflag);

    mct_daemon_free(&daemon, daemon_local.flags.vflag);

    mct_log(LOG_NOTICE, "Leaving MCT daemon\n");

    return 0;
} /* main() */

int mct_daemon_local_init_p1(MctDaemon *daemon, MctDaemonLocal *daemon_local, int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);
    int ret = MCT_RETURN_OK;

    if ((daemon == 0) || (daemon_local == 0)) {
        mct_log(LOG_ERR,
                "Invalid function parameters used for function mct_daemon_local_init_p1()\n");
        return -1;
    }

#ifdef MCT_DAEMON_USE_FIFO_IPC

    if (mct_daemon_create_pipes_dir(daemon_local->flags.userPipesDir) == MCT_RETURN_ERROR) {
        return MCT_RETURN_ERROR;
    }

#endif

    /* Check for daemon mode */
    if (daemon_local->flags.dflag) {
        mct_daemon_daemonize(daemon_local->flags.vflag);
    }

    /* Re-Initialize internal logging facility after fork */
    mct_log_set_filename(daemon_local->flags.loggingFilename);
    mct_log_set_level(daemon_local->flags.loggingLevel);
    mct_log_init(daemon_local->flags.loggingMode);

    /* initialise structure to use MCT file */
    ret = mct_file_init(&(daemon_local->file), daemon_local->flags.vflag);

    if (ret == MCT_RETURN_ERROR) {
        mct_log(LOG_ERR, "Could not initialize file structure\n");
        /* Return value ignored, mct daemon will exit */
        mct_file_free(&(daemon_local->file), daemon_local->flags.vflag);
        return ret;
    }

    signal(SIGPIPE, SIG_IGN);

    signal(SIGTERM, mct_daemon_signal_handler); /* software termination signal from kill */
    signal(SIGHUP, mct_daemon_signal_handler);  /* hangup signal */
    signal(SIGQUIT, mct_daemon_signal_handler);
    signal(SIGINT, mct_daemon_signal_handler);

    return MCT_RETURN_OK;
}

int mct_daemon_local_init_p2(MctDaemon *daemon, MctDaemonLocal *daemon_local, int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == 0) || (daemon_local == 0)) {
        mct_log(LOG_ERR,
                "Invalid function parameters used for function mct_daemon_local_init_p2()\n");
        return -1;
    }

    /* Daemon data */
    if (mct_daemon_init(daemon, daemon_local->RingbufferMinSize, daemon_local->RingbufferMaxSize,
                        daemon_local->RingbufferStepSize, daemon_local->flags.ivalue,
                        daemon_local->flags.contextLogLevel,
                        daemon_local->flags.contextTraceStatus,
                        daemon_local->flags.enforceContextLLAndTS,
                        daemon_local->flags.vflag) == -1) {
        mct_log(LOG_ERR, "Could not initialize daemon data\n");
        return -1;
    }

    /* init offline trace */
    if (daemon_local->flags.offlineTraceDirectory[0]) {
        if (mct_offline_trace_init(&(daemon_local->offlineTrace),
                                   daemon_local->flags.offlineTraceDirectory,
                                   daemon_local->flags.offlineTraceFileSize,
                                   daemon_local->flags.offlineTraceMaxSize,
                                   daemon_local->flags.offlineTraceFilenameTimestampBased) == -1) {
            mct_log(LOG_ERR, "Could not initialize offline trace\n");
            return -1;
        }

        daemon_local->internal_client_connections++;
    }

    /* Init offline logstorage for MAX devices */
    if (daemon_local->flags.offlineLogstorageMaxDevices > 0) {
        daemon->storage_handle = malloc(
                sizeof(MctLogStorage) * daemon_local->flags.offlineLogstorageMaxDevices);

        if (daemon->storage_handle == NULL) {
            mct_log(LOG_ERR, "Could not initialize offline logstorage\n");
            return -1;
        }

        memset(daemon->storage_handle, 0,
               (sizeof(MctLogStorage) * daemon_local->flags.offlineLogstorageMaxDevices));
    }

    /* Set ECU id of daemon */
    if (daemon_local->flags.evalue[0]) {
        mct_set_id(daemon->ecuid, daemon_local->flags.evalue);
    } else {
        mct_set_id(daemon->ecuid, MCT_DAEMON_ECU_ID);
    }

    /* Set flag for optional sending of serial header */
    daemon->sendserialheader = daemon_local->flags.lflag;

    /* prepare main loop */
    if (mct_message_init(&(daemon_local->msg), daemon_local->flags.vflag) == MCT_RETURN_ERROR) {
        mct_log(LOG_ERR, "Could not initialize message\n");
        return -1;
    }

    /* configure sending timing packets */
    if (daemon_local->flags.sendMessageTime) {
        daemon->timingpackets = 1;
    }

    /* Get ECU version info from a file. If it fails, use mct_version as fallback. */
    if (mct_daemon_local_ecu_version_init(daemon, daemon_local, daemon_local->flags.vflag) < 0) {
        daemon->ECUVersionString = malloc(MCT_DAEMON_TEXTBUFSIZE);

        if (daemon->ECUVersionString == 0) {
            mct_log(LOG_WARNING, "Could not allocate memory for version string\n");
            return -1;
        }

        mct_get_version(daemon->ECUVersionString, MCT_DAEMON_TEXTBUFSIZE);
    }

    daemon->blockMode = MCT_MODE_NON_BLOCKING;

    /* Set to allows to maintain logstorage loglevel as default */
    daemon->maintain_logstorage_loglevel = MCT_MAINTAIN_LOGSTORAGE_LOGLEVEL_ON;

    return 0;
}

static int mct_daemon_init_serial(MctDaemonLocal *daemon_local)
{
    /* create and open serial connection from/to client */
    /* open serial connection */
    int fd = -1;

    if (daemon_local->flags.yvalue[0] == '\0') {
        return 0;
    }

    fd = open(daemon_local->flags.yvalue, O_RDWR);

    if (fd < 0) {
        mct_vlog(LOG_ERR, "Failed to open serial device %s\n",
                 daemon_local->flags.yvalue);

        daemon_local->flags.yvalue[0] = 0;
        return -1;
    }

    if (isatty(fd)) {
        int speed = MCT_DAEMON_SERIAL_DEFAULT_BAUDRATE;

        if (daemon_local->flags.bvalue[0]) {
            speed = atoi(daemon_local->flags.bvalue);
        }

        daemon_local->baudrate = mct_convert_serial_speed(speed);

        if (mct_setup_serial(fd, daemon_local->baudrate) < 0) {
            close(fd);
            daemon_local->flags.yvalue[0] = 0;

            mct_vlog(LOG_ERR, "Failed to configure serial device %s (%s) \n",
                     daemon_local->flags.yvalue, strerror(errno));

            return -1;
        }

        if (daemon_local->flags.vflag) {
            mct_log(LOG_DEBUG, "Serial init done\n");
        }
    } else {
        close(fd);
        fprintf(stderr,
                "Device is not a serial device, device = %s (%s) \n",
                daemon_local->flags.yvalue,
                strerror(errno));
        daemon_local->flags.yvalue[0] = 0;
        return -1;
    }

    return mct_connection_create(daemon_local,
                                 &daemon_local->pEvent,
                                 fd,
                                 POLLIN,
                                 MCT_CONNECTION_CLIENT_MSG_SERIAL);
}

#ifdef MCT_DAEMON_USE_FIFO_IPC
static int mct_daemon_init_fifo(MctDaemonLocal *daemon_local)
{
    int ret;
    int fd = -1;
    int fifo_size;

    /* open named pipe(FIFO) to receive MCT messages from users */
    umask(0);

    /* Try to delete existing pipe, ignore result of unlink */
    const char *tmpFifo = daemon_local->flags.daemonFifoName;
    unlink(tmpFifo);

    ret = mkfifo(tmpFifo, S_IRUSR | S_IWUSR | S_IWGRP);

    if (ret == -1) {
        mct_vlog(LOG_WARNING, "FIFO user %s cannot be created (%s)!\n",
                 tmpFifo, strerror(errno));
        return -1;
    } /* if */

    /* Set group of daemon FIFO */
    if (daemon_local->flags.daemonFifoGroup[0] != 0) {
        errno = 0;
        struct group *group_mct = getgrnam(daemon_local->flags.daemonFifoGroup);

        if (group_mct) {
            ret = chown(tmpFifo, -1, group_mct->gr_gid);

            if (ret == -1) {
                mct_vlog(LOG_ERR, "FIFO user %s cannot be chowned to group %s (%s)\n",
                         tmpFifo, daemon_local->flags.daemonFifoGroup,
                         strerror(errno));
            }
        } else if ((errno == 0) || (errno == ENOENT) || (errno == EBADF) || (errno == EPERM)) {
            mct_vlog(LOG_ERR, "Group name %s is not found (%s)\n",
                     daemon_local->flags.daemonFifoGroup,
                     strerror(errno));
        } else {
            mct_vlog(LOG_ERR, "Failed to get group id of %s (%s)\n",
                     daemon_local->flags.daemonFifoGroup,
                     strerror(errno));
        }
    }

    fd = open(tmpFifo, O_RDWR);

    if (fd == -1) {
        mct_vlog(LOG_WARNING, "FIFO user %s cannot be opened (%s)!\n",
                 tmpFifo, strerror(errno));
        return -1;
    } /* if */

#ifdef __linux__
    /* F_SETPIPE_SZ and F_GETPIPE_SZ are only supported for Linux.
     * For other OSes it depends on its system e.g. pipe manager.
     */
    if (daemon_local->daemonFifoSize != 0) {
        /* Set Daemon FIFO size */
        if (fcntl(fd, F_SETPIPE_SZ, daemon_local->daemonFifoSize) == -1) {
            mct_vlog(LOG_ERR, "set FIFO size error: %s\n", strerror(errno));
        }
    }

    /* Get Daemon FIFO size */
    if ((fifo_size = fcntl(fd, F_GETPIPE_SZ, 0)) == -1) {
        mct_vlog(LOG_ERR, "get FIFO size error: %s\n", strerror(errno));
    } else {
        mct_vlog(LOG_INFO, "FIFO size: %d\n", fifo_size);
    }
#endif

    /* Early init, to be able to catch client (app) connections
     * as soon as possible. This registration is automatically ignored
     * during next execution.
     */
    return mct_connection_create(daemon_local,
                                 &daemon_local->pEvent,
                                 fd,
                                 POLLIN,
                                 MCT_CONNECTION_APP_MSG);
}
#endif

#ifdef MCT_DAEMON_USE_UNIX_SOCKET_IPC
static MctReturnValue mct_daemon_init_app_socket(MctDaemonLocal *daemon_local)
{
    /* socket access permission set to srw-rw-rw- (666) */
    int mask = S_IXUSR | S_IXGRP | S_IXOTH;
    MctReturnValue ret = MCT_RETURN_OK;
    int fd = -1;

    if (daemon_local == NULL) {
        mct_vlog(LOG_ERR, "%s: Invalid function parameters\n", __func__);
        return MCT_RETURN_ERROR;
    }
    ret = mct_daemon_unix_socket_open(&fd,
                                      daemon_local->flags.appSockPath,
                                      SOCK_STREAM,
                                      mask);
    if (ret == MCT_RETURN_OK) {
        if (mct_connection_create(daemon_local,
                                  &daemon_local->pEvent,
                                  fd,
                                  POLLIN,
                                  MCT_CONNECTION_APP_CONNECT)) {
            mct_log(LOG_CRIT, "Could not create connection for app socket.\n");
            return MCT_RETURN_ERROR;
        }
    }
    else {
        mct_log(LOG_CRIT, "Could not create and open app socket.\n");
        return MCT_RETURN_ERROR;
    }

    return ret;
}
#endif

static MctReturnValue mct_daemon_initialize_control_socket(MctDaemonLocal *daemon_local)
{
    /* socket access permission set to srw-rw---- (660)  */
    int mask = S_IXUSR | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH;
    MctReturnValue ret = MCT_RETURN_OK;
    int fd = -1;

    if (daemon_local == NULL) {
        mct_vlog(LOG_ERR, "%s: Invalid function parameters\n", __func__);
        return -1;
    }
    ret = mct_daemon_unix_socket_open(&fd,
                                      daemon_local->flags.ctrlSockPath,
                                      SOCK_STREAM,
                                      mask);
    if (ret == MCT_RETURN_OK) {
        if (mct_connection_create(daemon_local,
                                  &daemon_local->pEvent,
                                  fd,
                                  POLLIN,
                                  MCT_CONNECTION_CONTROL_CONNECT) < MCT_RETURN_OK) {
            mct_log(LOG_ERR, "Could not initialize control socket.\n");
            ret = MCT_RETURN_ERROR;
        }
    }

    return ret;
}

int mct_daemon_local_connection_init(MctDaemon *daemon,
                                     MctDaemonLocal *daemon_local,
                                     int verbose)
{
    int fd = -1;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL)) {
        mct_vlog(LOG_ERR, "%s: Invalid function parameters\n", __func__);
        return -1;
    }

    MctBindAddress_t *head = daemon_local->flags.ipNodes;

#ifdef MCT_DAEMON_USE_UNIX_SOCKET_IPC
    /* create and open socket to receive incoming connections from user application */
    if (mct_daemon_init_app_socket(daemon_local) < MCT_RETURN_OK) {
        mct_log(LOG_ERR, "Unable to initialize app socket.\n");
        return MCT_RETURN_ERROR;
    }

#else /* MCT_DAEMON_USE_FIFO_IPC */

    if (mct_daemon_init_fifo(daemon_local)) {
        mct_log(LOG_ERR, "Unable to initialize fifo.\n");
        return MCT_RETURN_ERROR;
    }

#endif


    /* create and open socket to receive incoming connections from client */
    daemon_local->client_connections = 0;
    daemon_local->internal_client_connections = 0;

    if (mct_daemon_filter_is_connection_allowed(&daemon_local->pFilter,
                                                MCT_CONNECTION_CLIENT_CONNECT) > 0) {
        if (head == NULL) { /* no IP set in BindAddress option, will use "0.0.0.0" as default */
            if (mct_daemon_socket_open(&fd, daemon_local->flags.port,
                                       "0.0.0.0") == MCT_RETURN_OK) {
                if (mct_connection_create(daemon_local,
                                          &daemon_local->pEvent,
                                          fd,
                                          POLLIN,
                                          MCT_CONNECTION_CLIENT_CONNECT)) {
                    mct_log(LOG_ERR, "Could not initialize main socket.\n");
                    return MCT_RETURN_ERROR;
                }
            } else {
                mct_log(LOG_ERR, "Could not initialize main socket.\n");
                return MCT_RETURN_ERROR;
            }
        } else {
            while (head != NULL) { /* open socket for each IP in the bindAddress list */
                if (mct_daemon_socket_open(&fd, daemon_local->flags.port,
                                           head->ip) == MCT_RETURN_OK) {
                    if (mct_connection_create(daemon_local,
                                              &daemon_local->pEvent,
                                              fd,
                                              POLLIN,
                                              MCT_CONNECTION_CLIENT_CONNECT)) {
                        mct_log(LOG_ERR, "Could not initialize main socket.\n");
                        return MCT_RETURN_ERROR;
                    }
                } else {
                    mct_log(LOG_ERR, "Could not initialize main socket.\n");
                    return MCT_RETURN_ERROR;
                }

                head = head->next;
            }
        }
    }

    /* create and open unix socket to receive incoming connections from
     * control application */
    if (mct_daemon_initialize_control_socket(daemon_local) < MCT_RETURN_OK) {
        mct_log(LOG_ERR, "Could not initialize control socket.\n");
        return MCT_RETURN_ERROR;
    }

    /* Init serial */
    if (mct_daemon_init_serial(daemon_local) < 0) {
        mct_log(LOG_ERR, "Could not initialize daemon data\n");
        return MCT_RETURN_ERROR;
    }

    return 0;
}

int mct_daemon_local_ecu_version_init(MctDaemon *daemon, MctDaemonLocal *daemon_local, int verbose)
{
    char *version = NULL;
    FILE *f = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    /* By default, version string is null. */
    daemon->ECUVersionString = NULL;

    /* Open the file. Bail out if error occurs */
    f = fopen(daemon_local->flags.pathToECUSoftwareVersion, "r");

    if (f == NULL) {
        /* Error level notice, because this might be deliberate choice */
        mct_log(LOG_NOTICE, "Failed to open ECU Software version file.\n");
        return -1;
    }

    /* Get the file size. Bail out if stat fails. */
    int fd = fileno(f);
    struct stat s_buf;

    if (fstat(fd, &s_buf) < 0) {
        mct_log(LOG_WARNING, "Failed to stat ECU Software version file.\n");
        fclose(f);
        return -1;
    }

    /* Bail out if file is too large. Use MCT_DAEMON_TEXTBUFSIZE max.
     * Reserve one byte for trailing '\0' */
    off_t size = s_buf.st_size;

    if (size >= MCT_DAEMON_TEXTBUFSIZE) {
        mct_log(LOG_WARNING, "Too large file for ECU version.\n");
        fclose(f);
        return -1;
    }

    /* Allocate permanent buffer for version info */
    version = malloc(size + 1);

    if (version == 0) {
        mct_log(LOG_WARNING, "Cannot allocate memory for ECU version.\n");
        fclose(f);
        return -1;
    }

    off_t offset = 0;

    while (!feof(f)) {
        offset += fread(version + offset, 1, size, f);

        if (ferror(f)) {
            mct_log(LOG_WARNING, "Failed to read ECU Software version file.\n");
            free(version);
            fclose(f);
            return -1;
        }

        if (offset > size) {
            mct_log(LOG_WARNING, "Too long file for ECU Software version info.\n");
            free(version);
            fclose(f);
            return -1;
        }
    }

    version[offset] = '\0'; /*append null termination at end of version string */
    daemon->ECUVersionString = version;
    fclose(f);
    return 0;
}

void mct_daemon_local_cleanup(MctDaemon *daemon, MctDaemonLocal *daemon_local, int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == 0) || (daemon_local == 0)) {
        mct_log(LOG_ERR,
                "Invalid function parameters used for function mct_daemon_local_cleanup()\n");
        return;
    }

    /* Don't receive event anymore */
    mct_event_handler_cleanup_connections(&daemon_local->pEvent);

    mct_message_free(&(daemon_local->msg), daemon_local->flags.vflag);

    /* free shared memory */
    if (daemon_local->flags.offlineTraceDirectory[0]) {
        mct_offline_trace_free(&(daemon_local->offlineTrace));
    }

    /* Ignore result */
    mct_file_free(&(daemon_local->file), daemon_local->flags.vflag);

#ifdef MCT_DAEMON_USE_FIFO_IPC
    /* Try to delete existing pipe, ignore result of unlink() */
    unlink(daemon_local->flags.daemonFifoName);
#else /* MCT_DAEMON_USE_UNIX_SOCKET_IPC */
      /* Try to delete existing pipe, ignore result of unlink() */
    unlink(daemon_local->flags.appSockPath);
#endif
    if (daemon_local->flags.offlineLogstorageMaxDevices > 0) {
        /* disconnect all logstorage devices */
        mct_daemon_logstorage_cleanup(daemon,
                                      daemon_local,
                                      daemon_local->flags.vflag);

        free(daemon->storage_handle);
    }

    if (daemon->ECUVersionString != NULL) {
        free(daemon->ECUVersionString);
    }

    unlink(daemon_local->flags.ctrlSockPath);

    /* free IP list */
    free(daemon_local->flags.ipNodes);
}

void mct_daemon_exit_trigger()
{
    /* stop event loop */
    g_exit = -1;

#ifdef MCT_DAEMON_USE_FIFO_IPC
    char tmp[MCT_PATH_MAX] = {0};

    ssize_t n;
    n = snprintf(tmp, MCT_PATH_MAX, "%s/mct", mctFifoBaseDir);

    if ((n < 0) || ((size_t)n > MCT_PATH_MAX)) {
        mct_vlog(LOG_WARNING, "%s: snprintf truncation/error(%ld) %s\n",
                 __func__, n, tmp);
    }

    (void)unlink(tmp);
#endif


}

void mct_daemon_signal_handler(int sig)
{
    g_signo = sig;

    switch (sig) {
        case SIGHUP:
        case SIGTERM:
        case SIGINT:
        case SIGQUIT:
        {
            /* finalize the server */
            mct_vlog(LOG_NOTICE, "Exiting MCT daemon due to signal: %s\n",
                     strsignal(sig));
            mct_daemon_exit_trigger();
            break;
        }
        default:
        {
            /* This case should never happen! */
            break;
        }
    } /* switch */
}     /* mct_daemon_signal_handler() */

void mct_daemon_daemonize(int verbose)
{
    int i;
    int fd;

    PRINT_FUNCTION_VERBOSE(verbose);

    mct_log(LOG_NOTICE, "Daemon mode\n");

    /* Daemonize */
    i = fork();

    if (i < 0) {
        mct_log(LOG_CRIT, "Unable to fork(), exiting MCT daemon\n");
        exit(-1); /* fork error */
    }

    if (i > 0) {
        exit(0); /* parent exits */
    }

    /* child (daemon) continues */

    /* Process independency */

    /* obtain a new process group */
    if (setsid() == -1) {
        mct_log(LOG_CRIT, "setsid() failed, exiting MCT daemon\n");
        exit(-1); /* fork error */
    }

    /* Open standard descriptors stdin, stdout, stderr */
    fd = open("/dev/null", O_RDWR);

    if (fd != -1) {
        /* Redirect STDOUT to /dev/null */
        if (dup2(fd, STDOUT_FILENO) < 0) {
            mct_vlog(LOG_WARNING, "Failed to direct stdout to /dev/null. Error: %s\n",
                     strerror(errno));
        }

        /* Redirect STDERR to /dev/null */
        if (dup2(fd, STDERR_FILENO) < 0) {
            mct_vlog(LOG_WARNING, "Failed to direct stderr to /dev/null. Error: %s\n",
                     strerror(errno));
        }

        close(fd);
    } else {
        mct_log(LOG_CRIT, "Error opening /dev/null, exiting MCT daemon\n");
        exit(-1); /* fork error */
    }

    /* Set umask */
    umask(MCT_DAEMON_UMASK);

    /* Change to root directory */
    if (chdir("/") < 0) {
        mct_log(LOG_WARNING, "Failed to chdir to root\n");
    }

    /* Catch signals */
    signal(SIGCHLD, SIG_IGN); /* ignore child */
    signal(SIGTSTP, SIG_IGN); /* ignore tty signals */
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
} /* mct_daemon_daemonize() */

/* This function logs str to the configured output sink (socket, serial, offline trace).
 * To avoid recursion this function must be called only from MCT highlevel functions.
 * E. g. calling it to output a failure when the open of the offline trace file fails
 * would cause an endless loop because mct_daemon_log_internal() would itself again try
 * to open the offline trace file.
 * This is a mct-daemon only function. The libmct has no equivalent function available. */
int mct_daemon_log_internal(MctDaemon *daemon, MctDaemonLocal *daemon_local, char *str, int verbose)
{
    MctMessage msg;
    memset(&msg, 0, sizeof(MctMessage));
    static uint8_t uiMsgCount = 0;
    MctStandardHeaderExtra *pStandardExtra = NULL;
    uint32_t uiType;
    uint16_t uiSize;
    uint32_t uiExtraSize;

    PRINT_FUNCTION_VERBOSE(verbose);

    /* Set storageheader */
    msg.storageheader = (MctStorageHeader *)(msg.headerbuffer);
    mct_set_storageheader(msg.storageheader, daemon->ecuid);

    /* Set standardheader */
    msg.standardheader = (MctStandardHeader *)(msg.headerbuffer + sizeof(MctStorageHeader));
    msg.standardheader->htyp = MCT_HTYP_UEH | MCT_HTYP_WEID | MCT_HTYP_WSID | MCT_HTYP_WTMS |
        MCT_HTYP_PROTOCOL_VERSION1;
    msg.standardheader->mcnt = uiMsgCount++;

    uiExtraSize = MCT_STANDARD_HEADER_EXTRA_SIZE(msg.standardheader->htyp) +
        (MCT_IS_HTYP_UEH(msg.standardheader->htyp) ? sizeof(MctExtendedHeader) : 0);
    msg.headersize = sizeof(MctStorageHeader) + sizeof(MctStandardHeader) + uiExtraSize;

    /* Set extraheader */
    pStandardExtra =
        (MctStandardHeaderExtra *)(msg.headerbuffer + sizeof(MctStorageHeader) +
                                   sizeof(MctStandardHeader));
    mct_set_id(pStandardExtra->ecu, daemon->ecuid);
    pStandardExtra->tmsp = MCT_HTOBE_32(mct_uptime());
    pStandardExtra->seid = MCT_HTOBE_32(getpid());

    /* Set extendedheader */
    msg.extendedheader =
        (MctExtendedHeader *)(msg.headerbuffer + sizeof(MctStorageHeader) +
                              sizeof(MctStandardHeader) +
                              MCT_STANDARD_HEADER_EXTRA_SIZE(msg.standardheader->htyp));
    msg.extendedheader->msin = MCT_MSIN_VERB | (MCT_TYPE_LOG << MCT_MSIN_MSTP_SHIFT) |
        ((MCT_LOG_INFO << MCT_MSIN_MTIN_SHIFT) & MCT_MSIN_MTIN);
    msg.extendedheader->noar = 1;
    mct_set_id(msg.extendedheader->apid, "MCTD");
    mct_set_id(msg.extendedheader->ctid, "INTM");

    /* Set payload data... */
    uiType = MCT_TYPE_INFO_STRG;
    uiSize = strlen(str) + 1;
    msg.datasize = sizeof(uint32_t) + sizeof(uint16_t) + uiSize;

    msg.databuffer = (uint8_t *)malloc(msg.datasize);
    msg.databuffersize = msg.datasize;

    if (msg.databuffer == 0) {
        mct_log(LOG_WARNING, "Can't allocate buffer for get log info message\n");
        return -1;
    }

    msg.datasize = 0;
    memcpy((uint8_t *)(msg.databuffer + msg.datasize), (uint8_t *)(&uiType), sizeof(uint32_t));
    msg.datasize += sizeof(uint32_t);
    memcpy((uint8_t *)(msg.databuffer + msg.datasize), (uint8_t *)(&uiSize), sizeof(uint16_t));
    msg.datasize += sizeof(uint16_t);
    memcpy((uint8_t *)(msg.databuffer + msg.datasize), str, uiSize);
    msg.datasize += uiSize;

    /* Calc length */
    msg.standardheader->len = MCT_HTOBE_16(msg.headersize - sizeof(MctStorageHeader) + msg.datasize);

    mct_daemon_client_send(MCT_DAEMON_SEND_TO_ALL, daemon, daemon_local,
                           msg.headerbuffer, sizeof(MctStorageHeader),
                           msg.headerbuffer + sizeof(MctStorageHeader),
                           msg.headersize - sizeof(MctStorageHeader),
                           msg.databuffer, msg.datasize, verbose);

    free(msg.databuffer);

    return 0;
}

int mct_daemon_check_numeric_setting(char *token,
                                    char *value,
                                    unsigned long *data)
{
    char value_check[value_length];
    value_check[0] = 0;
    sscanf(value, "%lu%s", data, value_check);
    if (value_check[0] || !isdigit(value[0])) {
        fprintf(stderr, "Invalid input [%s] detected in option %s\n",
                value,
                token);
        return -1;
    }
    return 0;
}

int mct_daemon_process_client_connect(MctDaemon *daemon,
                                      MctDaemonLocal *daemon_local,
                                      MctReceiver *receiver,
                                      int verbose)
{
    socklen_t cli_size;
    struct sockaddr_un cli;

    int in_sock = -1;
    char local_str[MCT_DAEMON_TEXTBUFSIZE] = {'\0'};

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (receiver == NULL)) {
        mct_log(LOG_ERR,
                "Invalid function parameters used for function "
                "mct_daemon_process_client_connect()\n");
        return -1;
    }

    /* event from TCP server socket, new connection */
    cli_size = sizeof(cli);

    if ((in_sock = accept(receiver->fd, (struct sockaddr *)&cli, &cli_size)) < 0) {
        mct_vlog(LOG_ERR, "accept() for socket %d failed: %s\n", receiver->fd, strerror(errno));
        return -1;
    }

    /* check if file file descriptor was already used, and make it invalid if it
     * is reused. */
    /* This prevents sending messages to wrong file descriptor */
    mct_daemon_applications_invalidate_fd(daemon, daemon->ecuid, in_sock, verbose);
    mct_daemon_contexts_invalidate_fd(daemon, daemon->ecuid, in_sock, verbose);

    /* Set socket timeout in reception */
    struct timeval timeout_send;
    timeout_send.tv_sec = daemon_local->timeoutOnSend;
    timeout_send.tv_usec = 100000;

    if (setsockopt (in_sock,
                    SOL_SOCKET,
                    SO_SNDTIMEO,
                    (char *)&timeout_send,
                    sizeof(timeout_send)) < 0) {
        mct_log(LOG_WARNING, "setsockopt failed\n");
    }

    if (mct_connection_create(daemon_local,
                              &daemon_local->pEvent,
                              in_sock,
                              POLLIN,
                              MCT_CONNECTION_CLIENT_MSG_TCP)) {
        mct_log(LOG_ERR, "Failed to register new client. \n");
        /* TODO: Perform clean-up */
        return -1;
    }

    /* send connection info about connected */
    mct_daemon_control_message_connection_info(in_sock,
                                               daemon,
                                               daemon_local,
                                               MCT_CONNECTION_STATUS_CONNECTED,
                                               "",
                                               verbose);

    /* send ecu version string */
    if (daemon_local->flags.sendECUSoftwareVersion > 0) {
        if (daemon_local->flags.sendECUSoftwareVersion > 0) {
            mct_daemon_control_get_software_version(MCT_DAEMON_SEND_TO_ALL,
                                                    daemon,
                                                    daemon_local,
                                                    daemon_local->flags.vflag);
        }

        if (daemon_local->flags.sendTimezone > 0) {
            mct_daemon_control_message_timezone(MCT_DAEMON_SEND_TO_ALL,
                                                daemon,
                                                daemon_local,
                                                daemon_local->flags.vflag);
        }
    }

    snprintf(local_str, MCT_DAEMON_TEXTBUFSIZE,
             "New client connection #%d established, Total Clients : %d",
             in_sock, daemon_local->client_connections);

    mct_daemon_log_internal(daemon, daemon_local, local_str,
                            daemon_local->flags.vflag);
    mct_vlog(LOG_DEBUG, "%s%s", local_str, "\n");

    if (daemon_local->client_connections == 1) {
        if (daemon_local->flags.vflag) {
            mct_log(LOG_DEBUG, "Send ring-buffer to client\n");
        }

        mct_daemon_change_state(daemon, MCT_DAEMON_STATE_SEND_BUFFER);

        if (mct_daemon_client_update(daemon, daemon_local, verbose) !=
            MCT_RETURN_OK) {
            mct_log(LOG_WARNING, "Updating client failed\n");
        }

        /* send new log state to all applications */
        daemon->connectionState = 1;
        mct_daemon_user_send_all_log_state(daemon, verbose);
    }

    return 0;
}

int mct_daemon_client_update(MctDaemon *daemon,
                             MctDaemonLocal *daemon_local,
                             int verbose)
{

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL)) {
        mct_vlog(LOG_ERR,
                 "Invalid function parameters used for %s\n",
                 __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* send new log state to all applications */
    daemon->connectionState = 1;
    mct_daemon_user_send_all_log_state(daemon, verbose);

    if (mct_daemon_send_ringbuffer_to_client(daemon,
                                             daemon_local,
                                             verbose) != MCT_DAEMON_ERROR_OK) {
        mct_log(LOG_ERR, "Can't send contents of ring-buffer to clients\n");
        return MCT_RETURN_ERROR;
    }

    /* Send overflow if overflow occurred */
    if (daemon->overflow_counter) {
        if (mct_daemon_send_message_overflow(daemon,
                                             daemon_local,
                                             verbose) == 0) {
            mct_vlog(LOG_INFO,
                     "Overflow occurred: %u messages discarded!\n",
                     daemon->overflow_counter);
            daemon->overflow_counter = 0;
        } else {
            mct_log(LOG_ERR, "Can't send overflow message to clients\n");
            return MCT_RETURN_ERROR;
        }
    }

    return MCT_RETURN_OK;
}

int mct_daemon_process_client_messages(MctDaemon *daemon,
                                       MctDaemonLocal *daemon_local,
                                       MctReceiver *receiver,
                                       int verbose)
{
    int bytes_to_be_removed = 0;
    int must_close_socket = -1;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (receiver == NULL)) {
        mct_vlog(LOG_ERR, "%s: Invalid parameters\n", __func__);
        return -1;
    }

    must_close_socket = mct_receiver_receive(receiver);

    if (must_close_socket < 0) {
        mct_daemon_close_socket(receiver->fd,
                                daemon,
                                daemon_local,
                                verbose);
        return -1;
    }

    /* Process all received messages */
    while (mct_message_read(&(daemon_local->msg),
                            (uint8_t *)receiver->buf,
                            receiver->bytesRcvd,
                            daemon_local->flags.nflag,
                            daemon_local->flags.vflag) == MCT_MESSAGE_ERROR_OK) {
        /* Check for control message */
        if ((0 < receiver->fd) &&
            MCT_MSG_IS_CONTROL_REQUEST(&(daemon_local->msg))) {
            mct_daemon_client_process_control(receiver->fd,
                                              daemon,
                                              daemon_local,
                                              &(daemon_local->msg),
                                              daemon_local->flags.vflag);
        }

        bytes_to_be_removed = daemon_local->msg.headersize +
            daemon_local->msg.datasize -
            sizeof(MctStorageHeader);

        if (daemon_local->msg.found_serialheader) {
            bytes_to_be_removed += sizeof(mctSerialHeader);
        }

        if (daemon_local->msg.resync_offset) {
            bytes_to_be_removed += daemon_local->msg.resync_offset;
        }

        if (mct_receiver_remove(receiver, bytes_to_be_removed) == -1) {
            mct_log(LOG_WARNING,
                    "Can't remove bytes from receiver for sockets\n");
            return -1;
        }
    } /* while */

    if (mct_receiver_move_to_begin(receiver) == -1) {
        mct_log(LOG_WARNING,
                "Can't move bytes to beginning of receiver buffer for sockets\n");
        return -1;
    }

    if (must_close_socket == 0) {
        /* FIXME: Why the hell do we need to close the socket
         * on control message reception ??
         */
        mct_daemon_close_socket(receiver->fd,
                                daemon,
                                daemon_local,
                                verbose);
    }

    return 0;
}

int mct_daemon_process_client_messages_serial(MctDaemon *daemon,
                                              MctDaemonLocal *daemon_local,
                                              MctReceiver *receiver,
                                              int verbose)
{
    int bytes_to_be_removed = 0;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (receiver == NULL)) {
        mct_vlog(LOG_ERR, "%s: Invalid parameters\n", __func__);
        return -1;
    }

    if (mct_receiver_receive(receiver) <= 0) {
        mct_log(LOG_WARNING,
                "mct_receiver_receive_fd() for messages from serial interface "
                "failed!\n");
        return -1;
    }

    /* Process all received messages */
    while (mct_message_read(&(daemon_local->msg),
                            (uint8_t *)receiver->buf,
                            receiver->bytesRcvd,
                            daemon_local->flags.mflag,
                            daemon_local->flags.vflag) == MCT_MESSAGE_ERROR_OK) {
        /* Check for control message */
        if (MCT_MSG_IS_CONTROL_REQUEST(&(daemon_local->msg))) {
            if (mct_daemon_client_process_control(receiver->fd,
                                                  daemon,
                                                  daemon_local,
                                                  &(daemon_local->msg),
                                                  daemon_local->flags.vflag)
                == -1) {
                mct_log(LOG_WARNING, "Can't process control messages\n");
                return -1;
            }
        }

        bytes_to_be_removed = daemon_local->msg.headersize +
            daemon_local->msg.datasize -
            sizeof(MctStorageHeader);

        if (daemon_local->msg.found_serialheader) {
            bytes_to_be_removed += sizeof(mctSerialHeader);
        }

        if (daemon_local->msg.resync_offset) {
            bytes_to_be_removed += daemon_local->msg.resync_offset;
        }

        if (mct_receiver_remove(receiver, bytes_to_be_removed) == -1) {
            mct_log(LOG_WARNING,
                    "Can't remove bytes from receiver for serial connection\n");
            return -1;
        }
    } /* while */

    if (mct_receiver_move_to_begin(receiver) == -1) {
        mct_log(LOG_WARNING,
                "Can't move bytes to beginning of receiver buffer for serial "
                "connection\n");
        return -1;
    }

    return 0;
}

int mct_daemon_process_control_connect(
    MctDaemon *daemon,
    MctDaemonLocal *daemon_local,
    MctReceiver *receiver,
    int verbose)
{
    socklen_t ctrl_size;
    struct sockaddr_un ctrl;
    int in_sock = -1;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (receiver == NULL)) {
        mct_log(LOG_ERR,
                "Invalid function parameters used for function "
                "mct_daemon_process_control_connect()\n");
        return -1;
    }

    /* event from UNIX server socket, new connection */
    ctrl_size = sizeof(ctrl);

    if ((in_sock = accept(receiver->fd, (struct sockaddr *)&ctrl, &ctrl_size)) < 0) {
        mct_vlog(LOG_ERR, "accept() on UNIX control socket %d failed: %s\n", receiver->fd,
                 strerror(errno));
        return -1;
    }

    /* check if file file descriptor was already used, and make it invalid if it
     *  is reused */
    /* This prevents sending messages to wrong file descriptor */
    mct_daemon_applications_invalidate_fd(daemon, daemon->ecuid, in_sock, verbose);
    mct_daemon_contexts_invalidate_fd(daemon, daemon->ecuid, in_sock, verbose);

    if (mct_connection_create(daemon_local,
                              &daemon_local->pEvent,
                              in_sock,
                              POLLIN,
                              MCT_CONNECTION_CONTROL_MSG)) {
        mct_log(LOG_ERR, "Failed to register new client. \n");
        /* TODO: Perform clean-up */
        return -1;
    }

    if (verbose) {
        mct_vlog(LOG_INFO, "New connection to control client established\n");
    }

    return 0;
}

int mct_daemon_process_control_messages(
    MctDaemon *daemon,
    MctDaemonLocal *daemon_local,
    MctReceiver *receiver,
    int verbose)
{
    int bytes_to_be_removed = 0;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (receiver == NULL)) {
        mct_log(LOG_ERR,
                "Invalid function parameters used for function "
                "mct_daemon_process_control_messages()\n");
        return -1;
    }

    if (mct_receiver_receive(receiver) <= 0) {
        mct_daemon_close_socket(receiver->fd,
                                daemon,
                                daemon_local,
                                verbose);
        /* FIXME: Why the hell do we need to close the socket
         * on control message reception ??
         */
        return 0;
    }

    /* Process all received messages */
    while (mct_message_read(
               &(daemon_local->msg),
               (uint8_t *)receiver->buf,
               receiver->bytesRcvd,
               daemon_local->flags.nflag,
               daemon_local->flags.vflag) == MCT_MESSAGE_ERROR_OK) {
        /* Check for control message */
        if ((receiver->fd > 0) &&
            MCT_MSG_IS_CONTROL_REQUEST(&(daemon_local->msg))) {
            mct_daemon_client_process_control(receiver->fd,
                                              daemon, daemon_local,
                                              &(daemon_local->msg),
                                              daemon_local->flags.vflag);
        }

        bytes_to_be_removed = daemon_local->msg.headersize +
            daemon_local->msg.datasize -
            sizeof(MctStorageHeader);

        if (daemon_local->msg.found_serialheader) {
            bytes_to_be_removed += sizeof(mctSerialHeader);
        }

        if (daemon_local->msg.resync_offset) {
            bytes_to_be_removed += daemon_local->msg.resync_offset;
        }

        if (mct_receiver_remove(receiver, bytes_to_be_removed) == -1) {
            mct_log(LOG_WARNING,
                    "Can't remove bytes from receiver for sockets\n");
            return -1;
        }
    } /* while */

    if (mct_receiver_move_to_begin(receiver) == -1) {
        mct_log(LOG_WARNING, "Can't move bytes to beginning of receiver buffer for sockets\n");
        return -1;
    }

    return 0;
}

static int mct_daemon_process_user_message_not_sup(MctDaemon *daemon,
                                                   MctDaemonLocal *daemon_local,
                                                   MctReceiver *receiver,
                                                   int verbose)
{
    MctUserHeader *userheader = (MctUserHeader *)(receiver->buf);
    (void)daemon;
    (void)daemon_local;

    PRINT_FUNCTION_VERBOSE(verbose);

    mct_vlog(LOG_ERR, "Invalid user message type received: %u!\n",
             userheader->message);

    /* remove user header */
    if (mct_receiver_remove(receiver, sizeof(MctUserHeader)) == -1) {
        mct_log(LOG_WARNING,
                "Can't remove bytes from receiver for user messages\n");
    }

    return -1;
}

static mct_daemon_process_user_message_func process_user_func[MCT_USER_MESSAGE_NOT_SUPPORTED] = {
    mct_daemon_process_user_message_not_sup,
    mct_daemon_process_user_message_log,
    mct_daemon_process_user_message_register_application,
    mct_daemon_process_user_message_unregister_application,
    mct_daemon_process_user_message_register_context,
    mct_daemon_process_user_message_unregister_context,
    mct_daemon_process_user_message_not_sup,
    mct_daemon_process_user_message_not_sup,
    mct_daemon_process_user_message_overflow,
    mct_daemon_process_user_message_set_app_ll_ts,
    mct_daemon_process_user_message_not_sup,
    mct_daemon_process_user_message_not_sup,
    mct_daemon_process_user_message_not_sup,
    mct_daemon_process_user_message_marker,
    mct_daemon_process_user_message_not_sup,
    mct_daemon_process_user_message_not_sup
};

int mct_daemon_process_user_messages(MctDaemon *daemon,
                                     MctDaemonLocal *daemon_local,
                                     MctReceiver *receiver,
                                     int verbose)
{
    int offset = 0;
    int run_loop = 1;
    int32_t min_size = (int32_t)sizeof(MctUserHeader);
    MctUserHeader *userheader;
    int recv;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (receiver == NULL)) {
        mct_log(LOG_ERR,
                "Invalid function parameters used for function "
                "mct_daemon_process_user_messages()\n");
        return -1;
    }

    recv = mct_receiver_receive(receiver);

    if ((recv <= 0) && (receiver->type == MCT_RECEIVE_SOCKET)) {
        mct_daemon_close_socket(receiver->fd,
                                daemon,
                                daemon_local,
                                verbose);
        return 0;
    } else if (recv < 0) {
        mct_log(LOG_WARNING,
                "mct_receiver_receive_fd() for user messages failed!\n");
        return -1;
    }

    /* look through buffer as long as data is in there */
    while ((receiver->bytesRcvd >= min_size) && run_loop) {
        mct_daemon_process_user_message_func func = NULL;

        offset = 0;
        userheader = (MctUserHeader *)(receiver->buf + offset);

        while (!mct_user_check_userheader(userheader) &&
               (offset + min_size <= receiver->bytesRcvd)) {
            /* resync if necessary */
            offset++;
            userheader = (MctUserHeader *)(receiver->buf + offset);
        }

        /* Check for user header pattern */
        if (!mct_user_check_userheader(userheader)) {
            break;
        }

        /* Set new start offset */
        if (offset > 0) {
            mct_receiver_remove(receiver, offset);
        }

        if (userheader->message >= MCT_USER_MESSAGE_NOT_SUPPORTED) {
            func = mct_daemon_process_user_message_not_sup;
        } else {
            func = process_user_func[userheader->message];
        }

        if (func(daemon,
                 daemon_local,
                 receiver,
                 daemon_local->flags.vflag) == -1) {
            run_loop = 0;
        }
    }

    /* keep not read data in buffer */
    if (mct_receiver_move_to_begin(receiver) == -1) {
        mct_log(LOG_WARNING,
                "Can't move bytes to beginning of receiver buffer for user "
                "messages\n");
        return -1;
    }

    return 0;
}

int mct_daemon_process_user_message_overflow(MctDaemon *daemon,
                                             MctDaemonLocal *daemon_local,
                                             MctReceiver *rec,
                                             int verbose)
{
    uint32_t len = sizeof(MctUserControlMsgBufferOverflow);
    MctUserControlMsgBufferOverflow userpayload;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (rec == NULL)) {
        mct_vlog(LOG_ERR, "Invalid function parameters used for %s\n",
                 __func__);
        return -1;
    }

    if (mct_receiver_check_and_get(rec,
                                   &userpayload,
                                   len,
                                   MCT_RCV_SKIP_HEADER | MCT_RCV_REMOVE) < 0) {
        /* Not enough bytes received */
        return -1;
    }

    /* Store in daemon, that a message buffer overflow has occured */
    /* look if TCP connection to client is available or it least message can be put into buffer */
    if (mct_daemon_control_message_buffer_overflow(MCT_DAEMON_SEND_TO_ALL,
                                                   daemon,
                                                   daemon_local,
                                                   userpayload.overflow_counter,
                                                   userpayload.apid,
                                                   verbose)) {
        /* there was an error when storing message */
        /* add the counter of lost messages to the daemon counter */
        daemon->overflow_counter += userpayload.overflow_counter;
    }

    return 0;
}

int mct_daemon_send_message_overflow(MctDaemon *daemon, MctDaemonLocal *daemon_local, int verbose)
{
    int ret;
    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == 0) || (daemon_local == 0)) {
        mct_log(
            LOG_ERR,
            "Invalid function parameters used for function mct_daemon_process_user_message_overflow()\n");
        return MCT_DAEMON_ERROR_UNKNOWN;
    }

    /* Store in daemon, that a message buffer overflow has occured */
    if ((ret =
             mct_daemon_control_message_buffer_overflow(MCT_DAEMON_SEND_TO_ALL, daemon,
                                                        daemon_local,
                                                        daemon->overflow_counter,
                                                        "", verbose))) {
        return ret;
    }

    return MCT_DAEMON_ERROR_OK;
}

int mct_daemon_process_user_message_register_application(MctDaemon *daemon,
                                                         MctDaemonLocal *daemon_local,
                                                         MctReceiver *rec,
                                                         int verbose)
{
    uint32_t len = sizeof(MctUserControlMsgRegisterApplication);
    int to_remove = 0;
    MctDaemonApplication *application = NULL;
    MctDaemonApplication *old_application = NULL;
    pid_t old_pid = 0;
    char description[MCT_DAEMON_DESCSIZE + 1] = {'\0'};
    MctUserControlMsgRegisterApplication userapp;
    char *origin;
    int fd = -1;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (rec == NULL)) {
        mct_vlog(LOG_ERR, "Invalid function parameters used for %s\n",
                 __func__);
        return -1;
    }

    memset(&userapp, 0, sizeof(MctUserControlMsgRegisterApplication));
    origin = rec->buf;

    /* We shall not remove data before checking that everything is there. */
    to_remove = mct_receiver_check_and_get(rec,
                                           &userapp,
                                           len,
                                           MCT_RCV_SKIP_HEADER);

    if (to_remove < 0) {
        /* Not enough bytes received */
        return -1;
    }

    len = userapp.description_length;

    if (len > MCT_DAEMON_DESCSIZE) {
        len = MCT_DAEMON_DESCSIZE;
        mct_log(LOG_WARNING, "Application description exceeds limit\n");
    }

    /* adjust buffer pointer */
    rec->buf += to_remove + sizeof(MctUserHeader);

    if (mct_receiver_check_and_get(rec, description, len, MCT_RCV_NONE) < 0) {
        mct_log(LOG_ERR, "Unable to get application description\n");
        /* in case description was not readable, set dummy description */
        memcpy(description, "Unknown", sizeof("Unknown"));

        /* unknown len of original description, set to 0 to not remove in next
         * step. Because message buffer is re-adjusted the corrupted description
         * is ignored. */
        len = 0;
    }

    /* adjust to_remove */
    to_remove += sizeof(MctUserHeader) + len;
    /* point to begin of message */
    rec->buf = origin;

    /* We can now remove data. */
    if (mct_receiver_remove(rec, to_remove) != MCT_RETURN_OK) {
        mct_log(LOG_WARNING, "Can't remove bytes from receiver\n");
        return -1;
    }

    old_application = mct_daemon_application_find(daemon, userapp.apid, daemon->ecuid, verbose);

    if (old_application != NULL) {
        old_pid = old_application->pid;
    }

    if (rec->type == MCT_RECEIVE_SOCKET) {
        fd = rec->fd; /* For sockets, an app specific fd has already been created with accept(). */
    }

    application = mct_daemon_application_add(daemon,
                                             userapp.apid,
                                             userapp.pid,
                                             description,
                                             fd,
                                             daemon->ecuid,
                                             verbose);

    /* send log state to new application */
    mct_daemon_user_send_log_state(daemon, application, verbose);

    if (application == NULL) {
        mct_vlog(LOG_WARNING, "Can't add ApplicationID '%.4s' for PID %d\n",
                 userapp.apid, userapp.pid);
        return -1;
    } else if (old_pid != application->pid) {
        char local_str[MCT_DAEMON_TEXTBUFSIZE] = {'\0'};

        snprintf(local_str,
                 MCT_DAEMON_TEXTBUFSIZE,
                 "ApplicationID '%.4s' registered for PID %d, Description=%s",
                 application->apid,
                 application->pid,
                 application->application_description);
        mct_daemon_log_internal(daemon,
                                daemon_local,
                                local_str,
                                daemon_local->flags.vflag);
        mct_vlog(LOG_DEBUG, "%s%s", local_str, "\n");
    }

    /* Send the block mode state to the application */
    if (mct_daemon_user_update_blockmode(daemon,
                                         userapp.apid,
                                         daemon->blockMode,
                                         verbose) != MCT_RETURN_OK) {
        mct_vlog(LOG_WARNING, "Update BlockMode failed for %s\n", userapp.apid);
    }

    return 0;
}

int mct_daemon_process_user_message_register_context(MctDaemon *daemon,
                                                     MctDaemonLocal *daemon_local,
                                                     MctReceiver *rec,
                                                     int verbose)
{
    int to_remove = 0;
    uint32_t len = sizeof(MctUserControlMsgRegisterContext);
    MctUserControlMsgRegisterContext userctxt;
    char description[MCT_DAEMON_DESCSIZE + 1] = {'\0'};
    MctDaemonApplication *application = NULL;
    MctDaemonContext *context = NULL;
    MctServiceGetLogInfoRequest *req = NULL;
    char *origin;

    MctMessage msg;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (rec == NULL)) {
        mct_vlog(LOG_ERR, "Invalid function parameters used for %s\n",
                 __func__);
        return -1;
    }

    memset(&userctxt, 0, sizeof(MctUserControlMsgRegisterContext));
    origin = rec->buf;

    to_remove = mct_receiver_check_and_get(rec,
                                           &userctxt,
                                           len,
                                           MCT_RCV_SKIP_HEADER);

    if (to_remove < 0) {
        /* Not enough bytes received */
        return -1;
    }

    len = userctxt.description_length;

    if (len > MCT_DAEMON_DESCSIZE) {
        mct_vlog(LOG_WARNING, "Context description exceeds limit: %u\n", len);
        len = MCT_DAEMON_DESCSIZE;
    }

    /* adjust buffer pointer */
    rec->buf += to_remove + sizeof(MctUserHeader);

    if (mct_receiver_check_and_get(rec, description, len, MCT_RCV_NONE) < 0) {
        mct_log(LOG_ERR, "Unable to get context description\n");
        /* in case description was not readable, set dummy description */
        memcpy(description, "Unknown", sizeof("Unknown"));

        /* unknown len of original description, set to 0 to not remove in next
         * step. Because message buffer is re-adjusted the corrupted description
         * is ignored. */
        len = 0;
    }

    /* adjust to_remove */
    to_remove += sizeof(MctUserHeader) + len;
    /* point to begin of message */
    rec->buf = origin;

    /* We can now remove data. */
    if (mct_receiver_remove(rec, to_remove) != MCT_RETURN_OK) {
        mct_log(LOG_WARNING, "Can't remove bytes from receiver\n");
        return -1;
    }

    application = mct_daemon_application_find(daemon,
                                              userctxt.apid,
                                              daemon->ecuid,
                                              verbose);

    if (application == 0) {
        mct_vlog(LOG_WARNING,
                 "ApID '%.4s' not found for new ContextID '%.4s' in %s\n",
                 userctxt.apid,
                 userctxt.ctid,
                 __func__);

        return 0;
    }

    /* Set log level */
    if (userctxt.log_level == MCT_USER_LOG_LEVEL_NOT_SET) {
        userctxt.log_level = MCT_LOG_DEFAULT;
    } else {
        /* Plausibility check */
        if ((userctxt.log_level < MCT_LOG_DEFAULT) ||
                (userctxt.log_level > MCT_LOG_VERBOSE)) {
            return -1;
        }
    }

    /* Set trace status */
    if (userctxt.trace_status == MCT_USER_TRACE_STATUS_NOT_SET) {
        userctxt.trace_status = MCT_TRACE_STATUS_DEFAULT;
    } else {
        /* Plausibility check */
        if ((userctxt.trace_status < MCT_TRACE_STATUS_DEFAULT) ||
                (userctxt.trace_status > MCT_TRACE_STATUS_ON)) {
            return -1;
        }
    }

    context = mct_daemon_context_add(daemon,
                                     userctxt.apid,
                                     userctxt.ctid,
                                     userctxt.log_level,
                                     userctxt.trace_status,
                                     userctxt.log_level_pos,
                                     application->user_handle,
                                     description,
                                     daemon->ecuid,
                                     verbose);

    if (context == 0) {
        mct_vlog(LOG_WARNING,
                 "Can't add ContextID '%.4s' for ApID '%.4s'\n in %s",
                 userctxt.ctid, userctxt.apid, __func__);
        return -1;
    } else {
        char local_str[MCT_DAEMON_TEXTBUFSIZE] = {'\0'};

        snprintf(local_str,
                 MCT_DAEMON_TEXTBUFSIZE,
                 "ContextID '%.4s' registered for ApID '%.4s', Description=%s",
                 context->ctid,
                 context->apid,
                 context->context_description);

        if (verbose) {
            mct_daemon_log_internal(daemon, daemon_local, local_str, verbose);
        }

        mct_vlog(LOG_DEBUG, "%s%s", local_str, "\n");
    }

    if (daemon_local->flags.offlineLogstorageMaxDevices) {
        /* Store log level set for offline logstorage into context structure*/
        context->storage_log_level =
            mct_daemon_logstorage_get_loglevel(daemon,
                                               daemon_local->flags.offlineLogstorageMaxDevices,
                                               userctxt.apid,
                                               userctxt.ctid);
    } else {
        context->storage_log_level = MCT_LOG_DEFAULT;
    }

    /* Create automatic get log info response for registered context */
    if (daemon_local->flags.rflag) {
        /* Prepare request for get log info with one application and one context */
        if (mct_message_init(&msg, verbose) == -1) {
            mct_log(LOG_WARNING, "Can't initialize message");
            return -1;
        }

        msg.datasize = sizeof(MctServiceGetLogInfoRequest);

        if (msg.databuffer && (msg.databuffersize < msg.datasize)) {
            free(msg.databuffer);
            msg.databuffer = 0;
        }

        if (msg.databuffer == 0) {
            msg.databuffer = (uint8_t *)malloc(msg.datasize);
            msg.databuffersize = msg.datasize;
        }

        if (msg.databuffer == 0) {
            mct_log(LOG_WARNING, "Can't allocate buffer for get log info message\n");
            return -1;
        }

        req = (MctServiceGetLogInfoRequest *)msg.databuffer;

        req->service_id = MCT_SERVICE_ID_GET_LOG_INFO;
        req->options = daemon_local->flags.autoResponseGetLogInfoOption;
        mct_set_id(req->apid, userctxt.apid);
        mct_set_id(req->ctid, userctxt.ctid);
        mct_set_id(req->com, "remo");

        mct_daemon_control_get_log_info(MCT_DAEMON_SEND_TO_ALL, daemon, daemon_local, &msg, verbose);

        mct_message_free(&msg, verbose);
    }

    if (context->user_handle >= MCT_FD_MINIMUM) {
        if ((userctxt.log_level == MCT_LOG_DEFAULT) ||
            (userctxt.trace_status == MCT_TRACE_STATUS_DEFAULT)) {
            /* This call also replaces the default values with the values defined for default */
            if (mct_daemon_user_send_log_level(daemon, context, verbose) == -1) {
                mct_vlog(LOG_WARNING,
                         "Can't send current log level as response to %s for (%.4s;%.4s)\n",
                         __func__,
                         context->apid,
                         context->ctid);
                return -1;
            }
        }
    }

    return 0;
}

int mct_daemon_process_user_message_unregister_application(MctDaemon *daemon,
                                                           MctDaemonLocal *daemon_local,
                                                           MctReceiver *rec,
                                                           int verbose)
{
    uint32_t len = sizeof(MctUserControlMsgUnregisterApplication);
    MctUserControlMsgUnregisterApplication userapp;
    memset(&userapp, 0, sizeof(MctUserControlMsgUnregisterApplication));
    MctDaemonApplication *application = NULL;
    MctDaemonContext *context;
    int i, offset_base;
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (rec == NULL)) {
        mct_vlog(LOG_ERR,
                 "Invalid function parameters used for %s\n",
                 __func__);
        return -1;
    }

    if (mct_receiver_check_and_get(rec,
                                   &userapp,
                                   len,
                                   MCT_RCV_SKIP_HEADER | MCT_RCV_REMOVE) < 0) {
        /* Not enough bytes received */
        return -1;
    }

    user_list = mct_daemon_find_users_list(daemon, daemon->ecuid, verbose);

    if (user_list == NULL) {
        return -1;
    }

    if (user_list->num_applications > 0) {
        /* Delete this application and all corresponding contexts
         * for this application from internal table.
         */
        application = mct_daemon_application_find(daemon,
                                                  userapp.apid,
                                                  daemon->ecuid,
                                                  verbose);

        if (application) {
            /* Calculate start offset within contexts[] */
            offset_base = 0;

            for (i = 0; i < (application - (user_list->applications)); i++) {
                offset_base += user_list->applications[i].num_contexts;
            }

            for (i = (application->num_contexts) - 1; i >= 0; i--) {
                context = &(user_list->contexts[offset_base + i]);

                if (context) {
                    /* Delete context */
                    if (mct_daemon_context_del(daemon,
                                               context,
                                               daemon->ecuid,
                                               verbose) == -1) {
                        mct_vlog(LOG_WARNING,
                                 "Can't delete CtID '%.4s' for ApID '%.4s' in %s\n",
                                 context->ctid,
                                 context->apid,
                                 __func__);
                        return -1;
                    }
                }
            }

            /* Delete this application entry from internal table*/
            if (mct_daemon_application_del(daemon,
                                           application,
                                           daemon->ecuid,
                                           verbose) == -1) {
                mct_vlog(LOG_WARNING,
                         "Can't delete ApID '%.4s' in %s\n",
                         application->apid,
                         __func__);
                return -1;
            } else {
                char local_str[MCT_DAEMON_TEXTBUFSIZE] = {'\0'};

                snprintf(local_str,
                         MCT_DAEMON_TEXTBUFSIZE,
                         "Unregistered ApID '%.4s'",
                         userapp.apid);
                mct_daemon_log_internal(daemon,
                                        daemon_local,
                                        local_str,
                                        verbose);
                mct_vlog(LOG_DEBUG, "%s%s", local_str, "\n");
            }
        }
    }

    return 0;
}

int mct_daemon_process_user_message_unregister_context(MctDaemon *daemon,
                                                       MctDaemonLocal *daemon_local,
                                                       MctReceiver *rec,
                                                       int verbose)
{
    uint32_t len = sizeof(MctUserControlMsgUnregisterContext);
    MctUserControlMsgUnregisterContext userctxt;
    memset(&userctxt, 0, sizeof(MctUserControlMsgUnregisterContext));
    MctDaemonContext *context;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (rec == NULL)) {
        mct_vlog(LOG_ERR,
                 "Invalid function parameters used for %s\n",
                 __func__);

        return -1;
    }

    if (mct_receiver_check_and_get(rec,
                                   &userctxt,
                                   len,
                                   MCT_RCV_SKIP_HEADER | MCT_RCV_REMOVE) < 0) {
        /* Not enough bytes received */
        return -1;
    }

    context = mct_daemon_context_find(daemon,
                                      userctxt.apid,
                                      userctxt.ctid,
                                      daemon->ecuid,
                                      verbose);

    /* In case the daemon is loaded with predefined contexts and its context
     * unregisters, the context information will not be deleted from daemon's
     * table until its parent application is unregistered.
     */
    if (context && (context->predefined == false)) {
        /* Delete this connection entry from internal table*/
        if (mct_daemon_context_del(daemon, context, daemon->ecuid, verbose) == -1) {
            mct_vlog(LOG_WARNING,
                     "Can't delete CtID '%.4s' for ApID '%.4s' in %s\n",
                     userctxt.ctid,
                     userctxt.apid,
                     __func__);
            return -1;
        } else {
            char local_str[MCT_DAEMON_TEXTBUFSIZE] = {'\0'};

            snprintf(local_str,
                     MCT_DAEMON_TEXTBUFSIZE,
                     "Unregistered CtID '%.4s' for ApID '%.4s'",
                     userctxt.ctid,
                     userctxt.apid);

            if (verbose) {
                mct_daemon_log_internal(daemon,
                                        daemon_local,
                                        local_str,
                                        verbose);
            }

            mct_vlog(LOG_DEBUG, "%s%s", local_str, "\n");
        }
    }

    /* Create automatic unregister context response for unregistered context */
    if (daemon_local->flags.rflag) {
        mct_daemon_control_message_unregister_context(MCT_DAEMON_SEND_TO_ALL,
                                                      daemon,
                                                      daemon_local,
                                                      userctxt.apid,
                                                      userctxt.ctid,
                                                      "remo",
                                                      verbose);
    }

    return 0;
}

int mct_daemon_process_user_message_log(MctDaemon *daemon,
                                        MctDaemonLocal *daemon_local,
                                        MctReceiver *rec,
                                        int verbose)
{
    int ret = 0;
    int size = 0;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (rec == NULL)) {
        mct_vlog(LOG_ERR, "%s: invalid function parameters.\n", __func__);
        return MCT_DAEMON_ERROR_UNKNOWN;
    }
    ret = mct_message_read(&(daemon_local->msg),
                           (unsigned char *)rec->buf + sizeof(MctUserHeader),
                           rec->bytesRcvd - sizeof(MctUserHeader),
                           0,
                           verbose);

    if (ret != MCT_MESSAGE_ERROR_OK) {
        if (ret != MCT_MESSAGE_ERROR_SIZE) {
            /* This is a normal usecase: The daemon reads the data in 10kb chunks.
             * Thus the last trace in this chunk is probably not complete and will be completed
             * with the next chunk read. This happens always when the FIFO is filled with more than 10kb before
             * the daemon is able to read from the FIFO.
             * Thus the loglevel of this message is set to DEBUG.
             * A cleaner solution would be to check more in detail whether the message is not complete (normal usecase)
             * or the headers are corrupted (error case). */
            mct_log(LOG_DEBUG, "Can't read messages from receiver\n");
        }

        if (mct_receiver_remove(rec, rec->bytesRcvd) != MCT_RETURN_OK) {
            /* In certain rare scenarios where only a partial message has been received
             * (Eg: kernel IPC buffer memory being full), we want to discard the message
             * and not broadcast it forward to connected clients. Since the MCT library
             * checks return value of the writev() call against the sent total message
             * length, the partial message will be buffered and retransmitted again.
             * This implicitly ensures that no message loss occurs.
             */
            mct_log(LOG_WARNING, "failed to remove required bytes from receiver.\n");
        }

        return MCT_DAEMON_ERROR_UNKNOWN;
    }

    mct_daemon_client_send_message_to_all_client(daemon, daemon_local, verbose);

    /* keep not read data in buffer */
    size = daemon_local->msg.headersize +
        daemon_local->msg.datasize - sizeof(MctStorageHeader) +
        sizeof(MctUserHeader);

    if (daemon_local->msg.found_serialheader) {
        size += sizeof(mctSerialHeader);
    }

    if (mct_receiver_remove(rec, size) != MCT_RETURN_OK) {
        mct_log(LOG_WARNING, "failed to remove bytes from receiver.\n");
        return MCT_DAEMON_ERROR_UNKNOWN;
    }

    return MCT_DAEMON_ERROR_OK;
}

int mct_daemon_process_user_message_set_app_ll_ts(MctDaemon *daemon,
                                                  MctDaemonLocal *daemon_local,
                                                  MctReceiver *rec,
                                                  int verbose)
{
    uint32_t len = sizeof(MctUserControlMsgAppLogLevelTraceStatus);
    MctUserControlMsgAppLogLevelTraceStatus userctxt;
    MctDaemonApplication *application;
    MctDaemonContext *context;
    int i, offset_base;
    int8_t old_log_level, old_trace_status;
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (rec == NULL)) {
        mct_vlog(LOG_ERR,
                 "Invalid function parameters used for %s\n",
                 __func__);
        return MCT_RETURN_ERROR;
    }

    user_list = mct_daemon_find_users_list(daemon, daemon->ecuid, verbose);

    if (user_list == NULL) {
        return MCT_RETURN_ERROR;
    }

    memset(&userctxt, 0, len);

    if (mct_receiver_check_and_get(rec,
                                   &userctxt,
                                   len,
                                   MCT_RCV_SKIP_HEADER | MCT_RCV_REMOVE) < 0) {
        /* Not enough bytes received */
        return MCT_RETURN_ERROR;
    }

    if (user_list->num_applications > 0) {
        /* Get all contexts with application id matching the received application id */
        application = mct_daemon_application_find(daemon,
                                                  userctxt.apid,
                                                  daemon->ecuid,
                                                  verbose);

        if (application) {
            /* Calculate start offset within contexts[] */
            offset_base = 0;

            for (i = 0; i < (application - (user_list->applications)); i++) {
                offset_base += user_list->applications[i].num_contexts;
            }

            for (i = 0; i < application->num_contexts; i++) {
                context = &(user_list->contexts[offset_base + i]);

                if (context) {
                    old_log_level = context->log_level;
                    context->log_level = userctxt.log_level; /* No endianess conversion necessary*/

                    old_trace_status = context->trace_status;
                    context->trace_status = userctxt.trace_status;   /* No endianess conversion necessary */

                    /* The following function sends also the trace status */
                    if ((context->user_handle >= MCT_FD_MINIMUM) &&
                        (mct_daemon_user_send_log_level(daemon,
                                                        context,
                                                        verbose) != 0)) {
                        context->log_level = old_log_level;
                        context->trace_status = old_trace_status;
                    }
                }
            }
        }
    }

    return MCT_RETURN_OK;
}

int mct_daemon_process_user_message_marker(MctDaemon *daemon,
                                           MctDaemonLocal *daemon_local,
                                           MctReceiver *rec,
                                           int verbose)
{
    uint32_t len = sizeof(MctUserControlMsgLogMode);
    MctUserControlMsgLogMode userctxt;
    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon_local == NULL) || (rec == NULL)) {
        mct_vlog(LOG_ERR, "Invalid function parameters used for %s\n",
                 __func__);
        return -1;
    }

    memset(&userctxt, 0, len);

    if (mct_receiver_check_and_get(rec,
                                   &userctxt,
                                   len,
                                   MCT_RCV_SKIP_HEADER | MCT_RCV_REMOVE) < 0) {
        /* Not enough bytes received */
        return -1;
    }

    /* Create automatic unregister context response for unregistered context */
    mct_daemon_control_message_marker(MCT_DAEMON_SEND_TO_ALL, daemon, daemon_local, verbose);

    return 0;
}

int mct_daemon_send_ringbuffer_to_client(MctDaemon *daemon,
                                         MctDaemonLocal *daemon_local,
                                         int verbose)
{
    int ret;
    static uint8_t data[MCT_DAEMON_RCVBUFSIZE];
    int length;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == 0) || (daemon_local == 0)) {
        mct_log(
            LOG_ERR,
            "Invalid function parameters used for function mct_daemon_send_ringbuffer_to_client()\n");
        return MCT_DAEMON_ERROR_UNKNOWN;
    }

    if (mct_buffer_get_message_count(&(daemon->client_ringbuffer)) <= 0) {
        mct_daemon_change_state(daemon, MCT_DAEMON_STATE_SEND_DIRECT);
        return MCT_DAEMON_ERROR_OK;
    }

    while ((length = mct_buffer_copy(&(daemon->client_ringbuffer), data, sizeof(data))) > 0) {

        if ((ret =
                 mct_daemon_client_send(MCT_DAEMON_SEND_FORCE, daemon, daemon_local, 0, 0, data,
                                        length, 0, 0,
                                        verbose))) {
            return ret;
        }

        mct_buffer_remove(&(daemon->client_ringbuffer));

        if (daemon->state != MCT_DAEMON_STATE_SEND_BUFFER) {
            mct_daemon_change_state(daemon, MCT_DAEMON_STATE_SEND_BUFFER);
        }

        if (mct_buffer_get_message_count(&(daemon->client_ringbuffer)) <= 0) {
            mct_daemon_change_state(daemon, MCT_DAEMON_STATE_SEND_DIRECT);
            return MCT_DAEMON_ERROR_OK;
        }
    }

    return MCT_DAEMON_ERROR_OK;
}

int create_timer_fd(MctDaemonLocal *daemon_local,
                    int period_sec,
                    int starts_in,
                    MctTimers timer_id)
{
    int local_fd = MCT_FD_INIT;
    char *timer_name = NULL;

    if (timer_id >= MCT_TIMER_UNKNOWN) {
        mct_log(MCT_LOG_ERROR, "Unknown timer.");
        return -1;
    }

    timer_name = mct_timer_names[timer_id];

    if (daemon_local == NULL) {
        mct_log(MCT_LOG_ERROR, "Daemon local structure is NULL");
        return -1;
    }

    if ((period_sec <= 0) || (starts_in <= 0)) {
        /* timer not activated via the service file */
        mct_vlog(LOG_INFO, "<%s> not set: period=0\n", timer_name);
        local_fd = MCT_FD_INIT;
    } else {
#ifdef linux
        struct itimerspec l_timer_spec;
        local_fd = timerfd_create(CLOCK_MONOTONIC, 0);

        if (local_fd < 0) {
            mct_vlog(LOG_WARNING, "<%s> timerfd_create failed: %s\n",
                     timer_name, strerror(errno));
        }

        l_timer_spec.it_interval.tv_sec = period_sec;
        l_timer_spec.it_interval.tv_nsec = 0;
        l_timer_spec.it_value.tv_sec = starts_in;
        l_timer_spec.it_value.tv_nsec = 0;

        if (timerfd_settime(local_fd, 0, &l_timer_spec, NULL) < 0) {
            mct_vlog(LOG_WARNING, "<%s> timerfd_settime failed: %s\n",
                     timer_name, strerror(errno));
            local_fd = MCT_FD_INIT;
        }
#endif
    }

    /* If fully initialized we are done.
     * Event handling registration is done later on with other connections.
     */
    if (local_fd > 0) {
        mct_vlog(LOG_INFO, "<%s> initialized with %d timer\n", timer_name,
                 period_sec);
    }

    return mct_connection_create(daemon_local,
                                 &daemon_local->pEvent,
                                 local_fd,
                                 POLLIN,
                                 mct_timer_conn_types[timer_id]);
}

/* Close connection function */
int mct_daemon_close_socket(int sock, MctDaemon *daemon, MctDaemonLocal *daemon_local, int verbose)
{
    char local_str[MCT_DAEMON_TEXTBUFSIZE] = {'\0'};

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon_local == NULL) || (daemon == NULL)) {
        mct_log(LOG_ERR, "mct_daemon_close_socket: Invalid input parmeters\n");
        return -1;
    }

    /* Closure is done while unregistering has for any connection */
    mct_event_handler_unregister_connection(&daemon_local->pEvent,
                                            daemon_local,
                                            sock);

    if (daemon_local->client_connections == 0) {
        /* send new log state to all applications */
        daemon->connectionState = 0;
        mct_daemon_user_send_all_log_state(daemon, verbose);

        /* For offline tracing we still can use the same states */
        /* as for socket sending. Using this trick we see the traces */
        /* In the offline trace AND in the socket stream. */
        if (daemon_local->flags.yvalue[0] == 0) {
            mct_daemon_change_state(daemon, MCT_DAEMON_STATE_BUFFER);
        }

        /* update blockmode to NON-BLOCKING when no internal client is connected */
        if (daemon_local->internal_client_connections == 0) {
            if (mct_daemon_user_update_blockmode(daemon,
                                                 MCT_ALL_APPLICATIONS,
                                                 MCT_MODE_NON_BLOCKING,
                                                 verbose) != MCT_RETURN_OK) {
                mct_log(LOG_WARNING,
                        "Reset to NON-BLOCKING due to missing client connection failed\n");
            } else {
                mct_log(LOG_DEBUG,
                        "Switching to NON-BLOCKING due to missing client connection\n");
            }
        }
    }

    mct_daemon_control_message_connection_info(MCT_DAEMON_SEND_TO_ALL,
                                               daemon,
                                               daemon_local,
                                               MCT_CONNECTION_STATUS_DISCONNECTED,
                                               "",
                                               verbose);

    snprintf(local_str, MCT_DAEMON_TEXTBUFSIZE,
             "Client connection #%d closed. Total Clients : %d",
             sock,
             daemon_local->client_connections);
    mct_daemon_log_internal(daemon, daemon_local, local_str, daemon_local->flags.vflag);
    mct_vlog(LOG_DEBUG, "%s%s", local_str, "\n");

    return 0;
}

/**
 * \}
 */
