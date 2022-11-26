#ifndef MCT_OFFLINE_LOGSTORAGE_INTERNAL_H
#define MCT_OFFLINE_LOGSTORAGE_INTERNAL_H

static int mct_logstorage_list_destroy(MctLogStorageFilterList **list,
                                           MctLogStorageUserConfig *uconfig,
                                           char *dev_path,
                                           int reason);

static int mct_logstorage_list_add_config(MctLogStorageFilterConfig *data,
                                              MctLogStorageFilterConfig **listdata);
static int mct_logstorage_list_add(char *key,
                                       int num_keys,
                                       MctLogStorageFilterConfig *data,
                                       MctLogStorageFilterList **list);

static int mct_logstorage_list_find(char *key,
                                        MctLogStorageFilterList **list,
                                        MctLogStorageFilterConfig **config);

static int mct_logstorage_count_ids(const char *str);

static int mct_logstorage_read_number(unsigned int *number, char *value);

static int mct_logstorage_read_list_of_names(char **names, const char *value);

static int mct_logstorage_check_apids(MctLogStorageFilterConfig *config, char *value);

static int mct_logstorage_check_ctids(MctLogStorageFilterConfig *config, char *value);

static int mct_logstorage_store_config_excluded_apids(MctLogStorageFilterConfig *config, char *value);

static int mct_logstorage_store_config_excluded_ctids(MctLogStorageFilterConfig *config, char *value);

static bool mct_logstorage_check_excluded_ids(char *id, char *delim, char *excluded_ids);

static int mct_logstorage_check_loglevel(MctLogStorageFilterConfig *config, char *value);

static int mct_logstorage_check_filename(MctLogStorageFilterConfig *config, char *value);

static int mct_logstorage_check_filesize(MctLogStorageFilterConfig *config, char *value);

static int mct_logstorage_check_nofiles(MctLogStorageFilterConfig *config, char *value);

static int mct_logstorage_check_sync_strategy(MctLogStorageFilterConfig *config, char *value);

static int mct_logstorage_check_ecuid(MctLogStorageFilterConfig *config, char *value);

static int mct_logstorage_check_param(MctLogStorageFilterConfig *config,
                                          MctLogstorageFilterConfType ctype,
                                          char *value);

static int mct_logstorage_store_filters(MctLogStorage *handle,
                                            char *config_file_name);

void mct_logstorage_free(MctLogStorage *handle, int reason);

static int mct_logstorage_create_keys(char *apids,
                                          char *ctids,
                                          char *ecuid,
                                          char **keys,
                                          int *num_keys);

static int mct_logstorage_prepare_table(MctLogStorage *handle,
                                            MctLogStorageFilterConfig *data);

static int mct_logstorage_validate_filter_name(char *name);

static void mct_logstorage_filter_set_strategy(MctLogStorageFilterConfig *config,
                                                   int strategy);

static int mct_logstorage_load_config(MctLogStorage *handle);

static int mct_logstorage_filter(MctLogStorage *handle,
                                     MctLogStorageFilterConfig **config,
                                     char *apid,
                                     char *ctid,
                                     char *ecuid,
                                     int log_level);

#endif /* MCT_OFFLINE_LOGSTORAGE_INTERNAL_H */
