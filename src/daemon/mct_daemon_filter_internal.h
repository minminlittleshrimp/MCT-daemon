#ifndef _MCT_DAEMON_FILTER_INTERNAL_H
#define _MCT_DAEMON_FILTER_INTERNAL_H

static MctReturnValue enable_all(MctServiceIdFlag *flags);

static MctReturnValue init_flags(MctServiceIdFlag *flags);

static MctReturnValue set_bit(MctServiceIdFlag *flags, int id);

static MctReturnValue bit(MctServiceIdFlag *flags, int id);

static MctReturnValue mct_daemon_filter_name(MctMessageFilter *mf,
                                                 MctFilterConfiguration *config,
                                                 char *value);

static MctReturnValue mct_daemon_filter_level(MctMessageFilter *mf,
                                                  MctFilterConfiguration *config,
                                                  char *value);

static MctReturnValue mct_daemon_filter_control_mask(MctMessageFilter *mf,
                                                         MctFilterConfiguration *config,
                                                         char *value);

static MctReturnValue mct_daemon_filter_client_mask(MctMessageFilter *mf,
                                                        MctFilterConfiguration *config,
                                                        char *value);

static MctReturnValue mct_daemon_filter_injections(MctMessageFilter *mf,
                                                       MctFilterConfiguration *config,
                                                       char *value);

static MctReturnValue mct_daemon_set_injection_service_ids(int **ids,
                                                               int *num,
                                                               char *value);

static MctInjectionConfig *mct_daemon_filter_find_injection_by_name(
    MctInjectionConfig *injections,
    char *name);

static MctReturnValue mct_daemon_injection_name(MctMessageFilter *mf,
                                                    MctInjectionConfig *config,
                                                    char *value);

static MctReturnValue mct_daemon_injection_apid(MctMessageFilter *mf,
                                                    MctInjectionConfig *config,
                                                    char *value);

static MctReturnValue mct_daemon_injection_ctid(MctMessageFilter *mf,
                                                    MctInjectionConfig *config,
                                                    char *value);

static MctReturnValue mct_daemon_injection_ecu_id(MctMessageFilter *mf,
                                                      MctInjectionConfig *config,
                                                      char *value);

static MctReturnValue mct_daemon_injection_service_id(MctMessageFilter *mf,
                                                          MctInjectionConfig *config,
                                                          char *value);

static MctReturnValue mct_daemon_get_name(MctMessageFilter *mf, char *val);

static MctReturnValue mct_daemon_get_default_level(MctMessageFilter *mf, char *val);

static MctReturnValue mct_daemon_get_backend(MctMessageFilter *mf, char *val);

static MctReturnValue mct_daemon_setup_filter_section(MctMessageFilter *mf,
                                                          MctConfigFile *config,
                                                          char *sec_name);

static MctReturnValue mct_daemon_setup_filter_properties(MctMessageFilter *mf,
                                                             MctConfigFile *config,
                                                             char *sec_name);

static MctReturnValue mct_daemon_setup_injection_config(MctMessageFilter *mf,
                                                            MctConfigFile *config,
                                                            char *sec_name);

void mct_daemon_free_filter_configuration(MctFilterConfiguration *conf);

static MctReturnValue mct_daemon_check_filter_level(MctFilterConfiguration *conf,
                                                        MctFilterConfiguration *configs);

static MctReturnValue mct_daemon_insert_filter(MctFilterConfiguration *conf,
                                                   MctFilterConfiguration **configs);

static MctFilterConfiguration *mct_daemon_add_closed_filter(char *name);

static MctReturnValue mct_daemon_check_filter_level_range(MctMessageFilter *mf);

static MctReturnValue mct_daemon_set_default_level(MctMessageFilter *mf);

#endif
