#include <netdb.h>
#include <ctype.h>
#include <stdio.h>      /* for printf() and fprintf() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */

#include "mct.h"
#include "mct_common.h" /* for mct_get_version() */

int mct_user_injection_callback(uint32_t service_id, void *data, uint32_t length);
int mct_user_injection_callback_with_specific_data(uint32_t service_id, void *data, uint32_t length, void *priv_data);

void mct_user_log_level_changed_callback(char context_id[MCT_ID_SIZE], uint8_t log_level, uint8_t trace_status);

MCT_DECLARE_CONTEXT(mycontext1)
MCT_DECLARE_CONTEXT(mycontext2)
MCT_DECLARE_CONTEXT(mycontext3)

/**
 * Print usage information of tool.
 */
void usage()
{
    char version[255];

    mct_get_version(version, 255);

    printf("Usage: mct-log-writer [options] message\n");
    printf("Generate MCT messages and store them to file or send them to daemon.\n");
    printf("%s \n", version);
    printf("Options:\n");
    printf("  -d delay      Milliseconds to wait between sending messages (Default: 500)\n");
    printf("  -f filename   Use local log file instead of sending to daemon\n");
    printf("  -S filesize   Set maximum size of local log file (Default: UINT_MAX)\n");
    printf("  -n count      Number of messages to be generated (Default: 10)\n");
    printf("  -g            Switch to non-verbose mode (Default: verbose mode)\n");
    printf("  -a            Enable local printing of MCT messages (Default: disabled)\n");
    printf("  -k            Send marker message\n");
    printf("  -m mode       Set log mode 0=off, 1=external, 2=internal, 3=both\n");
    printf("  -l level      Set log level to <level>, level=-1..6\n");
    printf("  -C ContextID  Set context ID for send message (Default: TEST)\n");
    printf("  -A AppID      Set app ID for send message (Default: LOG)\n");
    printf("  -t timeout    Set timeout when sending messages at exit, in ms (Default: 10000 = 10sec)\n");
    printf("  -r size       Send raw data with specified size instead of string\n");
#ifdef MCT_TEST_ENABLE
    printf("  -c            Corrupt user header\n");
    printf("  -s size       Corrupt message size\n");
    printf("  -z size          Size of message\n");
#endif /* MCT_TEST_ENABLE */
}

/**
 * Main function of tool.
 */
