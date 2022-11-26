#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h> /* send() */

#include "mct_types.h"
#include "mct_daemon_common.h"
#include "mct_daemon_common_cfg.h"
#include "mct_user_shared.h"
#include "mct_user_shared_cfg.h"
#include "mct-daemon.h"

#include "mct_daemon_socket.h"
#include "mct_daemon_serial.h"

char *app_recv_buffer = NULL; /* pointer to receiver buffer for application msges */

static int mct_daemon_cmp_apid(const void *m1, const void *m2)
{
    if ((m1 == NULL) || (m2 == NULL)) {
        return -1;
    }

    MctDaemonApplication *mi1 = (MctDaemonApplication *)m1;
    MctDaemonApplication *mi2 = (MctDaemonApplication *)m2;

    return memcmp(mi1->apid, mi2->apid, MCT_ID_SIZE);
}

static int mct_daemon_cmp_apid_ctid(const void *m1, const void *m2)
{
    if ((m1 == NULL) || (m2 == NULL)) {
        return -1;
    }

    int ret, cmp;
    MctDaemonContext *mi1 = (MctDaemonContext *)m1;
    MctDaemonContext *mi2 = (MctDaemonContext *)m2;

    cmp = memcmp(mi1->apid, mi2->apid, MCT_ID_SIZE);

    if (cmp < 0) {
        ret = -1;
    } else if (cmp == 0) {
        ret = memcmp(mi1->ctid, mi2->ctid, MCT_ID_SIZE);
    } else {
        ret = 1;
    }

    return ret;
}

MctDaemonRegisteredUsers *mct_daemon_find_users_list(MctDaemon *daemon,
                                                     char *ecu,
                                                     int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    int i = 0;

    if ((daemon == NULL) || (ecu == NULL)) {
        mct_vlog(LOG_ERR, "%s: Wrong parameters", __func__);
        return (MctDaemonRegisteredUsers *)NULL;
    }

    for (i = 0; i < daemon->num_user_lists; i++) {
        if (strncmp(ecu, daemon->user_list[i].ecu, MCT_ID_SIZE) == 0) {
            return &daemon->user_list[i];
        }
    }

    mct_vlog(LOG_ERR, "Cannot find user list for ECU: %4s\n", ecu);
    return (MctDaemonRegisteredUsers *)NULL;
}
int mct_daemon_init_runtime_configuration(MctDaemon *daemon,
                                          const char *runtime_directory,
                                          int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);
    int append_length = 0;

    if (daemon == NULL) {
        return MCT_RETURN_ERROR;
    }

    /* Default */
    daemon->mode = MCT_USER_MODE_EXTERNAL;

    if (runtime_directory == NULL) {
        return MCT_RETURN_ERROR;
    }

    /* prepare filenames for configuration */
    append_length = PATH_MAX - sizeof(MCT_RUNTIME_APPLICATION_CFG);

    if (runtime_directory[0]) {
        strncpy(daemon->runtime_application_cfg, runtime_directory, append_length);
        daemon->runtime_application_cfg[append_length] = 0;
    } else {
        strncpy(daemon->runtime_application_cfg, MCT_RUNTIME_DEFAULT_DIRECTORY, append_length);
        daemon->runtime_application_cfg[append_length] = 0;
    }

    strcat(daemon->runtime_application_cfg, MCT_RUNTIME_APPLICATION_CFG); /* strcat uncritical here, because max length already checked */

    append_length = PATH_MAX - sizeof(MCT_RUNTIME_CONTEXT_CFG);

    if (runtime_directory[0]) {
        strncpy(daemon->runtime_context_cfg, runtime_directory, append_length);
        daemon->runtime_context_cfg[append_length] = 0;
    } else {
        strncpy(daemon->runtime_context_cfg, MCT_RUNTIME_DEFAULT_DIRECTORY, append_length);
        daemon->runtime_context_cfg[append_length] = 0;
    }

    strcat(daemon->runtime_context_cfg, MCT_RUNTIME_CONTEXT_CFG); /* strcat uncritical here, because max length already checked */

    append_length = PATH_MAX - sizeof(MCT_RUNTIME_CONFIGURATION);

    if (runtime_directory[0]) {
        strncpy(daemon->runtime_configuration, runtime_directory, append_length);
        daemon->runtime_configuration[append_length] = 0;
    } else {
        strncpy(daemon->runtime_configuration, MCT_RUNTIME_DEFAULT_DIRECTORY, append_length);
        daemon->runtime_configuration[append_length] = 0;
    }

    strcat(daemon->runtime_configuration, MCT_RUNTIME_CONFIGURATION); /* strcat uncritical here, because max length already checked */

    return MCT_RETURN_OK;
}

int mct_daemon_init(MctDaemon *daemon,
                    unsigned long RingbufferMinSize,
                    unsigned long RingbufferMaxSize,
                    unsigned long RingbufferStepSize,
                    const char *runtime_directory,
                    int InitialContextLogLevel,
                    int InitialContextTraceStatus,
                    int ForceLLTS,
                    int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (runtime_directory == NULL)) {
        return -1;
    }

    daemon->user_list = NULL;
    daemon->num_user_lists = 0;

    daemon->default_log_level = InitialContextLogLevel;
    daemon->default_trace_status = InitialContextTraceStatus;
    daemon->force_ll_ts = ForceLLTS;

    daemon->overflow_counter = 0;

    daemon->runtime_context_cfg_loaded = 0;

    daemon->connectionState = 0; /* no logger connected */

    daemon->state = MCT_DAEMON_STATE_INIT; /* initial logging state */

    daemon->sendserialheader = 0;
    daemon->timingpackets = 0;

    mct_set_id(daemon->ecuid, "");

    /* initialize ring buffer for client connection */
    mct_vlog(LOG_INFO, "Ringbuffer configuration: %lu/%lu/%lu\n",
             RingbufferMinSize, RingbufferMaxSize, RingbufferStepSize);

    if (mct_buffer_init_dynamic(
            &(daemon->client_ringbuffer),
            RingbufferMinSize,
            RingbufferMaxSize,
            RingbufferStepSize) < MCT_RETURN_OK) {
        return -1;
    }

    daemon->storage_handle = NULL;
    return 0;
}

