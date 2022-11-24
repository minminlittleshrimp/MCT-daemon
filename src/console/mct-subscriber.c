#include <ctype.h>      /* for isprint() */
#include <stdlib.h>     /* for atoi() */
#include <sys/stat.h>   /* for S_IRUSR, S_IWUSR, S_IRGRP, S_IROTH */mct_subscriber
#include <fcntl.h>      /* for open() */
#include <sys/uio.h>    /* for writev() */
#include <errno.h>
#include <string.h>
#include <glob.h>
#include <syslog.h>
#include <signal.h>
#include <sys/socket.h>
#ifdef __linux__
#   include <linux/limits.h>
#else
#   include <limits.h>
#endif
#include <inttypes.h>
#include "mct_subscriber.h"

mct_subscriber subscriber;

void signal_handler(int signal)
{
    switch (signal) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
    case SIGQUIT:
        /* stop main loop */
        shutdown(subscriber.receiver.fd, SHUT_RD);
        break;
    default:
        /* This case should never happen! */
        break;
    } /* switch */

}

/* Function prototypes */
int mct_subscriber_message_callback(DltMessage *message, void *data);

typedef struct {
    int aflag;
    int sflag;
    int xflag;
    int mflag;
    int vflag;
    int yflag;
    int uflag;
    char *ovalue;
    char *ovaluebase; /* ovalue without ".mct" */
    char *fvalue;       /* filename for space separated filter file (<AppID> <ContextID>) */
    char *jvalue;       /* filename for json filter file */
    char *evalue;
    int bvalue;
    int sendSerialHeaderFlag;
    int resyncSerialHeaderFlag;
    int64_t climit;
    char ctrl_unit_id[4];
    int ohandle;
    int64_t totalbytes; /* bytes written so far into the output file, used to check the file size limit */
    int part_num;    /* number of current output file if limit was exceeded */
    mct_file file;
    mct_filter filter;
    int port;
    char *ifaddr;
} mct_subscriber_data;

/**
 * Print usage information of tool.
 */
void usage()
{
    char version[255];

    mct_get_version(version, 255);

    printf("Usage: mct_subscriber [options] hostname/serial_device_name\n");
    printf("Receive mct messages from MCT daemon and print or store the messages.\n");
    printf("%s \n", version);
    printf("Options:\n");
    printf("  -a                    Print MCT messages; payload as ASCII\n");
    printf("  -x                    Print MCT messages; payload as hex\n");
    printf("  -m                    Print MCT messages; payload as hex and ASCII\n");
    printf("  -v                    Verbose mode\n");
    printf("  -h                    Usage\n");
    printf("  -i addr               Host interface address\n");
    printf("  -b baudrate           Serial device baudrate (Default: 115200)\n");
    printf("  -c control unit       Set ECU ID (Default: RECV)\n");
    printf("  -o filename           Output messages in new MCT file\n");
    printf("  -l limit              Restrict file size to <limit> bytes when output to file\n");
    printf("                        When limit is reached, a new file is opened. Use K,M,G as\n");
    printf("                        suffix to specify kilo-, mega-, giga-bytes respectively\n");
    printf("  -f filename           Enable filtering of messages with space separated list (<AppID> <ContextID>)\n");
    printf("  -j filename           Enable filtering of messages with filter defined in json file\n");
    printf("  -p port               Use the given port instead the default port\n");
    printf("                        Cannot be used with serial devices\n");
}


int64_t convert_arg_to_byte_size(char *arg)
{
    size_t i;
    int64_t factor;
    int64_t result;

    /* check if valid input */
    for (i = 0; i < strlen(arg) - 1; ++i)
        if (!isdigit(arg[i]))
            return -2;

    /* last character */
    factor = 1;

    if ((arg[strlen(arg) - 1] == 'K') || (arg[strlen(arg) - 1] == 'k'))
        factor = 1024;
    else if ((arg[strlen(arg) - 1] == 'M') || (arg[strlen(arg) - 1] == 'm'))
        factor = 1024 * 1024;
    else if ((arg[strlen(arg) - 1] == 'G') || (arg[strlen(arg) - 1] == 'g'))
        factor = 1024 * 1024 * 1024;
    else if (!isdigit(arg[strlen(arg) - 1]))
        return -2;

    /* range checking */
    int64_t const mult = atoll(arg);

    if (((INT64_MAX) / factor) < mult)
        /* Would overflow! */
        return -2;

    result = factor * mult;

    /* The result be at least the size of one message
     * One message consists of its header + user data:
     */
    mct_msg msg;
    int64_t min_size = sizeof(msg.headerbuffer);
    min_size += 2048 /* MCT_USER_BUF_MAX_SIZE */;

    if (min_size > result) {
        mct_vlog(LOG_ERR,
                 "ERROR: Specified limit: %" PRId64 "is smaller than a the size of a single message: %" PRId64 "!\n",
                 result,
                 min_size);
        result = -2;
    }

    return result;
}


