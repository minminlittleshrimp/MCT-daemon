#ifndef _DLT_DAEMON_FILTER_INTERNAL_H
#define _DLT_DAEMON_FILTER_INTERNAL_H

DLT_STATIC DltReturnValue enable_all(DltServiceIdFlag *flags);

DLT_STATIC DltReturnValue init_flags(DltServiceIdFlag *flags);

DLT_STATIC DltReturnValue set_bit(DltServiceIdFlag *flags, int id);

DLT_STATIC DltReturnValue bit(DltServiceIdFlag *flags, int id);

DLT_STATIC DltReturnValue mct_daemon_filter_name(DltMessageFilter *mf,
                                                 DltFilterConfiguration *config,
                                                 char *value);

DLT_STATIC DltReturnValue mct_daemon_filter_level(DltMessageFilter *mf,
                                                  DltFilterConfiguration *config,
                                                  char *value);

DLT_STATIC DltReturnValue mct_daemon_filter_control_mask(DltMessageFilter *mf,
                                                         DltFilterConfiguration *config,
                                                         char *value);

DLT_STATIC DltReturnValue mct_daemon_filter_client_mask(DltMessageFilter *mf,
                                                        DltFilterConfiguration *config,
                                                        char *value);

DLT_STATIC DltReturnValue mct_daemon_filter_injections(DltMessageFilter *mf,
                                                       DltFilterConfiguration *config,
                                                       char *value);

DLT_STATIC DltReturnValue mct_daemon_set_injection_service_ids(int **ids,
                                                               int *num,
                                                               char *value);

DLT_STATIC DltInjectionConfig *mct_daemon_filter_find_injection_by_name(
    DltInjectionConfig *injections,
    char *name);

DLT_STATIC DltReturnValue mct_daemon_injection_name(DltMessageFilter *mf,
                                                    DltInjectionConfig *config,
                                                    char *value);

DLT_STATIC DltReturnValue mct_daemon_injection_apid(DltMessageFilter *mf,
                                                    DltInjectionConfig *config,
                                                    char *value);

DLT_STATIC DltReturnValue mct_daemon_injection_ctid(DltMessageFilter *mf,
                                                    DltInjectionConfig *config,
                                                    char *value);

DLT_STATIC DltReturnValue mct_daemon_injection_ecu_id(DltMessageFilter *mf,
                                                      DltInjectionConfig *config,
                                                      char *value);

DLT_STATIC DltReturnValue mct_daemon_injection_service_id(DltMessageFilter *mf,
                                                          DltInjectionConfig *config,
                                                          char *value);

DLT_STATIC DltReturnValue mct_daemon_get_name(DltMessageFilter *mf, char *val);

DLT_STATIC DltReturnValue mct_daemon_get_default_level(DltMessageFilter *mf, char *val);

DLT_STATIC DltReturnValue mct_daemon_get_backend(DltMessageFilter *mf, char *val);

DLT_STATIC DltReturnValue mct_daemon_setup_filter_section(DltMessageFilter *mf,
                                                          DltConfigFile *config,
                                                          char *sec_name);

DLT_STATIC DltReturnValue mct_daemon_setup_filter_properties(DltMessageFilter *mf,
                                                             DltConfigFile *config,
                                                             char *sec_name);

DLT_STATIC DltReturnValue mct_daemon_setup_injection_config(DltMessageFilter *mf,
                                                            DltConfigFile *config,
                                                            char *sec_name);

void mct_daemon_free_filter_configuration(DltFilterConfiguration *conf);

DLT_STATIC DltReturnValue mct_daemon_check_filter_level(DltFilterConfiguration *conf,
                                                        DltFilterConfiguration *configs);

DLT_STATIC DltReturnValue mct_daemon_insert_filter(DltFilterConfiguration *conf,
                                                   DltFilterConfiguration **configs);

DLT_STATIC DltFilterConfiguration *mct_daemon_add_closed_filter(char *name);

DLT_STATIC DltReturnValue mct_daemon_check_filter_level_range(DltMessageFilter *mf);

DLT_STATIC DltReturnValue mct_daemon_set_default_level(DltMessageFilter *mf);

#endif