int mct_daemon_free(MctDaemon *daemon, int verbose)
{
    int i = 0;
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon->user_list == NULL)) {
        return -1;
    }

    /* free all registered user information */
    for (i = 0; i < daemon->num_user_lists; i++) {
        user_list = &daemon->user_list[i];

        if (user_list != NULL) {
            /* ignore return values */
            mct_daemon_contexts_clear(daemon, user_list->ecu, verbose);
            mct_daemon_applications_clear(daemon, user_list->ecu, verbose);
        }
    }

    free(daemon->user_list);

    if (app_recv_buffer) {
        free(app_recv_buffer);
    }

    /* free ringbuffer */
    mct_buffer_free_dynamic(&(daemon->client_ringbuffer));

    return 0;
}

int mct_daemon_applications_invalidate_fd(MctDaemon *daemon,
                                          char *ecu,
                                          int fd,
                                          int verbose)
{
    int i;
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (ecu == NULL)) {
        return MCT_RETURN_ERROR;
    }

    user_list = mct_daemon_find_users_list(daemon, ecu, verbose);

    if (user_list != NULL) {
        for (i = 0; i < user_list->num_applications; i++) {
            if (user_list->applications[i].user_handle == fd) {
                user_list->applications[i].user_handle = MCT_FD_INIT;
            }
        }

        return MCT_RETURN_OK;
    }

    return MCT_RETURN_ERROR;
}

int mct_daemon_applications_clear(MctDaemon *daemon, char *ecu, int verbose)
{
    int i;
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon->user_list == NULL) || (ecu == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    user_list = mct_daemon_find_users_list(daemon, ecu, verbose);

    if (user_list == NULL) {
        return MCT_RETURN_ERROR;
    }

    for (i = 0; i < user_list->num_applications; i++) {
        if (user_list->applications[i].application_description != NULL) {
            free(user_list->applications[i].application_description);
            user_list->applications[i].application_description = NULL;
        }
    }

    if (user_list->applications != NULL) {
        free(user_list->applications);
    }

    user_list->applications = NULL;
    user_list->num_applications = 0;

    return 0;
}

static void mct_daemon_application_reset_user_handle(MctDaemon *daemon,
                                                     MctDaemonApplication *application,
                                                     int verbose)
{
    MctDaemonRegisteredUsers *user_list;
    MctDaemonContext *context;
    int i;

    if (application->user_handle == MCT_FD_INIT) {
        return;
    }

    user_list = mct_daemon_find_users_list(daemon, daemon->ecuid, verbose);

    if (user_list != NULL) {
        for (i = 0; i < user_list->num_contexts; i++) {
            context = &user_list->contexts[i];

            if (context->user_handle == application->user_handle) {
                context->user_handle = MCT_FD_INIT;
            }
        }
    }

    if (application->owns_user_handle) {
        close(application->user_handle);
    }

    application->user_handle = MCT_FD_INIT;
    application->owns_user_handle = false;
}

MctDaemonApplication *mct_daemon_application_add(MctDaemon *daemon,
                                                 char *apid,
                                                 pid_t pid,
                                                 char *description,
                                                 int fd,
                                                 char *ecu,
                                                 int verbose)
{
    MctDaemonApplication *application;
    MctDaemonApplication *old;
    int new_application;
    int mct_user_handle;
    bool owns_user_handle;
    MctDaemonRegisteredUsers *user_list = NULL;
#ifdef MCT_DAEMON_USE_FIFO_IPC
    (void)fd;  /* To avoid compiler warning : unused variable */
    char filename[MCT_DAEMON_COMMON_TEXTBUFSIZE];
#endif

    if ((daemon == NULL) || (apid == NULL) || (apid[0] == '\0') || (ecu == NULL)) {
        return (MctDaemonApplication *)NULL;
    }

    user_list = mct_daemon_find_users_list(daemon, ecu, verbose);

    if (user_list == NULL) {
        return (MctDaemonApplication *)NULL;
    }

    if (user_list->applications == NULL) {
        user_list->applications = (MctDaemonApplication *)
            malloc(sizeof(MctDaemonApplication) * MCT_DAEMON_APPL_ALLOC_SIZE);

        if (user_list->applications == NULL) {
            return (MctDaemonApplication *)NULL;
        }
    }

    new_application = 0;

    /* Check if application [apid] is already available */
    application = mct_daemon_application_find(daemon, apid, ecu, verbose);

    if (application == NULL) {
        user_list->num_applications += 1;

        if (user_list->num_applications != 0) {
            if ((user_list->num_applications % MCT_DAEMON_APPL_ALLOC_SIZE) == 0) {
                /* allocate memory in steps of MCT_DAEMON_APPL_ALLOC_SIZE, e.g. 100 */
                old = user_list->applications;
                user_list->applications = (MctDaemonApplication *)
                    malloc(sizeof(MctDaemonApplication) *
                           ((user_list->num_applications / MCT_DAEMON_APPL_ALLOC_SIZE) + 1) *
                           MCT_DAEMON_APPL_ALLOC_SIZE);

                if (user_list->applications == NULL) {
                    user_list->applications = old;
                    user_list->num_applications -= 1;
                    return (MctDaemonApplication *)NULL;
                }

                memcpy(user_list->applications,
                       old,
                       sizeof(MctDaemonApplication) * user_list->num_applications);
                free(old);
            }
        }

        application = &(user_list->applications[user_list->num_applications - 1]);

        mct_set_id(application->apid, apid);
        application->pid = 0;
        application->application_description = NULL;
        application->num_contexts = 0;
        application->user_handle = MCT_FD_INIT;
        application->blockmode_status = MCT_MODE_NON_BLOCKING;
        application->owns_user_handle = false;

        new_application = 1;
    } else if ((pid != application->pid) && (application->pid != 0)) {

        mct_vlog(
            LOG_WARNING,
            "Duplicate registration of ApplicationID: '%.4s'; registering from PID %d, existing from PID %d\n",
            apid,
            pid,
            application->pid);
    }

    /* Store application description and pid of application */
    if (application->application_description) {
        free(application->application_description);
        application->application_description = NULL;
    }

    if (description != NULL) {
        application->application_description = malloc(strlen(description) + 1);

        if (application->application_description) {
            memcpy(application->application_description, description, strlen(description) + 1);
        } else {
            mct_log(LOG_ERR, "Cannot allocate memory to store application description\n");
            free(application);
            return (MctDaemonApplication *)NULL;
        }
    }

    if (application->pid != pid) {
        mct_daemon_application_reset_user_handle(daemon, application, verbose);
        application->pid = 0;
    }

    /* open user pipe only if it is not yet opened */
    if ((application->user_handle == MCT_FD_INIT) && (pid != 0)) {
        mct_user_handle = MCT_FD_INIT;
        owns_user_handle = false;

#if defined MCT_DAEMON_USE_UNIX_SOCKET_IPC
        if (fd >= MCT_FD_MINIMUM) {
            mct_user_handle = fd;
            owns_user_handle = false;
        }

#endif
#ifdef MCT_DAEMON_USE_FIFO_IPC

        if (mct_user_handle < MCT_FD_MINIMUM) {
            snprintf(filename,
                     MCT_DAEMON_COMMON_TEXTBUFSIZE,
                     "%s/mctpipes/mct%d",
                     mctFifoBaseDir,
                     pid);

            mct_user_handle = open(filename, O_WRONLY | O_NONBLOCK);

            if (mct_user_handle < 0) {
                int prio = (errno == ENOENT) ? LOG_INFO : LOG_WARNING;
                mct_vlog(prio, "open() failed to %s, errno=%d (%s)!\n", filename, errno,
                         strerror(errno));
            } else {
                owns_user_handle = true;
            }
        }

#endif
        /* check if file descriptor was already used, and make it invalid if it
        * is reused. This prevents sending messages to wrong file descriptor */
        mct_daemon_applications_invalidate_fd(daemon, ecu, mct_user_handle, verbose);
        mct_daemon_contexts_invalidate_fd(daemon, ecu, mct_user_handle, verbose);

        application->user_handle = mct_user_handle;
        application->owns_user_handle = owns_user_handle;
        application->pid = pid;
    }

    /* Sort */
    if (new_application) {
        qsort(user_list->applications,
              user_list->num_applications,
              sizeof(MctDaemonApplication),
              mct_daemon_cmp_apid);

        /* Find new position of application with apid*/
        application = mct_daemon_application_find(daemon, apid, ecu, verbose);
    }

    return application;
}

