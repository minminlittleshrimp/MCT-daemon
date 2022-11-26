#ifndef MCT_OFFLINELOGSTORAGE_BEHAVIOR_INTERNAL_H_
#define MCT_OFFLINELOGSTORAGE_BEHAVIOR_INTERNAL_H_

void mct_logstorage_log_file_name(char *log_file_name,
                                  MctLogStorageUserConfig *file_config,
                                  const char *name,
                                  const int num_files,
                                  const int idx);

unsigned int mct_logstorage_sort_file_name(MctLogStorageFileList **head);

void mct_logstorage_rearrange_file_name(MctLogStorageFileList **head);

unsigned int mct_logstorage_get_idx_of_log_file(MctLogStorageUserConfig *file_config,
                                                char *file);

int mct_logstorage_storage_dir_info(MctLogStorageUserConfig *file_config,
                                    char *path,
                                    MctLogStorageFilterConfig *config);

int mct_logstorage_open_log_file(MctLogStorageFilterConfig *config,
                                 MctLogStorageUserConfig *file_config,
                                 char *dev_path,
                                 int msg_size,
                                 bool is_update_required,
                                 bool is_sync);

static int mct_logstorage_sync_to_file(MctLogStorageFilterConfig *config,
                                           MctLogStorageUserConfig *file_config,
                                           char *dev_path,
                                           MctLogStorageCacheFooter *footer,
                                           unsigned int start_offset,
                                           unsigned int end_offset);

static int mct_logstorage_find_mct_header(void *ptr,
                                              unsigned int offset,
                                              unsigned int cnt);

static int mct_logstorage_find_last_mct_header(void *ptr,
                                                   unsigned int offset,
                                                   unsigned int cnt);

#endif /* MCT_OFFLINELOGSTORAGE_BEHAVIOR_INTERNAL_H_ */