int main(int argc, char *argv[])
{
    int gflag = 0;
    int aflag = 0;
    int kflag = 0;
#ifdef MCT_TEST_ENABLE
    int cflag = 0;
    char *svalue = 0;
    char *zvalue = 0;
#endif /* MCT_TEST_ENABLE */
    char *dvalue = 0;
    char *fvalue = 0;
    unsigned int filesize = 0;
    char *nvalue = 0;
    char *mvalue = 0;
    char *message = 0;
    int lvalue = MCT_LOG_WARN;
    char *tvalue = 0;
    int rvalue = -1;
    int index;
    int c;

    char *appID = "LOG";
    char *contextID = "TEST";

    char *text;
    int num, maxnum;
    int delay;
    struct timespec ts;

    int state = -1, newstate;

    opterr = 0;
#ifdef MCT_TEST_ENABLE

    while ((c = getopt (argc, argv, "vgakcd:f:S:n:m:z:r:s:l:t:A:C:")) != -1)
#else

    while ((c = getopt (argc, argv, "vgakd:f:S:n:m:l:r:t:A:C:")) != -1)
#endif /* MCT_TEST_ENABLE */
    {
        switch (c) {
        case 'g':
        {
            gflag = 1;
            break;
        }
        case 'a':
        {
            aflag = 1;
            break;
        }
        case 'k':
        {
            kflag = 1;
            break;
        }
#ifdef MCT_TEST_ENABLE
        case 'c':
        {
            cflag = 1;
            break;
        }
        case 's':
        {
            svalue = optarg;
            break;
        }
        case 'z':
        {
            zvalue = optarg;
            break;
        }
#endif /* MCT_TEST_ENABLE */
        case 'd':
        {
            dvalue = optarg;
            break;
        }
        case 'f':
        {
            fvalue = optarg;
            break;
        }
        case 'S':
        {
            filesize = atoi(optarg);
            break;
        }
        case 'n':
        {
            nvalue = optarg;
            break;
        }
        case 'm':
        {
            mvalue = optarg;
            break;
        }
        case 'l':
        {
            lvalue = atoi(optarg);
            break;
        }
        case 'A':
        {
            appID = optarg;
            break;
        }
        case 'C':
        {
            contextID = optarg;
            break;
        }
        case 't':
        {
            tvalue = optarg;
            break;
        }
        case 'r':
        {
            rvalue = atoi(optarg);
            break;
        }
        case '?':
        {
            if ((optopt == 'd') || (optopt == 'f') || (optopt == 'n') ||
                (optopt == 'l') || (optopt == 't') || (optopt == 'S'))
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
            break;/*for parasoft */
        }
        }
    }

    if (rvalue == -1) {
        for (index = optind; index < argc; index++)
            message = argv[index];
    }
    else { /* allocate raw buffer */
        message = calloc(sizeof(char), rvalue);
        memset(message, 'X', rvalue - 1);
    }

    if (message == 0) {
        /* no message, show usage and terminate */
        fprintf(stderr, "ERROR: No message selected\n");
        usage();
        return -1;
    }

    if (fvalue) {
        /* MCT is initialized automatically, except another output target will be used */
        if (mct_init_file(fvalue) < 0) /* log to file */
            return -1;
    }

    if (filesize != 0) {
        if (mct_set_filesize_max(filesize) < 0)
            return -1;
    }

    mct_with_session_id(1);
    mct_with_timestamp(1);
    mct_with_ecu_id(1);
    mct_verbose_mode();

    MCT_REGISTER_APP(appID, "Test Application for Logging");
    MCT_REGISTER_CONTEXT(mycontext1, contextID, "Test Context for Logging");
    MCT_REGISTER_CONTEXT_LLCCB(mycontext2, "TS1", "Test Context1 for injection", mct_user_log_level_changed_callback);
    MCT_REGISTER_CONTEXT_LLCCB(mycontext3, "TS2", "Test Context2 for injection", mct_user_log_level_changed_callback);


    MCT_REGISTER_INJECTION_CALLBACK(mycontext1, 0x1000, mct_user_injection_callback);
    MCT_REGISTER_INJECTION_CALLBACK_WITH_ID(mycontext2,
                                            0x1000,
                                            mct_user_injection_callback_with_specific_data,
                                            (void *)"TS1 context");
    MCT_REGISTER_INJECTION_CALLBACK(mycontext2, 0x1001, mct_user_injection_callback);
    MCT_REGISTER_INJECTION_CALLBACK_WITH_ID(mycontext3,
                                            0x1000,
                                            mct_user_injection_callback_with_specific_data,
                                            (void *)"TS2 context");
    MCT_REGISTER_INJECTION_CALLBACK(mycontext3, 0x1001, mct_user_injection_callback);
    MCT_REGISTER_LOG_LEVEL_CHANGED_CALLBACK(mycontext1, mct_user_log_level_changed_callback);

    text = message;

    if (mvalue) {
        printf("Log mode is not supported anymore.\n");
        printf("Use a filter configuration file and mct-filter-ctrl app to\n");
        printf("set/get the current filter level.\n");
        mct_set_log_mode(atoi(mvalue));
    }

    if (gflag)
        MCT_NONVERBOSE_MODE();

    if (aflag)
        MCT_ENABLE_LOCAL_PRINT();

    if (kflag)
        MCT_LOG_MARKER();

    if (nvalue)
        maxnum = atoi(nvalue);
    else
        maxnum = 10;

    if (dvalue)
        delay = atoi(dvalue);
    else
        delay = 500;

    if (tvalue)
        mct_set_resend_timeout_atexit(atoi(tvalue));

    if (gflag) {
        /* MCT messages to test Fibex non-verbose description: mct-example-non-verbose.xml */
        MCT_LOG_ID(mycontext1, MCT_LOG_INFO, 10);
        MCT_LOG_ID(mycontext1, MCT_LOG_INFO, 11, MCT_UINT16(1011));
        MCT_LOG_ID(mycontext1, MCT_LOG_INFO, 12, MCT_UINT32(1012), MCT_UINT32(1013));
        MCT_LOG_ID(mycontext1, MCT_LOG_INFO, 13, MCT_UINT8(123), MCT_FLOAT32(1.12));
        MCT_LOG_ID(mycontext1, MCT_LOG_INFO, 14, MCT_STRING("DEAD BEEF"));
    }

#ifdef MCT_TEST_ENABLE

    if (cflag)
        mct_user_test_corrupt_user_header(1);

    if (svalue)
        mct_user_test_corrupt_message_size(1, atoi(svalue));

    if (zvalue) {
        char *buffer = malloc(atoi(zvalue));

        if (buffer == 0) {
            /* no message, show usage and terminate */
            fprintf(stderr, "Cannot allocate buffer memory!\n");
            return -1;
        }

        MCT_LOG(mycontext1, MCT_LOG_WARN, MCT_STRING(text), MCT_RAW(buffer, atoi(zvalue)));
        free(buffer);
    }

#endif /* MCT_TEST_ENABLE */

    for (num = 0; num < maxnum; num++) {
        printf("Send %d %s\n", num, text);

        newstate = mct_get_log_state();

        if (state != newstate) {
            state = newstate;

            if (state == -1)
                printf("Client unknown state!\n");
            else if (state == 0)
                printf("Client disconnected!\n");
            else if (state == 1)
                printf("Client connected!\n");
        }

        if (gflag) {
            /* Non-verbose mode */
            MCT_LOG_ID(mycontext1, lvalue, num, MCT_INT(num), MCT_STRING(text));
        }
        else {
            if (rvalue == -1)
                /* Verbose mode */
                MCT_LOG(mycontext1, lvalue, MCT_INT(num), MCT_STRING(text));
            else
                MCT_LOG(mycontext1, lvalue, MCT_RAW(text, rvalue));
        }

        if (delay > 0) {
            ts.tv_sec = delay / 1000;
            ts.tv_nsec = (delay % 1000) * 1000000;
            nanosleep(&ts, NULL);
        }
    }

    sleep(1);

    MCT_UNREGISTER_CONTEXT(mycontext1);

    MCT_UNREGISTER_APP();

    return 0;

}