int mct_daemon_application_del(MctDaemon *daemon,
                               MctDaemonApplication *application,
                               char *ecu,
                               int verbose)
{
    int pos;
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (application == NULL) || (ecu == NULL)) {
        return -1;
    }

    user_list = mct_daemon_find_users_list(daemon, ecu, verbose);

    if (user_list == NULL) {
        return -1;
    }

    if (user_list->num_applications > 0) {
        mct_daemon_application_reset_user_handle(daemon, application, verbose);

        /* Free description of application to be deleted */
        if (application->application_description) {
            free(application->application_description);
            application->application_description = NULL;
        }

        pos = application - (user_list->applications);

        /* move all applications above pos to pos */
        memmove(&(user_list->applications[pos]),
                &(user_list->applications[pos + 1]),
                sizeof(MctDaemonApplication) * ((user_list->num_applications - 1) - pos));

        /* Clear last application */
        memset(&(user_list->applications[user_list->num_applications - 1]),
               0,
               sizeof(MctDaemonApplication));

        user_list->num_applications--;
    }

    return 0;
}

MctDaemonApplication *mct_daemon_application_find(MctDaemon *daemon,
                                                  char *apid,
                                                  char *ecu,
                                                  int verbose)
{
    MctDaemonApplication application;
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (daemon->user_list == NULL) || (apid == NULL) ||
        (apid[0] == '\0') || (ecu == NULL)) {
        return (MctDaemonApplication *)NULL;
    }

    user_list = mct_daemon_find_users_list(daemon, ecu, verbose);

    if ((user_list == NULL) || (user_list->num_applications == 0)) {
        return (MctDaemonApplication *)NULL;
    }

    /* Check, if apid is smaller than smallest apid or greater than greatest apid */
    if ((memcmp(apid, user_list->applications[0].apid, MCT_ID_SIZE) < 0) ||
        (memcmp(apid,
                user_list->applications[user_list->num_applications - 1].apid,
                MCT_ID_SIZE) > 0)) {
        return (MctDaemonApplication *)NULL;
    }

    mct_set_id(application.apid, apid);
    return (MctDaemonApplication *)bsearch(&application,
                                           user_list->applications,
                                           user_list->num_applications,
                                           sizeof(MctDaemonApplication),
                                           mct_daemon_cmp_apid);
}

int mct_daemon_applications_load(MctDaemon *daemon, const char *filename, int verbose)
{
    FILE *fd;
    ID4 apid;
    char buf[MCT_DAEMON_COMMON_TEXTBUFSIZE];
    char *ret;
    char *pb;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (filename == NULL) || (filename[0] == '\0')) {
        return -1;
    }

    fd = fopen(filename, "r");

    if (fd == NULL) {
        mct_vlog(LOG_WARNING,
                 "%s: cannot open file %s: %s\n",
                 __func__,
                 filename,
                 strerror(errno));

        return -1;
    }

    while (!feof(fd)) {
        /* Clear buf */
        memset(buf, 0, sizeof(buf));

        /* Get line */
        ret = fgets(buf, sizeof(buf), fd);

        if (NULL == ret) {
            /* fgets always null pointer if the last byte of the file is a new line
             * We need to check here if there was an error or was it feof.*/
            if (ferror(fd)) {
                mct_vlog(LOG_WARNING,
                         "%s: fgets(buf,sizeof(buf),fd) returned NULL. %s\n",
                         __func__,
                         strerror(errno));
                fclose(fd);
                return -1;
            } else if (feof(fd)) {
                fclose(fd);
                return 0;
            } else {
                mct_vlog(LOG_WARNING,
                         "%s: fgets(buf,sizeof(buf),fd) returned NULL. Unknown error.\n",
                         __func__);
                fclose(fd);
                return -1;
            }
        }

        if (strcmp(buf, "") != 0) {
            /* Split line */
            pb = strtok(buf, ":");

            if (pb != NULL) {
                mct_set_id(apid, pb);
                pb = strtok(NULL, ":");

                if (pb != NULL) {
                    /* pb contains now the description */
                    /* pid is unknown at loading time */
                    if (mct_daemon_application_add(daemon,
                                                   apid,
                                                   0,
                                                   pb,
                                                   -1,
                                                   daemon->ecuid,
                                                   verbose) == 0) {
                        mct_vlog(LOG_WARNING,
                                 "%s: mct_daemon_application_add failed for %4s\n",
                                 __func__,
                                 apid);
                        fclose(fd);
                        return -1;
                    }
                }
            }
        }
    }

    fclose(fd);

    return 0;
}

