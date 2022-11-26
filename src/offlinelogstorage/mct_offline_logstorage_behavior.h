#ifndef MCT_OFFLINELOGSTORAGE_MCT_OFFLINE_LOGSTORAGE_BEHAVIOR_H_
#define MCT_OFFLINELOGSTORAGE_MCT_OFFLINE_LOGSTORAGE_BEHAVIOR_H_

/* ON_MSG behavior */
int mct_logstorage_prepare_on_msg(MctLogStorageFilterConfig *config,
                                  MctLogStorageUserConfig *file_config,
                                  char *dev_path,
                                  int log_msg_size,
                                  MctNewestFileName *newest_file_info);
int mct_logstorage_write_on_msg(MctLogStorageFilterConfig *config,
                                MctLogStorageUserConfig *file_config,
                                char *dev_path,
                                unsigned char *data1,
                                int size1,
                                unsigned char *data2,
                                int size2,
                                unsigned char *data3,
                                int size3);

/* status is strategy, e.g. MCT_LOGSTORAGE_SYNC_ON_MSG is used when callback
 * is called on message received */
int mct_logstorage_sync_on_msg(MctLogStorageFilterConfig *config,
                               MctLogStorageUserConfig *file_config,
                               char *dev_path,
                               int status);

/* Logstorage cache functionality */
int mct_logstorage_prepare_msg_cache(MctLogStorageFilterConfig *config,
                                     MctLogStorageUserConfig *file_config,
                                     char *dev_path,
                                     int log_msg_size,
                                     MctNewestFileName *newest_file_info);

int mct_logstorage_write_msg_cache(MctLogStorageFilterConfig *config,
                                   MctLogStorageUserConfig *file_config,
                                   char *dev_path,
                                   unsigned char *data1,
                                   int size1,
                                   unsigned char *data2,
                                   int size2,
                                   unsigned char *data3,
                                   int size3);

int mct_logstorage_sync_msg_cache(MctLogStorageFilterConfig *config,
                                  MctLogStorageUserConfig *file_config,
                                  char *dev_path,
                                  int status);

#endif /* MCT_OFFLINELOGSTORAGE_MCT_OFFLINE_LOGSTORAGE_BEHAVIOR_H_ */
