#include <ctype.h>      /* for isprint() */
#include <stdlib.h>     /* for atoi() */
#include <sys/stat.h>   /* for S_IRUSR, S_IWUSR, S_IRGRP, S_IROTH */
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

#include "mct_client.h"

#define DLT_RECEIVE_ECU_ID "RECV"

DltClient mctclient;

void signal_handler(int signal)
{
    switch (signal) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
    case SIGQUIT:
        /* stop main loop */
        shutdown(mctclient.receiver.fd, SHUT_RD);
        break;
    default:
        /* This case should never happen! */
        break;
    } /* switch */

}

/* Function prototypes */
int mct_receive_message_callback(DltMessage *message, void *data);

typedef struct {
    int aflag;
    int sflag;
    int xflag;
    int mflag;
    int vflag;
    int yflag;
    int Bflag;
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
    char ecuid[4];
    int ohandle;
    int64_t totalbytes; /* bytes written so far into the output file, used to check the file size limit */
    int part_num;    /* number of current output file if limit was exceeded */
    DltFile file;
    DltFilter filter;
    int port;
} DltReceiveData;

/**
 * Print usage information of tool.
 */
void usage()
{
    char version[255];

    mct_get_version(version, 255);

    printf("Usage: mct-receive [options] hostname/serial_device_name\n");
    printf("Receive DLT messages from DLT daemon and print or store the messages.\n");
    printf("Use filters to filter received messages.\n");
    printf("%s \n", version);
    printf("Options:\n");
    printf("  -a            Print DLT messages; payload as ASCII\n");
    printf("  -x            Print DLT messages; payload as hex\n");
    printf("  -m            Print DLT messages; payload as hex and ASCII\n");
    printf("  -s            Print DLT messages; only headers\n");
    printf("  -v            Verbose mode\n");
    printf("  -h            Usage\n");
    printf("  -S            Send message with serial header (Default: Without serial header)\n");
    printf("  -R            Enable resync serial header\n");
    printf("  -y            Serial device mode\n");
    printf("  -u            UDP multicast mode\n");
    printf("  -b baudrate   Serial device baudrate (Default: 115200)\n");
    printf("  -e ecuid      Set ECU ID (Default: RECV)\n");
    printf("  -o filename   Output messages in new DLT file\n");
    printf("  -c limit      Restrict file size to <limit> bytes when output to file\n");
    printf("                When limit is reached, a new file is opened. Use K,M,G as\n");
    printf("                suffix to specify kilo-, mega-, giga-bytes respectively\n");
    printf("  -f filename   Enable filtering of messages with space separated list (<AppID> <ContextID>)\n");
    printf("  -j filename   Enable filtering of messages with filter defined in json file\n");
    printf("  -p port       Use the given port instead the default port\n");
    printf("                Cannot be used with serial devices\n");
    printf("  -B            Start with Blockmode as BLOCKING\n");
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
    DltMessage msg;
    int64_t min_size = sizeof(msg.headerbuffer);
    min_size += 2048 /* DLT_USER_BUF_MAX_SIZE */;

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
int mct_receive_open_output_file(DltReceiveData *mctdata)
{
    /* if (file_already_exists) */
    glob_t outer;

    if (glob(mctdata->ovalue,
             GLOB_NOSORT, NULL, &outer) == 0) {
        if (mctdata->vflag)
            mct_vlog(LOG_INFO, "File %s already exists, need to rename first\n", mctdata->ovalue);

        if (mctdata->part_num < 0) {
            char pattern[PATH_MAX + 1];
            pattern[PATH_MAX] = 0;
            snprintf(pattern, PATH_MAX, "%s.*.mct", mctdata->ovaluebase);
            glob_t inner;

            /* sort does not help here because we have to traverse the
             * full result in any case. Remember, a sorted list would look like:
             * foo.1.mct
             * foo.10.mct
             * foo.1000.mct
             * foo.11.mct
             */
            if (glob(pattern,
                     GLOB_NOSORT, NULL, &inner) == 0) {
                /* search for the highest number used */
                size_t i;

                for (i = 0; i < inner.gl_pathc; ++i) {
                    /* convert string that follows the period after the initial portion,
                     * e.g. gt.gl_pathv[i] = foo.1.mct -> atoi("1.mct");
                     */
                    int cur = atoi(&inner.gl_pathv[i][strlen(mctdata->ovaluebase) + 1]);

                    if (cur > mctdata->part_num)
                        mctdata->part_num = cur;
                }
            }

            globfree(&inner);

            ++mctdata->part_num;

        }

        char filename[PATH_MAX + 1];
        filename[PATH_MAX] = 0;

        snprintf(filename, PATH_MAX, "%s.%i.mct", mctdata->ovaluebase, mctdata->part_num++);

        if (rename(mctdata->ovalue, filename) != 0)
            mct_vlog(LOG_ERR, "ERROR: rename %s to %s failed with error %s\n",
                     mctdata->ovalue, filename, strerror(errno));
        else if (mctdata->vflag)
            mct_vlog(LOG_INFO, "Renaming existing file from %s to %s\n",
                     mctdata->ovalue, filename);
    } /* if (file_already_exists) */

    globfree(&outer);

    mctdata->ohandle = open(mctdata->ovalue, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    return mctdata->ohandle;
}


void mct_receive_close_output_file(DltReceiveData *mctdata)
{
    if (mctdata->ohandle) {
        close(mctdata->ohandle);
        mctdata->ohandle = -1;
    }
}


/**
 * Main function of tool.
 */
int main(int argc, char *argv[])
{
    DltReceiveData mctdata;
    int c;
    int index;

    /* Initialize mctdata */
    mctdata.aflag = 0;
    mctdata.sflag = 0;
    mctdata.xflag = 0;
    mctdata.mflag = 0;
    mctdata.vflag = 0;
    mctdata.yflag = 0;
    mctdata.uflag = 0;
    mctdata.ovalue = 0;
    mctdata.ovaluebase = 0;
    mctdata.fvalue = 0;
    mctdata.jvalue = 0;
    mctdata.evalue = 0;
    mctdata.bvalue = 0;
    mctdata.sendSerialHeaderFlag = 0;
    mctdata.resyncSerialHeaderFlag = 0;
    mctdata.climit = -1; /* default: -1 = unlimited */
    mctdata.ohandle = -1;
    mctdata.totalbytes = 0;
    mctdata.part_num = -1;
    mctdata.Bflag = DLT_MODE_NON_BLOCKING;
    mctdata.port = 3490;

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

    while ((c = getopt (argc, argv, "vashSRyuxmf:j:o:e:b:c:p:B")) != -1)
        switch (c) {
        case 'v':
        {
            mctdata.vflag = 1;
            break;
        }
        case 'a':
        {
            mctdata.aflag = 1;
            break;
        }
        case 's':
        {
            mctdata.sflag = 1;
            break;
        }
        case 'x':
        {
            mctdata.xflag = 1;
            break;
        }
        case 'm':
        {
            mctdata.mflag = 1;
            break;
        }
        case 'h':
        {
            usage();
            return -1;
        }
        case 'S':
        {
            mctdata.sendSerialHeaderFlag = 1;
            break;
        }
        case 'R':
        {
            mctdata.resyncSerialHeaderFlag = 1;
            break;
        }
        case 'y':
        {
            mctdata.yflag = 1;
            break;
        }
        case 'u':
        {
            mctdata.uflag = 1;
            break;
        }
        case 'f':
        {
            mctdata.fvalue = optarg;
            break;
        }
        case 'j':
        {
            #ifdef EXTENDED_FILTERING
            mctdata.jvalue = optarg;
            break;
            #else
            fprintf (stderr,
                     "Extended filtering is not supported. Please build with the corresponding cmake option to use it.\n");
            return -1;
            #endif
        }
        case 'o':
        {
            mctdata.ovalue = optarg;
            size_t to_copy = strlen(mctdata.ovalue);

            if (strcmp(&mctdata.ovalue[to_copy - 4], ".mct") == 0)
                to_copy = to_copy - 4;

            mctdata.ovaluebase = (char *)calloc(1, to_copy + 1);

            if (mctdata.ovaluebase == NULL) {
                fprintf (stderr, "Memory allocation failed.\n");
                return -1;
            }

            mctdata.ovaluebase[to_copy] = '\0';
            memcpy(mctdata.ovaluebase, mctdata.ovalue, to_copy);
            break;
        }
        case 'e':
        {
            mctdata.evalue = optarg;
            break;
        }
        case 'b':
        {
            mctdata.bvalue = atoi(optarg);
            break;
        }
        case 'B':
        {
            mctdata.Bflag = DLT_MODE_BLOCKING;
            break;
        }
        case 'p':
        {
            mctdata.port = atoi(optarg);
            break;
        }
        case 'c':
        {
            mctdata.climit = convert_arg_to_byte_size(optarg);

            if (mctdata.climit < -1) {
                fprintf (stderr, "Invalid argument for option -c.\n");
                /* unknown or wrong option used, show usage information and terminate */
                usage();
                return -1;
            }

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

    /* Initialize DLT Client */
    mct_client_init(&mctclient, mctdata.vflag);

    /* Register callback to be called when message was received */
    mct_client_register_message_callback(mct_receive_message_callback);

    /* Setup DLT Client structure */
    if(mctdata.uflag) {
        mctclient.mode = DLT_CLIENT_MODE_UDP_MULTICAST;
    }
    else {
        mctclient.mode = mctdata.yflag;
    }

    if (mctclient.mode == DLT_CLIENT_MODE_TCP || mctclient.mode == DLT_CLIENT_MODE_UDP_MULTICAST) {
        mctclient.port = mctdata.port;
        for (index = optind; index < argc; index++)
            if (mct_client_set_server_ip(&mctclient, argv[index]) == -1) {
                fprintf(stderr, "set server ip didn't succeed\n");
                return -1;
            }

        if (mctclient.servIP == 0) {
            /* no hostname selected, show usage and terminate */
            fprintf(stderr, "ERROR: No hostname selected\n");
            usage();
            mct_client_cleanup(&mctclient, mctdata.vflag);
            return -1;
        }
    }
    else {
        for (index = optind; index < argc; index++)
            if (mct_client_set_serial_device(&mctclient, argv[index]) == -1) {
                fprintf(stderr, "set serial device didn't succeed\n");
                return -1;
            }

        if (mctclient.serialDevice == 0) {
            /* no serial device name selected, show usage and terminate */
            fprintf(stderr, "ERROR: No serial device name specified\n");
            usage();
            return -1;
        }

        mct_client_setbaudrate(&mctclient, mctdata.bvalue);
    }

    /* Update the send and resync serial header flags based on command line option */
    mctclient.send_serial_header = mctdata.sendSerialHeaderFlag;
    mctclient.resync_serial_header = mctdata.resyncSerialHeaderFlag;

    /* initialise structure to use DLT file */
    mct_file_init(&(mctdata.file), mctdata.vflag);

    /* first parse filter file if filter parameter is used */
    mct_filter_init(&(mctdata.filter), mctdata.vflag);

    if (mctdata.fvalue) {
        if (mct_filter_load(&(mctdata.filter), mctdata.fvalue, mctdata.vflag) < DLT_RETURN_OK) {
            mct_file_free(&(mctdata.file), mctdata.vflag);
            return -1;
        }

        mct_file_set_filter(&(mctdata.file), &(mctdata.filter), mctdata.vflag);
    }

    #ifdef EXTENDED_FILTERING

    if (mctdata.jvalue) {
        if (mct_json_filter_load(&(mctdata.filter), mctdata.jvalue, mctdata.vflag) < DLT_RETURN_OK) {
            mct_file_free(&(mctdata.file), mctdata.vflag);
            return -1;
        }

        mct_file_set_filter(&(mctdata.file), &(mctdata.filter), mctdata.vflag);
    }

    #endif

    /* open DLT output file */
    if (mctdata.ovalue) {
        if (mctdata.climit > -1) {
            mct_vlog(LOG_INFO, "Using file size limit of %" PRId64 "bytes\n",
                     mctdata.climit);
            mctdata.ohandle = mct_receive_open_output_file(&mctdata);
        }
        else { /* in case no limit for the output file is given, we simply overwrite any existing file */
            mctdata.ohandle = open(mctdata.ovalue, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        }

        if (mctdata.ohandle == -1) {
            mct_file_free(&(mctdata.file), mctdata.vflag);
            fprintf(stderr, "ERROR: Output file %s cannot be opened!\n", mctdata.ovalue);
            return -1;
        }
    }

    if (mctdata.evalue)
        mct_set_id(mctdata.ecuid, mctdata.evalue);
    else
        mct_set_id(mctdata.ecuid, DLT_RECEIVE_ECU_ID);

    /* Connect to TCP socket or open serial device */
    if (mct_client_connect(&mctclient, mctdata.vflag) != DLT_RETURN_ERROR) {
        if (mctdata.Bflag) {
            /* send control message*/
            if (0 != mct_client_send_set_blockmode(&mctclient, DLT_MODE_BLOCKING))
                fprintf(stderr, "ERROR: Could not send Blockmode control message\n");

            ;
        }

        /* Dlt Client Main Loop */
        mct_client_main_loop(&mctclient, &mctdata, mctdata.vflag);

        /* Dlt Client Cleanup */
        mct_client_cleanup(&mctclient, mctdata.vflag);
    }

    /* mct-receive cleanup */
    if (mctdata.ovalue)
        close(mctdata.ohandle);

    free(mctdata.ovaluebase);

    mct_file_free(&(mctdata.file), mctdata.vflag);

    mct_filter_free(&(mctdata.filter), mctdata.vflag);

    return 0;
}

int mct_receive_message_callback(DltMessage *message, void *data)
{
    DltReceiveData *mctdata;
    static char text[DLT_RECEIVE_BUFSIZE];

    struct iovec iov[2];
    int bytes_written;

    if ((message == 0) || (data == 0))
        return -1;

    mctdata = (DltReceiveData *)data;

    /* prepare storage header */
    if (DLT_IS_HTYP_WEID(message->standardheader->htyp))
        mct_set_storageheader(message->storageheader, message->headerextra.ecu);
    else
        mct_set_storageheader(message->storageheader, mctdata->ecuid);

    if (((mctdata->fvalue || mctdata->jvalue) == 0) ||
        (mct_message_filter_check(message, &(mctdata->filter), mctdata->vflag) == DLT_RETURN_TRUE)) {
        /* if no filter set or filter is matching display message */
        if (mctdata->xflag) {
            mct_message_print_hex(message, text, DLT_RECEIVE_BUFSIZE, mctdata->vflag);
        }
        else if (mctdata->aflag)
        {

            mct_message_header(message, text, DLT_RECEIVE_BUFSIZE, mctdata->vflag);

            printf("%s ", text);

            mct_message_payload(message, text, DLT_RECEIVE_BUFSIZE, DLT_OUTPUT_ASCII, mctdata->vflag);

            printf("[%s]\n", text);
        }
        else if (mctdata->mflag)
        {
            mct_message_print_mixed_plain(message, text, DLT_RECEIVE_BUFSIZE, mctdata->vflag);
        }
        else if (mctdata->sflag)
        {

            mct_message_header(message, text, DLT_RECEIVE_BUFSIZE, mctdata->vflag);

            printf("%s \n", text);
        }

        /* if file output enabled write message */
        if (mctdata->ovalue) {
            iov[0].iov_base = message->headerbuffer;
            iov[0].iov_len = (uint32_t)message->headersize;
            iov[1].iov_base = message->databuffer;
            iov[1].iov_len = (uint32_t)message->datasize;

            if (mctdata->climit > -1) {
                uint32_t bytes_to_write = message->headersize + message->datasize;

                if ((bytes_to_write + mctdata->totalbytes > mctdata->climit)) {
                    mct_receive_close_output_file(mctdata);

                    if (mct_receive_open_output_file(mctdata) < 0) {
                        printf(
                            "ERROR: mct_receive_message_callback: Unable to open log when maximum filesize was reached!\n");
                        return -1;
                    }

                    mctdata->totalbytes = 0;
                }
            }

            bytes_written = (int)writev(mctdata->ohandle, iov, 2);

            mctdata->totalbytes += bytes_written;

            if (0 > bytes_written) {
                printf("mct_receive_message_callback: writev(mctdata->ohandle, iov, 2); returned an error!");
                return -1;
            }
        }
    }

    return 0;
}