int mct_daemon_applications_save(MctDaemon *daemon, const char *filename, int verbose)
{
    FILE *fd;
    int i;

    char apid[MCT_ID_SIZE + 1]; /* MCT_ID_SIZE+1, because the 0-termination is required here */
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (filename == NULL) || (filename[0] == '\0')) {
        return -1;
    }

    memset(apid, 0, sizeof(apid));

    user_list = mct_daemon_find_users_list(daemon, daemon->ecuid, verbose);

    if (user_list == NULL) {
        return -1;
    }

    if ((user_list->applications != NULL) && (user_list->num_applications > 0)) {
        fd = fopen(filename, "w");

        if (fd != NULL) {
            for (i = 0; i < user_list->num_applications; i++) {
                mct_set_id(apid, user_list->applications[i].apid);

                if ((user_list->applications[i].application_description) &&
                    (user_list->applications[i].application_description[0] != '\0')) {
                    fprintf(fd,
                            "%s:%s:\n",
                            apid,
                            user_list->applications[i].application_description);
                } else {
                    fprintf(fd, "%s::\n", apid);
                }
            }

            fclose(fd);
        } else {
            mct_vlog(LOG_ERR, "%s: open %s failed! No application information stored.\n",
                     __func__,
                     filename);
        }
    }

    return 0;
}

MctDaemonContext *mct_daemon_context_add(MctDaemon *daemon,
                                         char *apid,
                                         char *ctid,
                                         int8_t log_level,
                                         int8_t trace_status,
                                         int log_level_pos,
                                         int user_handle,
                                         char *description,
                                         char *ecu,
                                         int verbose)
{
    MctDaemonApplication *application;
    MctDaemonContext *context;
    MctDaemonContext *old;
    int new_context = 0;
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (apid == NULL) || (apid[0] == '\0') ||
        (ctid == NULL) || (ctid[0] == '\0') || (ecu == NULL)) {
        return (MctDaemonContext *)NULL;
    }

    if ((log_level < MCT_LOG_DEFAULT) || (log_level > MCT_LOG_VERBOSE)) {
        return (MctDaemonContext *)NULL;
    }

    if ((trace_status < MCT_TRACE_STATUS_DEFAULT) || (trace_status > MCT_TRACE_STATUS_ON)) {
        return (MctDaemonContext *)NULL;
    }

    user_list = mct_daemon_find_users_list(daemon, ecu, verbose);

    if (user_list == NULL) {
        return (MctDaemonContext *)NULL;
    }

    if (user_list->contexts == NULL) {
        user_list->contexts = (MctDaemonContext *)malloc(
                sizeof(MctDaemonContext) * MCT_DAEMON_CONTEXT_ALLOC_SIZE);

        if (user_list->contexts == NULL) {
            return (MctDaemonContext *)NULL;
        }
    }

    /* Check if application [apid] is available */
    application = mct_daemon_application_find(daemon, apid, ecu, verbose);

    if (application == NULL) {
        return (MctDaemonContext *)NULL;
    }

    /* Check if context [apid, ctid] is already available */
    context = mct_daemon_context_find(daemon, apid, ctid, ecu, verbose);

    if (context == NULL) {
        user_list->num_contexts += 1;

        if (user_list->num_contexts != 0) {
            if ((user_list->num_contexts % MCT_DAEMON_CONTEXT_ALLOC_SIZE) == 0) {
                /* allocate memory for context in steps of MCT_DAEMON_CONTEXT_ALLOC_SIZE, e.g 100 */
                old = user_list->contexts;
                user_list->contexts = (MctDaemonContext *)malloc(sizeof(MctDaemonContext) *
                                                                 ((user_list->num_contexts /
                                                                   MCT_DAEMON_CONTEXT_ALLOC_SIZE) +
                                                                  1) *
                                                                 MCT_DAEMON_CONTEXT_ALLOC_SIZE);

                if (user_list->contexts == NULL) {
                    user_list->contexts = old;
                    user_list->num_contexts -= 1;
                    return (MctDaemonContext *)NULL;
                }

                memcpy(user_list->contexts,
                       old,
                       sizeof(MctDaemonContext) * user_list->num_contexts);
                free(old);
            }
        }

        context = &(user_list->contexts[user_list->num_contexts - 1]);
        memset(context, 0, sizeof(MctDaemonContext));

        mct_set_id(context->apid, apid);
        mct_set_id(context->ctid, ctid);

        application->num_contexts++;
        new_context = 1;
    }

    /* Set context description */
    if (context->context_description) {
        free(context->context_description);
        context->context_description = NULL;
    }

    if (description != NULL) {
        context->context_description = malloc(strlen(description) + 1);

        if (context->context_description) {
            memcpy(context->context_description, description, strlen(description) + 1);
        }
    }

    if ((strncmp(daemon->ecuid, ecu, MCT_ID_SIZE) == 0) && (daemon->force_ll_ts)) {
        if (log_level > daemon->default_log_level) {
            log_level = daemon->default_log_level;
        }

        if (trace_status > daemon->default_trace_status) {
            trace_status = daemon->default_trace_status;
        }

        mct_vlog(LOG_NOTICE,
                 "Adapting ll_ts for context: %.4s:%.4s with %i %i\n",
                 apid,
                 ctid,
                 log_level,
                 trace_status);
    }

    /* Store log level and trace status,
     * if this is a new context, or
     * if this is an old context and the runtime cfg was not loaded */
    if ((new_context == 1) ||
        ((new_context == 0) && (daemon->runtime_context_cfg_loaded == 0))) {
        context->log_level = log_level;
        context->trace_status = trace_status;
    }

    context->log_level_pos = log_level_pos;
    context->user_handle = user_handle;

    /* In case a context is loaded from runtime config file,
     * the user_handle is 0 and we mark that context as predefined.
     */
    if (context->user_handle == 0) {
        context->predefined = true;
    } else {
        context->predefined = false;
    }

    /* Sort */
    if (new_context) {
        qsort(user_list->contexts,
              user_list->num_contexts,
              sizeof(MctDaemonContext),
              mct_daemon_cmp_apid_ctid);

        /* Find new position of context with apid, ctid */
        context = mct_daemon_context_find(daemon, apid, ctid, ecu, verbose);
    }

    return context;
}

