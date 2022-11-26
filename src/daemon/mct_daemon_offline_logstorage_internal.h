#ifndef MCT_DAEMON_OFFLINE_LOGSTORAGE_INTERNAL_H
#define MCT_DAEMON_OFFLINE_LOGSTORAGE_INTERNAL_H

static MctReturnValue mct_logstorage_split_key(char *key,
                                                   char *apid,
                                                   char *ctid,
                                                   char *ecuid);

MctReturnValue mct_logstorage_update_all_contexts(MctDaemon *daemon,
                                                  MctDaemonLocal *daemon_local,
                                                  char *id,
                                                  int curr_log_level,
                                                  int cmp_flag,
                                                  char *ecuid,
                                                  int verbose);

MctReturnValue mct_logstorage_update_context(MctDaemon *daemon,
                                             MctDaemonLocal *daemon_local,
                                             char *apid,
                                             char *ctid,
                                             char *ecuid,
                                             int curr_log_level,
                                             int verbose);

MctReturnValue mct_logstorage_update_context_loglevel(MctDaemon *daemon,
                                                      MctDaemonLocal *daemon_local,
                                                      char *key,
                                                      int curr_log_level,
                                                      int verbose);

#endif
