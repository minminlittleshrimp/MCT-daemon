#ifndef DLT_DAEMON_OFFLINE_LOGSTORAGE_INTERNAL_H
#define DLT_DAEMON_OFFLINE_LOGSTORAGE_INTERNAL_H

DLT_STATIC DltReturnValue mct_logstorage_split_key(char *key,
                                                   char *apid,
                                                   char *ctid,
                                                   char *ecuid);

DltReturnValue mct_logstorage_update_all_contexts(DltDaemon *daemon,
                                                  DltDaemonLocal *daemon_local,
                                                  char *id,
                                                  int curr_log_level,
                                                  int cmp_flag,
                                                  char *ecuid,
                                                  int verbose);

DltReturnValue mct_logstorage_update_context(DltDaemon *daemon,
                                             DltDaemonLocal *daemon_local,
                                             char *apid,
                                             char *ctid,
                                             char *ecuid,
                                             int curr_log_level,
                                             int verbose);

DltReturnValue mct_logstorage_update_context_loglevel(DltDaemon *daemon,
                                                      DltDaemonLocal *daemon_local,
                                                      char *key,
                                                      int curr_log_level,
                                                      int verbose);

#endif