int mct_daemon_context_del(MctDaemon *daemon,
                           MctDaemonContext *context,
                           char *ecu,
                           int verbose)
{
    int pos;
    MctDaemonApplication *application;
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (context == NULL) || (ecu == NULL)) {
        return -1;
    }

    user_list = mct_daemon_find_users_list(daemon, ecu, verbose);

    if (user_list == NULL) {
        return -1;
    }

    if (user_list->num_contexts > 0) {
        application = mct_daemon_application_find(daemon, context->apid, ecu, verbose);

        /* Free description of context to be deleted */
        if (context->context_description) {
            free(context->context_description);
            context->context_description = NULL;
        }

        pos = context - (user_list->contexts);

        /* move all contexts above pos to pos */
        memmove(&(user_list->contexts[pos]),
                &(user_list->contexts[pos + 1]),
                sizeof(MctDaemonContext) * ((user_list->num_contexts - 1) - pos));

        /* Clear last context */
        memset(&(user_list->contexts[user_list->num_contexts - 1]),
               0,
               sizeof(MctDaemonContext));

        user_list->num_contexts--;

        /* Check if application [apid] is available */
        if (application != NULL) {
            application->num_contexts--;
        }
    }

    return 0;
}

MctDaemonContext *mct_daemon_context_find(MctDaemon *daemon,
                                          char *apid,
                                          char *ctid,
                                          char *ecu,
                                          int verbose)
{
    MctDaemonContext context;
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (apid == NULL) || (apid[0] == '\0') ||
        (ctid == NULL) || (ctid[0] == '\0') || (ecu == NULL)) {
        return (MctDaemonContext *)NULL;
    }

    user_list = mct_daemon_find_users_list(daemon, ecu, verbose);

    if ((user_list == NULL) || (user_list->num_contexts == 0)) {
        return (MctDaemonContext *)NULL;
    }

    /* Check, if apid is smaller than smallest apid or greater than greatest apid */
    if ((memcmp(apid, user_list->contexts[0].apid, MCT_ID_SIZE) < 0) ||
        (memcmp(apid,
                user_list->contexts[user_list->num_contexts - 1].apid,
                MCT_ID_SIZE) > 0)) {
        return (MctDaemonContext *)NULL;
    }

    mct_set_id(context.apid, apid);
    mct_set_id(context.ctid, ctid);

    return (MctDaemonContext *)bsearch(&context,
                                       user_list->contexts,
                                       user_list->num_contexts,
                                       sizeof(MctDaemonContext),
                                       mct_daemon_cmp_apid_ctid);
}

int mct_daemon_contexts_invalidate_fd(MctDaemon *daemon,
                                      char *ecu,
                                      int fd,
                                      int verbose)
{
    int i;
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (ecu == NULL)) {
        return -1;
    }

    user_list = mct_daemon_find_users_list(daemon, ecu, verbose);

    if (user_list != NULL) {
        for (i = 0; i < user_list->num_contexts; i++) {
            if (user_list->contexts[i].user_handle == fd) {
                user_list->contexts[i].user_handle = MCT_FD_INIT;
            }
        }

        return 0;
    }

    return -1;
}

int mct_daemon_contexts_clear(MctDaemon *daemon, char *ecu, int verbose)
{
    int i;
    MctDaemonRegisteredUsers *users = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (ecu == NULL)) {
        return MCT_RETURN_WRONG_PARAMETER;
    }

    users = mct_daemon_find_users_list(daemon, ecu, verbose);

    if (users == NULL) {
        return MCT_RETURN_ERROR;
    }

    for (i = 0; i < users->num_contexts; i++) {
        if (users->contexts[i].context_description != NULL) {
            free(users->contexts[i].context_description);
            users->contexts[i].context_description = NULL;
        }
    }

    if (users->contexts) {
        free(users->contexts);
        users->contexts = NULL;
    }

    for (i = 0; i < users->num_applications; i++) {
        users->applications[i].num_contexts = 0;
    }

    users->num_contexts = 0;

    return 0;
}