/*
 * open output file
 */
int mct_subscriber_open_output_file(DltReceiveData *mct_data)
{
    /* if (file_already_exists) */
    glob_t outer;

    if (glob(mct_data->ovalue,
#ifndef __ANDROID_API__
             GLOB_TILDE |
#endif
             GLOB_NOSORT, NULL, &outer) == 0) {
        if (mct_data->vflag)
            mct_vlog(LOG_INFO, "File %s already exists, need to rename first\n", mct_data->ovalue);

        if (mct_data->part_num < 0) {
            char pattern[PATH_MAX + 1];
            pattern[PATH_MAX] = 0;
            snprintf(pattern, PATH_MAX, "%s.*.mct", mct_data->ovaluebase);
            glob_t inner;

            /* sort does not help here because we have to traverse the
             * full result in any case. Remember, a sorted list would look like:
             * foo.1.mct
             * foo.10.mct
             * foo.1000.mct
             * foo.11.mct
             */
            if (glob(pattern,
#ifndef __ANDROID_API__
                     GLOB_TILDE |
#endif
                     GLOB_NOSORT, NULL, &inner) == 0) {
                /* search for the highest number used */
                size_t i;

                for (i = 0; i < inner.gl_pathc; ++i) {
                    /* convert string that follows the period after the initial portion,
                     * e.g. gt.gl_pathv[i] = foo.1.mct -> atoi("1.mct");
                     */
                    int cur = atoi(&inner.gl_pathv[i][strlen(mct_data->ovaluebase) + 1]);

                    if (cur > mct_data->part_num)
                        mct_data->part_num = cur;
                }
            }

            globfree(&inner);

            ++mct_data->part_num;

        }

        char filename[PATH_MAX + 1];
        filename[PATH_MAX] = 0;

        snprintf(filename, PATH_MAX, "%s.%i.mct", mct_data->ovaluebase, mct_data->part_num++);

        if (rename(mct_data->ovalue, filename) != 0)
            mct_vlog(LOG_ERR, "ERROR: rename %s to %s failed with error %s\n",
                     mct_data->ovalue, filename, strerror(errno));
        else if (mct_data->vflag)
            mct_vlog(LOG_INFO, "Renaming existing file from %s to %s\n",
                     mct_data->ovalue, filename);
    } /* if (file_already_exists) */

    globfree(&outer);

    mct_data->ohandle = open(mct_data->ovalue, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    return mct_data->ohandle;
}


void mct_subscriber_close_output_file(mct_subscriber_data *mct_data)
{
    if (mct_data->ohandle) {
        close(mct_data->ohandle);
        mct_data->ohandle = -1;
    }
}


/**
 * Main function of tool.
 */
int main(int argc, char *argv[])
{
    mct_subscriber_data mct_data;
    memset(&mct_data, 0, sizeof(mct_data));
    int c;
    int index;

    /* Initialize mct_data */
    mct_data.climit = -1; /* default: -1 = unlimited */
    mct_data.ohandle = -1;
    mct_data.part_num = -1;
    mct_data.port = 3490;

    /* Config signal handler */
    struct sigaction act;

    /* Initialize signal handler struct */
    memset(&act, 0, sizeof(act));
    act.sa_handler = signal_handler;
    sigemptyset(&act.sa_mask);
    sigaction(SIGHUP, &act, 0);
    sigaction(SIGTERM, &act, 0);
    sigaction(SIGINT, &act, 0);
    sigaction(SIGQUIT, &act, 0);

    /* Fetch command line arguments */
    opterr = 0;

    while ((c = getopt (argc, argv, "vahyuxmf:j:o:c:b:l:p:i:")) != -1)
        switch (c) {
        case 'a':
        {
            mct_data.aflag = 1;
            break;
        }
        case 'x':
        {
            mct_data.xflag = 1;
            break;
        }
        case 'm':
        {
            mct_data.mflag = 1;
            break;
        }
        case 'v':
        {
            mct_data.vflag = 1;
            break;
        }
        case 'h':
        {
            usage();
            return -1;
        }
        case 'i':
        {
            mct_data.ifaddr = optarg;
            break;
        }
        case 'b':
        {
            mct_data.bvalue = atoi(optarg);
            break;
        }
        case 'c':
        {
            mct_data.evalue = optarg;
            break;
        }
        case 'o':
        {
            mct_data.ovalue = optarg;
            size_t to_copy = strlen(mct_data.ovalue);

            if (strcmp(&mct_data.ovalue[to_copy - 4], ".mct") == 0)
                to_copy = to_copy - 4;

            mct_data.ovaluebase = (char *)calloc(1, to_copy + 1);

            if (mct_data.ovaluebase == NULL) {
                fprintf (stderr, "Memory allocation failed.\n");
                return -1;
            }

            mct_data.ovaluebase[to_copy] = '\0';
            memcpy(mct_data.ovaluebase, mct_data.ovalue, to_copy);
            break;
        }
        case 'l':
        {
            mct_data.climit = convert_arg_to_byte_size(optarg);

            if (mct_data.climit < -1) {
                fprintf (stderr, "Invalid argument for option -c.\n");
                /* unknown or wrong option used, show usage information and terminate */
                usage();
                return -1;
            }

            break;
        }
        case 'f':
        {
            mct_data.fvalue = optarg;
            break;
        }
        case 'j':
        {
            fprintf (stderr,
                     "Extended filtering is not supported. Please build with the corresponding cmake option to use it.\n");
            return -1;
        }
        case 'p':
        {
            mct_data.port = atoi(optarg);
            break;
        }
        case '?':
        {
            if ((optopt == 'o') || (optopt == 'f') || (optopt == 'c'))
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);

            /* unknown or wrong option used, show usage information and terminate */
            usage();
            return -1;
        }
        default:
        {
            abort ();
            return -1;    /*for parasoft */
        }
        }

    /* Initialize MCT Client */
    mct_client_init(&mct_subscriber, mct_data.vflag);

    /* Register callback to be called when message was received */
    mct_client_register_message_callback(mct_subscriber_message_callback);

    /* Setup MCT Client structure */
    mct_subscriber.mode = mct_data.yflag;

    if (mct_subscriber.mode == MCT_SUBSCRIBER_MODE_TCP) {
        mct_subscriber.port = mct_data.port;

        unsigned int IP_Length = 1; // Counting the terminating 0 byte
        for (index = optind; index < argc; index++) {
            servIPLength += strlen(argv[index]);
            if (index > optind) {
                servIPLength++; // For the comma delimiter
            }
        }
        if (servIPLength > 1) {
            char* servIPString = malloc(servIPLength);
            strcpy(servIPString, argv[optind]);

            for (index = optind + 1; index < argc; index++) {
                strcat(servIPString, ",");
                strcat(servIPString, argv[index]);
            }

            int retval = mct_client_set_server_ip(&mct_subscriber, servIPString);
            free(servIPString);

            if (retval == -1) {
                fprintf(stderr, "set server ip didn't succeed\n");
                return -1;
            }
        }

        if (mct_subscriber.servIP == 0) {
            /* no hostname selected, show usage and terminate */
            fprintf(stderr, "ERROR: No hostname selected\n");
            usage();
            mct_client_cleanup(&mct_subscriber, mct_data.vflag);
            return -1;
        }

        if (mct_data.ifaddr != 0) {
            if (mct_client_set_host_if_address(&mct_subscriber, mct_data.ifaddr) != MCT_RETURN_OK) {
                fprintf(stderr, "set host interface address didn't succeed\n");
                return -1;
            }
        }
    }
    else {
        for (index = optind; index < argc; index++)
            if (mct_client_set_serial_device(&mct_subscriber, argv[index]) == -1) {
                fprintf(stderr, "set serial device didn't succeed\n");
                return -1;
            }

        if (mct_subscriber.serialDevice == 0) {
            /* no serial device name selected, show usage and terminate */
            fprintf(stderr, "ERROR: No serial device name specified\n");
            usage();
            return -1;
        }

        mct_client_setbaudrate(&mct_, mct_data.bvalue);
    }

    /* initialise structure to use MCT file */
    mct_file_init(&(mct_data.file), mct_data.vflag);

    /* first parse filter file if filter parameter is used */
    mct_filter_init(&(mct_data.filter), mct_data.vflag);

    if (mct_data.fvalue) {
        if (mct_filter_load(&(mct_data.filter), mct_data.fvalue, mct_data.vflag) < MCT_RETURN_OK) {
            mct_file_free(&(mct_data.file), mct_data.vflag);
            return -1;
        }

        mct_file_set_filter(&(mct_data.file), &(mct_data.filter), mct_data.vflag);
    }

    #ifdef EXTENDED_FILTERING

    if (mct_data.jvalue) {
        if (mct_json_filter_load(&(mct_data.filter), mct_data.jvalue, mct_data.vflag) < MCT_RETURN_OK) {
            mct_file_free(&(mct_data.file), mct_data.vflag);
            return -1;
        }

        mct_file_set_filter(&(mct_data.file), &(mct_data.filter), mct_data.vflag);
    }

    #endif

    /* open MCT output file */
    if (mct_data.ovalue) {
        if (mct_data.climit > -1) {
            mct_vlog(LOG_INFO, "Using file size limit of %" PRId64 "bytes\n",
                     mct_data.climit);
            mct_data.ohandle = mct_subscriber_open_output_file(&mct_data);
        }
        else { /* in case no limit for the output file is given, we simply overwrite any existing file */
            mct_data.ohandle = open(mct_data.ovalue, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        }

        if (mct_data.ohandle == -1) {
            mct_file_free(&(mct_data.file), mct_data.vflag);
            fprintf(stderr, "ERROR: Output file %s cannot be opened!\n", mct_data.ovalue);
            return -1;
        }
    }

    if (mct_data.evalue)
        mct_set_id(mct_data.ecuid, mct_data.evalue);
    else
        mct_set_id(mct_data.ecuid, mct_subscriber_ECU_ID);

    /* Connect to TCP socket or open serial device */
    if (mct_client_connect(&mct_subscriber, mct_data.vflag) != MCT_RETURN_ERROR) {

        /* Dlt Client Main Loop */
        mct_client_main_loop(&mct_subscriber, &mct_data, mct_data.vflag);

        /* Dlt Client Cleanup */
        mct_client_cleanup(&mct_subscriber, mct_data.vflag);
    }

    /* mct_subscriber cleanup */
    if (mct_data.ovalue)
        close(mct_data.ohandle);

    free(mct_data.ovaluebase);

    mct_file_free(&(mct_data.file), mct_data.vflag);

    mct_filter_free(&(mct_data.filter), mct_data.vflag);

    return 0;
}

int mct_subscriber_message_callback(DltMessage *message, void *data)
{
    DltReceiveData *mct_data;
    static char text[mct_subscriber_BUFSIZE];

    struct iovec iov[2];
    int bytes_written;

    if ((message == 0) || (data == 0))
        return -1;

    mct_data = (DltReceiveData *)data;

    /* prepare storage header */
    if (MCT_IS_HTYP_WEID(message->standardheader->htyp))
        mct_set_storageheader(message->storageheader, message->headerextra.ecu);
    else
        mct_set_storageheader(message->storageheader, mct_data->ecuid);

    if (((mct_data->fvalue || mct_data->jvalue) == 0) ||
        (mct_message_filter_check(message, &(mct_data->filter), mct_data->vflag) == MCT_RETURN_TRUE)) {
        /* if no filter set or filter is matching display message */
        if (mct_data->xflag) {
            mct_message_print_hex(message, text, mct_subscriber_BUFSIZE, mct_data->vflag);
        }
        else if (mct_data->aflag)
        {

            mct_message_header(message, text, mct_subscriber_BUFSIZE, mct_data->vflag);

            printf("%s ", text);

            mct_message_payload(message, text, mct_subscriber_BUFSIZE, MCT_OUTPUT_ASCII, mct_data->vflag);

            printf("[%s]\n", text);
        }
        else if (mct_data->mflag)
        {
            mct_message_print_mixed_plain(message, text, mct_subscriber_BUFSIZE, mct_data->vflag);
        }
        else if (mct_data->sflag)
        {

            mct_message_header(message, text, mct_subscriber_BUFSIZE, mct_data->vflag);

            printf("%s \n", text);
        }

        /* if file output enabled write message */
        if (mct_data->ovalue) {
            iov[0].iov_base = message->headerbuffer;
            iov[0].iov_len = (uint32_t)message->headersize;
            iov[1].iov_base = message->databuffer;
            iov[1].iov_len = (uint32_t)message->datasize;

            if (mct_data->climit > -1) {
                uint32_t bytes_to_write = message->headersize + message->datasize;

                if ((bytes_to_write + mct_data->totalbytes > mct_data->climit)) {
                    mct_subscriber_close_output_file(mct_data);

                    if (mct_subscriber_open_output_file(mct_data) < 0) {
                        printf(
                            "ERROR: mct_subscriber_message_callback: Unable to open log when maximum filesize was reached!\n");
                        return -1;
                    }

                    mct_data->totalbytes = 0;
                }
            }

            bytes_written = (int)writev(mct_data->ohandle, iov, 2);

            mct_data->totalbytes += bytes_written;

            if (0 > bytes_written) {
                printf("mct_subscriber_message_callback: writev(mct_data->ohandle, iov, 2); returned an error!");
                return -1;
            }
        }
    }

    return 0;
}