int mct_user_injection_callback(uint32_t service_id, void *data, uint32_t length)
{
    char text[1024];

    MCT_LOG(mycontext1, MCT_LOG_INFO, MCT_STRING("Injection: "), MCT_UINT32(service_id));
    printf("Injection %d, Length=%d \n", service_id, length);

    if (length > 0) {
        mct_print_mixed_string(text, 1024, data, length, 0);
        MCT_LOG(mycontext1, MCT_LOG_INFO, MCT_STRING("Data: "), MCT_STRING(text));
        printf("%s \n", text);
    }

    return 0;
}

int mct_user_injection_callback_with_specific_data(uint32_t service_id, void *data, uint32_t length, void *priv_data)
{
    char text[1024];

    MCT_LOG(mycontext1, MCT_LOG_INFO, MCT_STRING("Injection: "), MCT_UINT32(service_id));
    printf("Injection %d, Length=%d \n", service_id, length);

    if (length > 0) {
        mct_print_mixed_string(text, 1024, data, length, 0);
        MCT_LOG(mycontext1, MCT_LOG_INFO, MCT_STRING("Data: "), MCT_STRING(text), MCT_STRING(priv_data));
        printf("%s \n", text);
    }

    return 0;
}

void mct_user_log_level_changed_callback(char context_id[MCT_ID_SIZE], uint8_t log_level, uint8_t trace_status)
{
    char text[5];
    text[4] = 0;

    memcpy(text, context_id, MCT_ID_SIZE);

    printf("Log level changed of context %s, LogLevel=%u, TraceState=%u \n", text, log_level, trace_status);
}