int mct_daemon_contexts_load(MctDaemon *daemon, const char *filename, int verbose)
{
    FILE *fd;
    ID4 apid, ctid;
    char buf[MCT_DAEMON_COMMON_TEXTBUFSIZE];
    char *ret;
    char *pb;
    int ll, ts;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (filename == NULL) || (filename[0] == '\0')) {
        return -1;
    }

    fd = fopen(filename, "r");

    if (fd == NULL) {
        mct_vlog(LOG_WARNING,
                 "MCT runtime-context load, cannot open file %s: %s\n",
                 filename,
                 strerror(errno));

        return -1;
    }

    while (!feof(fd)) {
        /* Clear buf */
        memset(buf, 0, sizeof(buf));

        /* Get line */
        ret = fgets(buf, sizeof(buf), fd);

        if (NULL == ret) {
            /* fgets always returns null pointer if the last byte of the file is a new line.
             * We need to check here if there was an error or was it feof.*/
            if (ferror(fd)) {
                mct_vlog(LOG_WARNING,
                         "%s fgets(buf,sizeof(buf),fd) returned NULL. %s\n",
                         __func__,
                         strerror(errno));
                fclose(fd);
                return -1;
            } else if (feof(fd)) {
                fclose(fd);
                return 0;
            } else {
                mct_vlog(LOG_WARNING,
                         "%s fgets(buf,sizeof(buf),fd) returned NULL. Unknown error.\n",
                         __func__);
                fclose(fd);
                return -1;
            }
        }

        if (strcmp(buf, "") != 0) {
            /* Split line */
            pb = strtok(buf, ":");

            if (pb != NULL) {
                mct_set_id(apid, pb);
                pb = strtok(NULL, ":");

                if (pb != NULL) {
                    mct_set_id(ctid, pb);
                    pb = strtok(NULL, ":");

                    if (pb != NULL) {
                        sscanf(pb, "%d", &ll);
                        pb = strtok(NULL, ":");

                        if (pb != NULL) {
                            sscanf(pb, "%d", &ts);
                            pb = strtok(NULL, ":");

                            if (pb != NULL) {
                                /* pb contains now the description */

                                /* log_level_pos, and user_handle are unknown at loading time */
                                if (mct_daemon_context_add(daemon,
                                                           apid,
                                                           ctid,
                                                           (int8_t)ll,
                                                           (int8_t)ts,
                                                           0,
                                                           0,
                                                           pb,
                                                           daemon->ecuid,
                                                           verbose) == NULL) {
                                    mct_vlog(LOG_WARNING,
                                             "%s mct_daemon_context_add failed\n",
                                             __func__);
                                    fclose(fd);
                                    return -1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    fclose(fd);

    return 0;
}

int mct_daemon_contexts_save(MctDaemon *daemon, const char *filename, int verbose)
{
    FILE *fd;
    int i;

    char apid[MCT_ID_SIZE + 1], ctid[MCT_ID_SIZE + 1]; /* MCT_ID_SIZE+1, because the 0-termination is required here */
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (filename == NULL) || (filename[0] == '\0')) {
        return -1;
    }

    user_list = mct_daemon_find_users_list(daemon, daemon->ecuid, verbose);

    if (user_list == NULL) {
        return -1;
    }

    memset(apid, 0, sizeof(apid));
    memset(ctid, 0, sizeof(ctid));

    if ((user_list->contexts) && (user_list->num_contexts > 0)) {
        fd = fopen(filename, "w");

        if (fd != NULL) {
            for (i = 0; i < user_list->num_contexts; i++) {
                mct_set_id(apid, user_list->contexts[i].apid);
                mct_set_id(ctid, user_list->contexts[i].ctid);

                if ((user_list->contexts[i].context_description) &&
                    (user_list->contexts[i].context_description[0] != '\0')) {
                    fprintf(fd, "%s:%s:%d:%d:%s:\n", apid, ctid,
                            (int)(user_list->contexts[i].log_level),
                            (int)(user_list->contexts[i].trace_status),
                            user_list->contexts[i].context_description);
                } else {
                    fprintf(fd, "%s:%s:%d:%d::\n", apid, ctid,
                            (int)(user_list->contexts[i].log_level),
                            (int)(user_list->contexts[i].trace_status));
                }
            }

            fclose(fd);
        } else {
            mct_vlog(LOG_ERR,
                     "%s: Cannot open %s. No context information stored\n",
                     __func__,
                     filename);
        }
    }

    return 0;
}

int mct_daemon_configuration_save(MctDaemon *daemon, const char *filename, int verbose)
{
    FILE *fd;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (filename == NULL) || (filename[0] == '\0')) {
        return -1;
    }

    fd = fopen(filename, "w");

    if (fd != NULL) {
        fprintf(fd, "# 0 = off, 1 = external, 2 = internal, 3 = both\n");
        fprintf(fd, "LoggingMode = %d\n", daemon->mode);

        fclose(fd);
    }

    return 0;
}

int mct_daemon_configuration_load(MctDaemon *daemon, const char *filename, int verbose)
{
    if ((daemon == NULL) || (filename == NULL)) {
        return -1;
    }

    FILE *pFile;
    char line[1024];
    char token[1024];
    char value[1024];
    char *pch;

    PRINT_FUNCTION_VERBOSE(verbose);

    pFile = fopen (filename, "r");

    if (pFile != NULL) {
        while (1) {
            /* fetch line from configuration file */
            if (fgets (line, 1024, pFile) != NULL) {
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
                    if (strcmp(token, "LoggingMode") == 0) {
                        daemon->mode = atoi(value);
                        mct_vlog(LOG_INFO, "Runtime Option: %s=%d\n", token,
                                 daemon->mode);
                    } else {
                        mct_vlog(LOG_WARNING, "Unknown option: %s=%s\n", token,
                                 value);
                    }
                }
            } else {
                break;
            }
        }

        fclose (pFile);
    } else {
        mct_vlog(LOG_INFO, "Cannot open configuration file: %s\n", filename);
    }

    return 0;
}

int mct_daemon_user_send_log_level(MctDaemon *daemon, MctDaemonContext *context, int verbose)
{
    MctUserHeader userheader;
    MctUserControlMsgLogLevel usercontext;
    MctReturnValue ret;
    MctDaemonApplication *app;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (context == NULL)) {
        mct_vlog(LOG_ERR, "NULL parameter in %s", __func__);
        return -1;
    }

    if (mct_user_set_userheader(&userheader, MCT_USER_MESSAGE_LOG_LEVEL) < MCT_RETURN_OK) {
        mct_vlog(LOG_ERR, "Failed to set userheader in %s", __func__);
        return -1;
    }

    if ((context->storage_log_level != MCT_LOG_DEFAULT) &&
        (daemon->maintain_logstorage_loglevel != MCT_MAINTAIN_LOGSTORAGE_LOGLEVEL_OFF)) {
        usercontext.log_level = context->log_level >
            context->storage_log_level ? context->log_level : context->storage_log_level;
    } else { /* Storage log level is not updated (is DEFAULT) then  no device is yet connected so ignore */
        usercontext.log_level =
            ((context->log_level ==
              MCT_LOG_DEFAULT) ? daemon->default_log_level : context->log_level);
    }

    usercontext.trace_status =
        ((context->trace_status ==
          MCT_TRACE_STATUS_DEFAULT) ? daemon->default_trace_status : context->trace_status);

    usercontext.log_level_pos = context->log_level_pos;

    mct_vlog(LOG_NOTICE, "Send log-level to context: %.4s:%.4s [%i -> %i] [%i -> %i]\n",
             context->apid,
             context->ctid,
             context->log_level,
             usercontext.log_level,
             context->trace_status,
             usercontext.trace_status);

    /* log to FIFO */
    errno = 0;
    ret = mct_user_log_out2(context->user_handle,
                            &(userheader), sizeof(MctUserHeader),
                            &(usercontext), sizeof(MctUserControlMsgLogLevel));

    if (ret < MCT_RETURN_OK) {
        mct_vlog(LOG_ERR, "Failed to send data to application in %s: %s",
                 __func__,
                 errno != 0 ? strerror(errno) : "Unknown error");

        if (errno == EPIPE) {
            app = mct_daemon_application_find(daemon, context->apid, daemon->ecuid, verbose);

            if (app != NULL) {
                mct_daemon_application_reset_user_handle(daemon, app, verbose);
            }
        }
    }

    return (ret == MCT_RETURN_OK) ? MCT_RETURN_OK : MCT_RETURN_ERROR;
}

int mct_daemon_user_send_log_state(MctDaemon *daemon, MctDaemonApplication *app, int verbose)
{
    MctUserHeader userheader;
    MctUserControlMsgLogState logstate;
    MctReturnValue ret;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (app == NULL)) {
        return -1;
    }

    if (mct_user_set_userheader(&userheader, MCT_USER_MESSAGE_LOG_STATE) < MCT_RETURN_OK) {
        return -1;
    }

    logstate.log_state = daemon->connectionState;

    /* log to FIFO */
    ret = mct_user_log_out2(app->user_handle,
                            &(userheader), sizeof(MctUserHeader),
                            &(logstate), sizeof(MctUserControlMsgLogState));

    if (ret < MCT_RETURN_OK) {
        if (errno == EPIPE) {
            mct_daemon_application_reset_user_handle(daemon, app, verbose);
        }
    }

    return (ret == MCT_RETURN_OK) ? MCT_RETURN_OK : MCT_RETURN_ERROR;
}

void mct_daemon_control_reset_to_factory_default(MctDaemon *daemon,
                                                 const char *filename,
                                                 const char *filename1,
                                                 int InitialContextLogLevel,
                                                 int InitialContextTraceStatus,
                                                 int InitialEnforceLlTsStatus,
                                                 int verbose)
{
    FILE *fd;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (filename == NULL) || (filename1 == NULL)) {
        mct_log(LOG_WARNING, "Wrong parameter: Null pointer\n");
        return;
    }

    if ((filename[0] == '\0') || (filename1[0] == '\0')) {
        mct_log(LOG_WARNING, "Wrong parameter: Empty string\n");
        return;
    }

    /* Check for runtime cfg file and delete it, if available */
    fd = fopen(filename, "r");

    if (fd != NULL) {
        /* Close and delete file */
        fclose(fd);
        unlink(filename);
    }

    fd = fopen(filename1, "r");

    if (fd != NULL) {
        /* Close and delete file */
        fclose(fd);
        unlink(filename1);
    }

    daemon->default_log_level = InitialContextLogLevel;
    daemon->default_trace_status = InitialContextTraceStatus;
    daemon->force_ll_ts = InitialEnforceLlTsStatus;

    /* Reset all other things (log level, trace status, etc.
     *                         to default values             */

    /* Inform user libraries about changed default log level/trace status */
    mct_daemon_user_send_default_update(daemon, verbose);
}

void mct_daemon_user_send_default_update(MctDaemon *daemon, int verbose)
{
    int32_t count;
    MctDaemonContext *context;
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon == NULL) {
        mct_log(LOG_WARNING, "Wrong parameter: Null pointer\n");
        return;
    }

    user_list = mct_daemon_find_users_list(daemon, daemon->ecuid, verbose);

    if (user_list == NULL) {
        return;
    }

    for (count = 0; count < user_list->num_contexts; count++) {
        context = &(user_list->contexts[count]);

        if (context != NULL) {
            if ((context->log_level == MCT_LOG_DEFAULT) ||
                (context->trace_status == MCT_TRACE_STATUS_DEFAULT)) {
                if (context->user_handle >= MCT_FD_MINIMUM) {
                    if (mct_daemon_user_send_log_level(daemon,
                                                       context,
                                                       verbose) == -1) {
                        mct_vlog(LOG_WARNING,
                                 "Cannot update default of %.4s:%.4s\n",
                                 context->apid,
                                 context->ctid);
                    }
                }
            }
        }
    }
}

