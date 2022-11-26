#ifndef DLT_OFFLINE_LOGSTORAGE_INTERNAL_H
#define DLT_OFFLINE_LOGSTORAGE_INTERNAL_H

DLT_STATIC int mct_logstorage_list_destroy(DltLogStorageFilterList **list,
                                           DltLogStorageUserConfig *uconfig,
                                           char *dev_path,
                                           int reason);

DLT_STATIC int mct_logstorage_list_add_config(DltLogStorageFilterConfig *data,
                                              DltLogStorageFilterConfig **listdata);
DLT_STATIC int mct_logstorage_list_add(char *key,
                                       int num_keys,
                                       DltLogStorageFilterConfig *data,
                                       DltLogStorageFilterList **list);

DLT_STATIC int mct_logstorage_list_find(char *key,
                                        DltLogStorageFilterList **list,
                                        DltLogStorageFilterConfig **config);

DLT_STATIC int mct_logstorage_count_ids(const char *str);

DLT_STATIC int mct_logstorage_read_number(unsigned int *number, char *value);

DLT_STATIC int mct_logstorage_read_list_of_names(char **names, const char *value);

DLT_STATIC int mct_logstorage_check_apids(DltLogStorageFilterConfig *config, char *value);

DLT_STATIC int mct_logstorage_check_ctids(DltLogStorageFilterConfig *config, char *value);

DLT_STATIC int mct_logstorage_store_config_excluded_apids(DltLogStorageFilterConfig *config, char *value);

DLT_STATIC int mct_logstorage_store_config_excluded_ctids(DltLogStorageFilterConfig *config, char *value);

DLT_STATIC bool mct_logstorage_check_excluded_ids(char *id, char *delim, char *excluded_ids);

DLT_STATIC int mct_logstorage_check_loglevel(DltLogStorageFilterConfig *config, char *value);

DLT_STATIC int mct_logstorage_check_filename(DltLogStorageFilterConfig *config, char *value);

DLT_STATIC int mct_logstorage_check_filesize(DltLogStorageFilterConfig *config, char *value);

DLT_STATIC int mct_logstorage_check_nofiles(DltLogStorageFilterConfig *config, char *value);

DLT_STATIC int mct_logstorage_check_sync_strategy(DltLogStorageFilterConfig *config, char *value);

DLT_STATIC int mct_logstorage_check_ecuid(DltLogStorageFilterConfig *config, char *value);

DLT_STATIC int mct_logstorage_check_param(DltLogStorageFilterConfig *config,
                                          DltLogstorageFilterConfType ctype,
                                          char *value);

DLT_STATIC int mct_logstorage_store_filters(DltLogStorage *handle,
                                            char *config_file_name);

void mct_logstorage_free(DltLogStorage *handle, int reason);

DLT_STATIC int mct_logstorage_create_keys(char *apids,
                                          char *ctids,
                                          char *ecuid,
                                          char **keys,
                                          int *num_keys);

DLT_STATIC int mct_logstorage_prepare_table(DltLogStorage *handle,
                                            DltLogStorageFilterConfig *data);

DLT_STATIC int mct_logstorage_validate_filter_name(char *name);

DLT_STATIC void mct_logstorage_filter_set_strategy(DltLogStorageFilterConfig *config,
                                                   int strategy);

DLT_STATIC int mct_logstorage_load_config(DltLogStorage *handle);

DLT_STATIC int mct_logstorage_filter(DltLogStorage *handle,
                                     DltLogStorageFilterConfig **config,
                                     char *apid,
                                     char *ctid,
                                     char *ecuid,
                                     int log_level);

#endif /* DLT_OFFLINE_LOGSTORAGE_INTERNAL_H */
