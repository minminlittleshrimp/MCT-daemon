#ifndef DLT_OFFLINELOGSTORAGE_BEHAVIOR_INTERNAL_H_
#define DLT_OFFLINELOGSTORAGE_BEHAVIOR_INTERNAL_H_

void mct_logstorage_log_file_name(char *log_file_name,
                                  DltLogStorageUserConfig *file_config,
                                  const char *name,
                                  const int num_files,
                                  const int idx);

unsigned int mct_logstorage_sort_file_name(DltLogStorageFileList **head);

void mct_logstorage_rearrange_file_name(DltLogStorageFileList **head);

unsigned int mct_logstorage_get_idx_of_log_file(DltLogStorageUserConfig *file_config,
                                                char *file);

int mct_logstorage_storage_dir_info(DltLogStorageUserConfig *file_config,
                                    char *path,
                                    DltLogStorageFilterConfig *config);

int mct_logstorage_open_log_file(DltLogStorageFilterConfig *config,
                                 DltLogStorageUserConfig *file_config,
                                 char *dev_path,
                                 int msg_size,
                                 bool is_update_required,
                                 bool is_sync);

DLT_STATIC int mct_logstorage_sync_to_file(DltLogStorageFilterConfig *config,
                                           DltLogStorageUserConfig *file_config,
                                           char *dev_path,
                                           DltLogStorageCacheFooter *footer,
                                           unsigned int start_offset,
                                           unsigned int end_offset);

DLT_STATIC int mct_logstorage_find_mct_header(void *ptr,
                                              unsigned int offset,
                                              unsigned int cnt);

DLT_STATIC int mct_logstorage_find_last_mct_header(void *ptr,
                                                   unsigned int offset,
                                                   unsigned int cnt);

#endif /* DLT_OFFLINELOGSTORAGE_BEHAVIOR_INTERNAL_H_ */