void mct_daemon_user_send_all_log_level_update(MctDaemon *daemon, int8_t log_level, int verbose)
{
    int32_t count = 0;
    MctDaemonContext *context = NULL;
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon == NULL) {
        return;
    }

    user_list = mct_daemon_find_users_list(daemon, daemon->ecuid, verbose);

    if (user_list == NULL) {
        return;
    }

    for (count = 0; count < user_list->num_contexts; count++) {
        context = &(user_list->contexts[count]);

        if (context) {
            if (context->user_handle >= MCT_FD_MINIMUM) {
                context->log_level = log_level;

                if (mct_daemon_user_send_log_level(daemon,
                                                   context,
                                                   verbose) == -1) {
                    mct_vlog(LOG_WARNING,
                             "Cannot send log level %.4s:%.4s -> %i\n",
                             context->apid,
                             context->ctid,
                             context->log_level);
                }
            }
        }
    }
}

void mct_daemon_user_send_all_trace_status_update(MctDaemon *daemon,
                                                  int8_t trace_status,
                                                  int verbose)
{
    int32_t count = 0;
    MctDaemonContext *context = NULL;
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon == NULL) {
        return;
    }

    user_list = mct_daemon_find_users_list(daemon, daemon->ecuid, verbose);

    if (user_list == NULL) {
        return;
    }

    mct_vlog(LOG_NOTICE, "All trace status is updated -> %i\n", trace_status);

    for (count = 0; count < user_list->num_contexts; count++) {
        context = &(user_list->contexts[count]);

        if (context) {
            if (context->user_handle >= MCT_FD_MINIMUM) {
                context->trace_status = trace_status;

                if (mct_daemon_user_send_log_level(daemon, context, verbose) == -1) {
                    mct_vlog(LOG_WARNING,
                             "Cannot send trace status %.4s:%.4s -> %i\n",
                             context->apid,
                             context->ctid,
                             context->trace_status);
                }
            }
        }
    }
}

void mct_daemon_user_send_all_log_state(MctDaemon *daemon, int verbose)
{
    int32_t count;
    MctDaemonApplication *app;
    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if (daemon == NULL) {
        mct_log(LOG_WARNING, "Wrong parameter: Null pointer\n");
        return;
    }

    user_list = mct_daemon_find_users_list(daemon, daemon->ecuid, verbose);

    if (user_list == NULL) {
        return;
    }

    for (count = 0; count < user_list->num_applications; count++) {
        app = &(user_list->applications[count]);

        if (app != NULL) {
            if (app->user_handle >= MCT_FD_MINIMUM) {
                if (mct_daemon_user_send_log_state(daemon, app, verbose) == -1) {
                    mct_vlog(LOG_WARNING,
                             "Cannot send log state to Apid: %.4s, PID: %d\n",
                             app->apid,
                             app->pid);
                }
            }
        }
    }
}

void mct_daemon_change_state(MctDaemon *daemon, MctDaemonState newState)
{
    switch (newState) {
        case MCT_DAEMON_STATE_INIT:
            mct_log(LOG_INFO, "Switched to init state.\n");
            daemon->state = MCT_DAEMON_STATE_INIT;
            break;
        case MCT_DAEMON_STATE_BUFFER:
            mct_log(LOG_INFO, "Switched to buffer state for socket connections.\n");
            daemon->state = MCT_DAEMON_STATE_BUFFER;
            break;
        case MCT_DAEMON_STATE_BUFFER_FULL:
            mct_log(LOG_INFO, "Switched to buffer full state.\n");
            daemon->state = MCT_DAEMON_STATE_BUFFER_FULL;
            break;
        case MCT_DAEMON_STATE_SEND_BUFFER:
            mct_log(LOG_INFO, "Switched to send buffer state for socket connections.\n");
            daemon->state = MCT_DAEMON_STATE_SEND_BUFFER;
            break;
        case MCT_DAEMON_STATE_SEND_DIRECT:
            mct_log(LOG_INFO, "Switched to send direct state.\n");
            daemon->state = MCT_DAEMON_STATE_SEND_DIRECT;
            break;
    }
}

static int mct_daemon_user_send_update_blockmode(MctDaemon *daemon,
                                                 MctDaemonApplication *app,
                                                 MctUserHeader *uHeader,
                                                 MctUserControlMsgBlockMode *ucBM,
                                                 int verbose)
{
    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (app == NULL) || (uHeader == NULL) || (ucBM == NULL)) {
        mct_vlog(LOG_ERR, "%s: Wrong parameter: Null pointer\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    /* log to FIFO */
    if (mct_user_log_out2(app->user_handle,
                          uHeader,
                          sizeof(MctUserHeader),
                          ucBM,
                          sizeof(MctUserControlMsgBlockMode)) != MCT_RETURN_OK) {
        mct_vlog(LOG_WARNING,
                 "Unable to send BlockMode update to '%s'", app->apid);

        if (errno == EPIPE) {
            mct_daemon_application_del(daemon, app, daemon->ecuid, verbose);
        }

        return MCT_RETURN_ERROR;
    }

    return MCT_RETURN_OK;
}

int mct_daemon_user_update_blockmode(MctDaemon *daemon,
                                     char *name,
                                     int block_mode,
                                     int verbose)
{
    MctUserHeader uHeader;
    MctUserControlMsgBlockMode userBlockmode;
    int8_t ret = 0;
    int32_t count;
    MctDaemonApplication *application;

    MctDaemonRegisteredUsers *user_list = NULL;

    PRINT_FUNCTION_VERBOSE(verbose);

    if ((daemon == NULL) || (name == NULL) ||
        ((block_mode < MCT_MODE_NON_BLOCKING) ||
         (block_mode > MCT_MODE_BLOCKING))) {
        mct_vlog(LOG_ERR, "%s: Wrong parameter\n", __func__);
        return MCT_RETURN_WRONG_PARAMETER;
    }

    user_list = mct_daemon_find_users_list(daemon, daemon->ecuid, verbose);

    if (user_list == NULL) {
        return MCT_RETURN_ERROR;
    }

    if (mct_user_set_userheader(&uHeader, MCT_USER_MESSAGE_SET_BLOCK_MODE) == -1) {
        return MCT_RETURN_ERROR;
    }

    userBlockmode.block_mode = block_mode;

    if (strncmp(name, MCT_ALL_APPLICATIONS, MCT_ID_SIZE) == 0) { /* send to all application. */

        for (count = 0; count < user_list->num_applications; count++) {
            application = &(user_list->applications[count]);

            if (application == NULL) {
                mct_log(LOG_INFO, "No application registered\n");
                return MCT_RETURN_OK;
            }

            if (application->blockmode_status != block_mode) {
                if (mct_daemon_user_send_update_blockmode(daemon,
                                                          application,
                                                          &uHeader,
                                                          &userBlockmode,
                                                          verbose) == MCT_RETURN_OK) {
                    application->blockmode_status = block_mode;
                }
            }
        }

        /* set daemon block mode */
        daemon->blockMode = block_mode;
    } else { /* Send to specified application */
        application = mct_daemon_application_find(daemon, name,
                                                  daemon->ecuid, verbose);

        if (application == NULL) {
            mct_vlog(LOG_WARNING,
                     "Specified application %.4s not registered\n",
                     name);
            return MCT_RETURN_ERROR;
        }

        if (application->blockmode_status != block_mode) {
            if (mct_daemon_user_send_update_blockmode(daemon,
                                                      application,
                                                      &uHeader,
                                                      &userBlockmode,
                                                      verbose) == MCT_RETURN_OK) {
                application->blockmode_status = block_mode;
            }
        }
    }

    return ret;
}
