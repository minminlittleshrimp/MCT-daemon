#include <syslog.h>
#include <limits.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>

#include "mct_offline_logstorage.h"
#include "mct_offline_logstorage_behavior.h"
#include "mct_offline_logstorage_behavior_internal.h"

unsigned int g_logstorage_cache_size;
/**
 * mct_logstorage_log_file_name
 *
 * Create log file name in the form configured by the user
 *      \<filename\>\<delimiter\>\<index\>\<delimiter\>\<timestamp\>.mct
 *
 *      filename:       given in configuration file
 *      delimiter:      Punctuation characters (configured in mct.conf)
 *      timestamp:      yyyy-mm-dd-hh-mm-ss (enabled/disabled in mct.conf)
 *      index:          Index len depends on wrap around value in mct.conf
 *                      ex: wrap around = 99, index will 01..99
 *                      (enabled/disabled in mct.conf)
 *
 * @param[out] log_file_name     target buffer for the complete logfile name.
 *                               it needs to fit MCT_MOUNT_PATH_MAX chars
 * @param[in]  file_config       User configurations for log file
 * @param[in]  name              file name given in configuration file
 * @param[in]  num_files         max files given in configuration file
 * @param[in]  idx               continous index of log files
 * @ return                 None
 */
void mct_logstorage_log_file_name(char *log_file_name,
                                  MctLogStorageUserConfig *file_config,
                                  const char *name,
                                  const int num_files,
                                  const int idx)
{
    if ((log_file_name == NULL) || (file_config == NULL))
        return;

    const char delim = file_config->logfile_delimiter;
    int index_width = file_config->logfile_counteridxlen;

    if (file_config->logfile_maxcounter == UINT_MAX) {
        index_width = 0;
    }

    const char * suffix = ".mct";
    const int smax = MCT_MOUNT_PATH_MAX - strlen(suffix) - 1;
    int spos = 0;
    log_file_name[spos] = '\0';
    int rt;

    /* Append file name */
    spos += strlen(name);
    strncat(log_file_name, name, smax - spos);

    /* Append index */
    /* Do not append if there is only one file and optional index mode is true*/
    if (!(num_files == 1 && file_config->logfile_optional_counter)) {
        rt = snprintf(log_file_name+spos, smax-spos, "%c%0*d", delim, index_width, idx);
        if (rt >= smax-spos) {
            mct_vlog(LOG_WARNING, "%s: snprintf truncation %s\n", __func__, log_file_name);
            spos = smax;
        } else if (rt < 0) {
            mct_vlog(LOG_ERR, "%s: snprintf error rt=%d\n", __func__, rt);
            const char *fmt_err = "fmt_err";
            memcpy(log_file_name, fmt_err, strlen(fmt_err)+1);
            spos = strlen(fmt_err) + 1;
        } else {
            spos += rt;
        }
    }

    /* Add time stamp if user has configured */
    if (file_config->logfile_timestamp) {
        char stamp[MCT_OFFLINE_LOGSTORAGE_TIMESTAMP_LEN + 1] = { 0 };
        time_t t = time(NULL);
        struct tm tm_info;
        ssize_t n = 0;
        tzset();
        localtime_r(&t, &tm_info);
        n = snprintf(stamp,
                     MCT_OFFLINE_LOGSTORAGE_TIMESTAMP_LEN + 1,
                     "%c%04d%02d%02d-%02d%02d%02d",
                     delim,
                     1900 + tm_info.tm_year,
                     1 + tm_info.tm_mon,
                     tm_info.tm_mday,
                     tm_info.tm_hour,
                     tm_info.tm_min,
                     tm_info.tm_sec);
        if (n < 0 || (size_t)n > (MCT_OFFLINE_LOGSTORAGE_TIMESTAMP_LEN + 1)) {
            mct_vlog(LOG_WARNING, "%s: snprintf truncation %s\n", __func__,
                     stamp);
        }
        strncat(log_file_name, stamp, smax-spos);
    }

    strcat(log_file_name, suffix);
}

/**
 * mct_logstorage_sort_file_name
 *
 * Sort the filenames with index based ascending order (bubble sort)
 *
 * @param head              Log filename list
 * @ return                 The last (biggest) index
 */
unsigned int mct_logstorage_sort_file_name(MctLogStorageFileList **head)
{
    int done = 0;
    unsigned int max_idx = 0;

    if ((head == NULL) || (*head == NULL) || ((*head)->next == NULL))
        return 0;

    while (!done) {
        /* "source" of the pointer to the current node in the list struct */
        MctLogStorageFileList **pv = head;
        MctLogStorageFileList *nd = *head; /* local iterator pointer */
        MctLogStorageFileList *nx = (*head)->next; /* local next pointer */

        done = 1;

        while (nx) {
            max_idx = nx->idx;
            if (nd->idx > nx->idx) {
                max_idx = nd->idx;
                nd->next = nx->next;
                nx->next = nd;
                *pv = nx;

                done = 0;
            }

            pv = &nd->next;
            nd = nx;
            nx = nx->next;
        }
    }

    return max_idx;
}

/**
 * mct_logstorage_rearrange_file_name
 *
 * Rearrange the filenames in the order of latest and oldest
 *
 * @param head              Log filename list
 * @ return                 None
 */
void mct_logstorage_rearrange_file_name(MctLogStorageFileList **head)
{
    MctLogStorageFileList *n_prev = NULL;
    MctLogStorageFileList *tail = NULL;
    MctLogStorageFileList *wrap_pre = NULL;
    MctLogStorageFileList *wrap_post = NULL;
    MctLogStorageFileList *n = NULL;

    if ((head == NULL) || (*head == NULL) || ((*head)->next == NULL))
        return;

    if ((*head)->idx != 1)
    {
        /* Do not sort */
        return;
    }

    for (n = *head; n != NULL; n = n->next) {
        /* Compare the diff between n->idx and n_prev->idx only if
         * wrap_post and wrap_pre are not set yet. Otherwise continue the loop
         * until the tail */
        if (n && n_prev && !wrap_post && !wrap_pre) {
            if ((n->idx - n_prev->idx) != 1) {
                wrap_post = n;
                wrap_pre = n_prev;
            }
        }

        n_prev = n;
    }

    tail = n_prev;

    if (wrap_post && wrap_pre) {
        wrap_pre->next = NULL;
        tail->next = *head;
        *head = wrap_post;
    }
}

/**
 * mct_logstorage_get_idx_of_log_file
 *
 * Extract index of log file name passed as input argument
 *
 * @param file          file name to extract the index from
 * @param file_config   User configurations for log file
 * @return index on success, -1 if no index is found
 */
unsigned int mct_logstorage_get_idx_of_log_file(MctLogStorageUserConfig *file_config,
                                                char *file)
{
    unsigned int idx = -1;
    char *endptr;
    char *filename;
    unsigned int filename_len = 0;
    unsigned int fileindex_len = 0;

    if ((file_config == NULL) || (file == NULL))
        return -1;

    /* Calculate actual file name length */
    filename = strchr(file, file_config->logfile_delimiter);

    if (filename == NULL) {
        mct_vlog(LOG_ERR, "Cannot extract filename from %s\n", file);
        return -1;
    }

    filename_len = strlen(file) - strlen(filename);

    /* index is retrived from file name */
    if (file_config->logfile_timestamp) {
        fileindex_len = strlen(file) -
            (MCT_OFFLINE_LOGSTORAGE_FILE_EXTENSION_LEN +
             MCT_OFFLINE_LOGSTORAGE_TIMESTAMP_LEN +
             filename_len + 1);

        idx = (int)strtol(&file[strlen(file) -
                                (MCT_OFFLINE_LOGSTORAGE_FILE_EXTENSION_LEN +
                                 fileindex_len +
                                 MCT_OFFLINE_LOGSTORAGE_TIMESTAMP_LEN)],
                          &endptr,
                          10);
    }
    else {
        fileindex_len = strlen(file) -
            (MCT_OFFLINE_LOGSTORAGE_FILE_EXTENSION_LEN +
             filename_len + 1);

        idx = (int)strtol(&file[strlen(file) -
                                (MCT_OFFLINE_LOGSTORAGE_FILE_EXTENSION_LEN
                                 + fileindex_len)], &endptr, 10);
    }

    if ((endptr == file) || (idx == 0))
        mct_log(LOG_ERR,
                "Unable to calculate index from log file name. Reset to 001.\n");

    return idx;
}

/**
 * mct_logstorage_storage_dir_info
 *
 * Read file names of storage directory.
 * Update the file list, arrange it in order of latest and oldest
 *
 * @param file_config   User configurations for log file
 * @param path          Path to storage directory
 * @param  config       MctLogStorageFilterConfig
 * @return              0 on success, -1 on error
 */
int mct_logstorage_storage_dir_info(MctLogStorageUserConfig *file_config,
                                    char *path,
                                    MctLogStorageFilterConfig *config)
{
    int check = 0;
    int i = 0;
    int cnt = 0;
    int ret = 0;
    unsigned int max_idx = 0;
    struct dirent **files = { 0 };
    unsigned int current_idx = 0;
    MctLogStorageFileList *n = NULL;
    MctLogStorageFileList *n1 = NULL;
    char storage_path[MCT_OFFLINE_LOGSTORAGE_MAX_PATH_LEN + 1] = { '\0' };
    char file_name[MCT_OFFLINE_LOGSTORAGE_MAX_FILE_NAME_LEN + 1] = { '\0' };
    char* dir = NULL;

    if ((config == NULL) ||
        (file_config == NULL) ||
        (path == NULL) ||
        (config->file_name == NULL))
        return -1;

    strncpy(storage_path, path, MCT_OFFLINE_LOGSTORAGE_MAX_PATH_LEN);

    if (strstr(config->file_name, "/") != NULL) {
        /* Append directory path */
        char tmpdir[MCT_OFFLINE_LOGSTORAGE_MAX_FILE_NAME_LEN + 1] = { '\0' };
        char tmpfile[MCT_OFFLINE_LOGSTORAGE_MAX_FILE_NAME_LEN + 1] = { '\0' };
        char *file;
        strncpy(tmpdir, config->file_name, MCT_OFFLINE_LOGSTORAGE_MAX_FILE_NAME_LEN);
        strncpy(tmpfile, config->file_name, MCT_OFFLINE_LOGSTORAGE_MAX_FILE_NAME_LEN);
        dir = dirname(tmpdir);
        file = basename(tmpfile);
        if ((strlen(path) + strlen(dir)) > MCT_OFFLINE_LOGSTORAGE_MAX_PATH_LEN) {
            mct_vlog(LOG_ERR, "%s: Directory name [%s] is too long to store (file name [%s])\n",
                     __func__, dir, file);
            return -1;
        }
        strncat(storage_path, dir, MCT_OFFLINE_LOGSTORAGE_MAX_PATH_LEN - strlen(dir));
        strncpy(file_name, file, MCT_OFFLINE_LOGSTORAGE_MAX_FILE_NAME_LEN);
    } else {
        strncpy(file_name, config->file_name, MCT_OFFLINE_LOGSTORAGE_MAX_FILE_NAME_LEN);
    }

    cnt = scandir(storage_path, &files, 0, alphasort);

    if (cnt < 0) {
        mct_vlog(LOG_ERR, "%s: Failed to scan directory [%s] for file name [%s]\n",
                 __func__, storage_path, file_name);
        return -1;
    }

    mct_vlog(LOG_DEBUG, "%s: Scanned [%d] files from %s\n", __func__, cnt, storage_path);

    /* In order to have a latest status of file list,
     * the existing records must be deleted before updating
     */
    n = config->records;
    if (config->records) {
        while (n) {
            n1 = n;
            n = n->next;
            free(n1->name);
            n1->name = NULL;
            free(n1);
            n1 = NULL;
        }
        config->records = NULL;
    }

    for (i = 0; i < cnt; i++) {
        const char* suffix = ".mct";
        int len = 0;
        len = strlen(file_name);

        mct_vlog(LOG_DEBUG,
                 "%s: Scanned file name=[%s], filter file name=[%s]\n",
                  __func__, files[i]->d_name, file_name);
        if (strncmp(files[i]->d_name, file_name, len) == 0) {
            if (config->num_files == 1 && file_config->logfile_optional_counter) {
                /* <filename>.mct or <filename>_<tmsp>.mct */
                if ((files[i]->d_name[len] == suffix[0]) ||
                    (file_config->logfile_timestamp &&
                     (files[i]->d_name[len] == file_config->logfile_delimiter))) {
                    current_idx = 1;
                } else {
                    continue;
                }
            } else {
                /* <filename>_idx.mct or <filename>_idx_<tmsp>.mct */
                if (files[i]->d_name[len] == file_config->logfile_delimiter) {
                    current_idx = mct_logstorage_get_idx_of_log_file(file_config,
                                                                     files[i]->d_name);
                } else {
                    continue;
                }
            }

            MctLogStorageFileList **tmp = NULL;

            if (config->records == NULL) {
                config->records = malloc(sizeof(MctLogStorageFileList));

                if (config->records == NULL) {
                    ret = -1;
                    mct_log(LOG_ERR, "Memory allocation failed\n");
                    break;
                }

                tmp = &config->records;
            }
            else {
                tmp = &config->records;

                while (*(tmp) != NULL)
                    tmp = &(*tmp)->next;

                *tmp = malloc(sizeof(MctLogStorageFileList));

                if (*tmp == NULL) {
                    ret = -1;
                    mct_log(LOG_ERR, "Memory allocation failed\n");
                    break;
                }
            }

            char tmpfile[MCT_OFFLINE_LOGSTORAGE_MAX_LOG_FILE_LEN + 1] = { '\0' };
            if (dir != NULL) {
                /* Append directory path */
                strcat(tmpfile, dir);
                strcat(tmpfile, "/");
            }
            strcat(tmpfile, files[i]->d_name);
            (*tmp)->name = strdup(tmpfile);
            (*tmp)->idx = current_idx;
            (*tmp)->next = NULL;
            check++;
        }
    }

    mct_vlog(LOG_DEBUG, "%s: After dir scan: [%d] files of [%s]\n", __func__,
             check, file_name);

    if (ret == 0) {
        max_idx = mct_logstorage_sort_file_name(&config->records);

        /* Fault tolerance:
         * In case there are some log files are removed but
         * the index is still not reaching maxcounter, no need
         * to perform rearrangement of filename.
         * This would help the log keeps growing until maxcounter is reached and
         * the maximum number of log files could be obtained.
         */
        if (max_idx == file_config->logfile_maxcounter)
            mct_logstorage_rearrange_file_name(&config->records);
    }

    /* free scandir result */
    for (i = 0; i < cnt; i++)
        free(files[i]);

    free(files);

    return ret;
}

/**
 * mct_logstorage_open_log_file
 *
 * Open a log file. Check storage directory for already created files and open
 * the oldest if there is enough space to store at least msg_size.
 * Otherwise create a new file, but take configured max number of files into
 * account and remove the oldest file if needed.
 *
 * @param  config    MctLogStorageFilterConfig
 * @param  file_config   User configurations for log file
 * @param  dev_path      Storage device path
 * @param  msg_size  Size of incoming message
 * @param  is_update_required   The file list needs to be updated
 * @return 0 on succes, -1 on error
 */
int mct_logstorage_open_log_file(MctLogStorageFilterConfig *config,
                                 MctLogStorageUserConfig *file_config,
                                 char *dev_path,
                                 int msg_size,
                                 bool is_update_required,
                                 bool is_sync)
{
    int ret = 0;
    char absolute_file_path[MCT_OFFLINE_LOGSTORAGE_MAX_PATH_LEN + 1] = { '\0' };
    char storage_path[MCT_MOUNT_PATH_MAX + 1] = { '\0' };
    char file_name[MCT_OFFLINE_LOGSTORAGE_MAX_LOG_FILE_LEN + 1] = { '\0' };
    unsigned int num_log_files = 0;
    struct stat s;
    memset(&s, 0, sizeof(struct stat));
    MctLogStorageFileList **tmp = NULL;
    MctLogStorageFileList **newest = NULL;

    if (config == NULL)
        return -1;

    if (strlen(dev_path) > MCT_MOUNT_PATH_MAX) {
        mct_vlog(LOG_ERR, "device path '%s' is too long to store\n", dev_path);
        return -1;
    }

    snprintf(storage_path, MCT_MOUNT_PATH_MAX, "%s/", dev_path);

    /* check if there are already files stored */
    if (config->records == NULL || is_update_required) {
        if (mct_logstorage_storage_dir_info(file_config, storage_path, config) != 0)
            return -1;
    }

    /* obtain locations of newest, current file names, file count */
    tmp = &config->records;

    while (*(tmp) != NULL) {
        num_log_files += 1;

        if ((*tmp)->next == NULL)
            newest = tmp;

        tmp = &(*tmp)->next;
    }

    /* need new file*/
    if (num_log_files == 0) {
        mct_logstorage_log_file_name(file_name,
                                     file_config,
                                     config->file_name,
                                     config->num_files,
                                     1);

        /* concatenate path and file and open absolute path */
        strcat(absolute_file_path, storage_path);
        strcat(absolute_file_path, file_name);
        config->working_file_name = strdup(file_name);
        config->log = fopen(absolute_file_path, "a+");

        /* Add file to file list */
        *tmp = malloc(sizeof(MctLogStorageFileList));

        if (*tmp == NULL) {
            mct_log(LOG_ERR, "Memory allocation for file name failed\n");
            return -1;
        }

        (*tmp)->name = strdup(file_name);
        (*tmp)->idx = 1;
        (*tmp)->next = NULL;
    }
    else {
        strcat(absolute_file_path, storage_path);

        /* newest file available
         * Since the working file is already updated from newest file info
         * So if there is already wrap-up, the newest file will be the working file
         */
        if ((config->wrap_id == 0) || (config->working_file_name == NULL)) {
            if (config->working_file_name != NULL) {
                free(config->working_file_name);
                config->working_file_name = NULL;
            }
            config->working_file_name = strdup((*newest)->name);
        }
        strcat(absolute_file_path, config->working_file_name);

        mct_vlog(LOG_DEBUG,
                 "%s: Number of log files-newest file-wrap_id [%u]-[%s]-[%u]\n",
                 __func__, num_log_files, config->working_file_name,
                 config->wrap_id);

        ret = stat(absolute_file_path, &s);

        /* if file stats is read and, either
         * is_sync is true and (other than ON_MSG sync behavior and current size is less than configured size) or
         * msg_size fit into the size (ON_MSG or par of cache needs to be written into new file), open it */
        if ((ret == 0) &&
            ((is_sync && (s.st_size < (int) config->file_size)) ||
             (!is_sync && (s.st_size + msg_size <= (int) config->file_size)))) {
            config->log = fopen(absolute_file_path, "a+");
            config->current_write_file_offset = s.st_size;
        }
        else {
            /* no space in file or file stats cannot be read */
            unsigned int idx = 0;

            /* get index of newest log file */
            if (config->num_files == 1 && file_config->logfile_optional_counter) {
                idx = 1;
            } else {
                idx = mct_logstorage_get_idx_of_log_file(file_config,
                                                         config->working_file_name);
            }

            /* Check if file logging shall be stopped */
            if (config->overwrite == MCT_LOGSTORAGE_OVERWRITE_DISCARD_NEW) {
                mct_vlog(LOG_DEBUG,
                         "%s: num_files=%d, current_idx=%d (filename=%s)\n",
                         __func__, config->num_files, idx,
                         config->file_name);

                if (config->num_files == idx) {
                    mct_vlog(LOG_INFO,
                             "%s: logstorage limit reached, stopping capture for filter: %s\n",
                             __func__, config->file_name);
                    config->skip = 1;
                    return 0;
                }
            }

            idx += 1;

            /* wrap around if max index is reached or an error occurred
             * while calculating index from file name */
            if ((idx > file_config->logfile_maxcounter) || (idx == 0)) {
                idx = 1;
                config->wrap_id += 1;
            }

            mct_logstorage_log_file_name(file_name,
                                         file_config,
                                         config->file_name,
                                         config->num_files,
                                         idx);

            /* concatenate path and file and open absolute path */
            memset(absolute_file_path,
                   0,
                   sizeof(absolute_file_path) / sizeof(char));
            strcat(absolute_file_path, storage_path);
            strcat(absolute_file_path, file_name);

            if(config->working_file_name) {
                free(config->working_file_name);
                config->working_file_name = strdup(file_name);
            }

            /* If there is already wrap-up, check the existence of file
             * remove it and reopen it.
             * In this case number of log file won't be increased*/
            if (config->wrap_id && stat(absolute_file_path, &s) == 0) {
                remove(absolute_file_path);
                num_log_files -= 1;
                mct_vlog(LOG_DEBUG,
                         "%s: Remove '%s' (num_log_files: %u, config->num_files:%u)\n",
                         __func__, absolute_file_path, num_log_files, config->num_files);
            }

            config->log = fopen(absolute_file_path, "w+");

            mct_vlog(LOG_DEBUG,
                     "%s: Filename and Index after updating [%s]-[%u]\n",
                     __func__, file_name, idx);

            /* Add file to file list */
            *tmp = malloc(sizeof(MctLogStorageFileList));

            if (*tmp == NULL) {
                mct_log(LOG_ERR, "Memory allocation for file name failed\n");
                return -1;
            }

            (*tmp)->name = strdup(file_name);
            (*tmp)->idx = idx;
            (*tmp)->next = NULL;

            num_log_files += 1;

            /* check if number of log files exceeds configured max value */
            if (num_log_files > config->num_files) {
                if (!(config->num_files == 1 && file_config->logfile_optional_counter)) {
                    /* delete oldest */
                    MctLogStorageFileList **head = &config->records;
                    MctLogStorageFileList *n = *head;
                    memset(absolute_file_path,
                           0,
                           sizeof(absolute_file_path) / sizeof(char));
                    strcat(absolute_file_path, storage_path);
                    strcat(absolute_file_path, (*head)->name);
                    mct_vlog(LOG_DEBUG,
                             "%s: Remove '%s' (num_log_files: %d, config->num_files:%d, file_name:%s)\n",
                             __func__, absolute_file_path, num_log_files,
                             config->num_files, config->file_name);
                    remove(absolute_file_path);

                    free((*head)->name);
                    (*head)->name = NULL;
                    *head = n->next;
                    n->next = NULL;
                    free(n);
                }
            }

        }
    }

    if (config->log == NULL) {
        if (*tmp != NULL) {
            if ((*tmp)->name != NULL) {
                free((*tmp)->name);
                (*tmp)->name = NULL;
            }
            free(*tmp);
            *tmp = NULL;
        }

        if (config->working_file_name != NULL) {
            free(config->working_file_name);
            config->working_file_name = NULL;
        }

        mct_vlog(LOG_ERR, "%s: Unable to open log file.\n", __func__);
        return -1;
    }

    return ret;
}

/**
 * mct_logstorage_find_mct_header
 *
 * search for mct header in cache
 *
 * @param ptr         cache starting position
 * @param offset      offset
 * @param cnt         count
 * @return index on success, -1 on error
 */
static int mct_logstorage_find_mct_header(void *ptr,
                                              unsigned int offset,
                                              unsigned int cnt)
{
    const char magic[] = { 'D', 'L', 'T', 0x01 };
    const char *cache = (char*)ptr + offset;

    unsigned int i;
    for (i = 0; i < cnt; i++) {
        if ((cache[i] == 'D') && (strncmp(&cache[i], magic, 4) == 0))
           return i;
    }

    return -1;
}

/**
 * mct_logstorage_find_last_mct_header
 *
 * search for last mct header in cache
 *
 * @param ptr         cache starting position
 * @param offset      offset
 * @param cnt         count
 * @return index on success, -1 on error
 */
static int mct_logstorage_find_last_mct_header(void *ptr,
                                                   unsigned int offset,
                                                   unsigned int cnt)
{
    const char magic[] = {'D', 'L', 'T', 0x01};
    const char *cache = (char*)ptr + offset;

    int i;
    for (i = cnt - (MCT_ID_SIZE - 1) ; i > 0; i--) {
        if ((cache[i] == 'D') && (strncmp(&cache[i], magic, 4) == 0))
            return i;
    }

    return -1;
}

/**
 * mct_logstorage_check_write_ret
 *
 * check the return value of fwrite
 *
 * @param config      MctLogStorageFilterConfig
 * @param ret         return value of fwrite call
 */
static void mct_logstorage_check_write_ret(MctLogStorageFilterConfig *config,
                                               int ret)
{
    if (config == NULL)
        mct_vlog(LOG_ERR, "%s: cannot retrieve config information\n", __func__);

    if (ret <= 0) {
        if (ferror(config->log) != 0)
            mct_vlog(LOG_ERR, "%s: failed to write cache into log file\n", __func__);
    }
    else {
        /* force sync */
        if (fflush(config->log) != 0)
            mct_vlog(LOG_ERR, "%s: failed to flush log file\n", __func__);

        if (fsync(fileno(config->log)) != 0)
            /* some filesystem doesn't support fsync() */
            if (errno != ENOSYS)
            {
                mct_vlog(LOG_ERR, "%s: failed to sync log file\n", __func__);
            }
    }
}

/**
 * mct_logstorage_sync_to_file
 *
 * Write the log message to log file
 *
 * @param config        MctLogStorageFilterConfig
 * @param file_config   MctLogStorageUserConfig
 * @param dev_path      Storage device mount point path
 * @param footer        MctLogStorageCacheFooter
 * @param start_offset  Start offset of the cache
 * @param end_offset    End offset of the cache
 * @return 0 on success, -1 on error
 */
static int mct_logstorage_sync_to_file(MctLogStorageFilterConfig *config,
                                           MctLogStorageUserConfig *file_config,
                                           char *dev_path,
                                           MctLogStorageCacheFooter *footer,
                                           unsigned int start_offset,
                                           unsigned int end_offset)
{
    int ret = 0;
    int start_index = 0;
    int end_index = 0;
    int count = 0;
    int remain_file_size = 0;

    if ((config == NULL) || (file_config == NULL) || (dev_path == NULL) ||
        (footer == NULL))
    {
        mct_vlog(LOG_ERR, "%s: cannot retrieve config information\n", __func__);
        return -1;
    }

    count = end_offset - start_offset;

    /* In case of cached-based strategy, the newest file information
     * must be updated everytime of synchronization.
     */
    if (config->log) {
        fclose(config->log);
        config->log = NULL;
        config->current_write_file_offset = 0;
    }

    if (mct_logstorage_open_log_file(config, file_config,
            dev_path, count, true, true) != 0) {
        mct_vlog(LOG_ERR, "%s: failed to open log file\n", __func__);
        return -1;
    }

    if (config->skip == 1) {
        return 0;
    }

    remain_file_size = config->file_size - config->current_write_file_offset;

    if (count > remain_file_size)
    {
        /* Check if more than one message can fit into the remaining file */
        start_index = mct_logstorage_find_mct_header(config->cache, start_offset,
                                                     remain_file_size);
        end_index = mct_logstorage_find_last_mct_header(config->cache,
                                                     start_offset + start_index,
                                                     remain_file_size - start_index);
        count = end_index - start_index;

        if ((start_index >= 0) && (end_index > start_index) &&
            (count > 0) && (count <= remain_file_size))
        {
            ret = fwrite((uint8_t*)config->cache + start_offset + start_index,
                        count, 1, config->log);
            mct_logstorage_check_write_ret(config, ret);

            /* Close log file */
            fclose(config->log);
            config->log = NULL;
            config->current_write_file_offset = 0;

            footer->last_sync_offset = start_offset + count;
            start_offset = footer->last_sync_offset;
        }
        else
        {
            fclose(config->log);
            config->log = NULL;
            config->current_write_file_offset = 0;
        }
    }

    start_index = mct_logstorage_find_mct_header(config->cache, start_offset, count);
    count = end_offset - start_offset - start_index;

    if ((start_index >= 0) && (count > 0))
    {
        /* Prepare log file */
        if (config->log == NULL)
        {
            if (mct_logstorage_open_log_file(config, file_config, dev_path,
                                             count, true, false) != 0)
            {
                mct_vlog(LOG_ERR, "%s: failed to open log file\n", __func__);
                return -1;
            }

            if (config->skip == 1)
            {
                return 0;
            }
        }

        ret = fwrite((uint8_t*)config->cache + start_offset + start_index, count, 1,
                     config->log);
        mct_logstorage_check_write_ret(config, ret);

        config->current_write_file_offset += count;
        footer->last_sync_offset = end_offset;
    }

    footer->wrap_around_cnt = 0;

    return 0;
}

/**
 * mct_logstorage_prepare_on_msg
 *
 * Prepare the log file for a certain filer. If log file not open or log
 * files max size reached, open a new file.
 *
 * @param config        MctLogStorageFilterConfig
 * @param file_config   User configurations for log file
 * @param dev_path      Storage device path
 * @param log_msg_size  Size of log message
 * @param newest_file_info   Info of newest file for corresponding filename
 * @return 0 on success, -1 on error
 */
int mct_logstorage_prepare_on_msg(MctLogStorageFilterConfig *config,
                                  MctLogStorageUserConfig *file_config,
                                  char *dev_path,
                                  int log_msg_size,
                                  MctNewestFileName *newest_file_info)
{
    int ret = 0;
    struct stat s;

    if ((config == NULL) || (file_config == NULL) || (dev_path == NULL) ||
        (newest_file_info == NULL)) {
        mct_vlog(LOG_INFO, "%s: Wrong paratemters\n", __func__);
        return -1;
    }

    /* This is for ON_MSG/UNSET strategy */
    if (config->log == NULL) {
        /* Sync the wrap id and working file name before opening log file */
        if (config->wrap_id < newest_file_info->wrap_id) {
            config->wrap_id = newest_file_info->wrap_id;
            if (config->working_file_name) {
                free(config->working_file_name);
                config->working_file_name = NULL;
            }
            config->working_file_name = strdup(newest_file_info->newest_file);
        }

        /* open a new log file */
        ret = mct_logstorage_open_log_file(config,
                                           file_config,
                                           dev_path,
                                           log_msg_size,
                                           true,
                                           false);
    }
    else { /* already open, check size and create a new file if needed */
        ret = fstat(fileno(config->log), &s);

        if (ret == 0) {
            /* check if adding new data do not exceed max file size */
            /* Check if wrap id needs to be updated*/
            if ((s.st_size + log_msg_size > (int)config->file_size) ||
                (strcmp(config->working_file_name, newest_file_info->newest_file) != 0) ||
                (config->wrap_id < newest_file_info->wrap_id)) {

                /* Sync only if on_msg */
                if ((config->sync == MCT_LOGSTORAGE_SYNC_ON_MSG) ||
                    (config->sync == MCT_LOGSTORAGE_SYNC_UNSET)) {
                    if (fsync(fileno(config->log)) != 0) {
                        if (errno != ENOSYS) {
                            mct_vlog(LOG_ERR, "%s: failed to sync log file\n", __func__);
                        }
                    }
                }

                fclose(config->log);
                config->log = NULL;

                /* Sync the wrap id and working file name before opening log file */
                if (config->wrap_id <= newest_file_info->wrap_id) {
                    config->wrap_id = newest_file_info->wrap_id;
                    if (config->working_file_name) {
                        free(config->working_file_name);
                        config->working_file_name = NULL;
                    }
                    config->working_file_name = strdup(newest_file_info->newest_file);
                }

                ret = mct_logstorage_open_log_file(config,
                                                   file_config,
                                                   dev_path,
                                                   log_msg_size,
                                                   true,
                                                   false);
            }
            else { /*everything is prepared */
                ret = 0;
            }
        }
        else {
            mct_vlog(LOG_ERR, "%s: stat() failed.\n", __func__);
            ret = -1;
        }
    }

    return ret;
}

/**
 * mct_logstorage_write_on_msg
 *
 * Write the log message.
 *
 * @param config        MctLogStorageFilterConfig
 * @param file_config   MctLogStorageUserConfig
 * @param dev_path      Path to device
 * @param data1         header
 * @param size1         header size
 * @param data2         storage header
 * @param size2         storage header size
 * @param data3         payload
 * @param size3         payload size
 * @return 0 on success, -1 on error
 */
int mct_logstorage_write_on_msg(MctLogStorageFilterConfig *config,
                                MctLogStorageUserConfig *file_config,
                                char *dev_path,
                                unsigned char *data1,
                                int size1,
                                unsigned char *data2,
                                int size2,
                                unsigned char *data3,
                                int size3)
{
    int ret;

    if ((config == NULL) || (data1 == NULL) || (data2 == NULL) || (data3 == NULL) ||
        (file_config == NULL) || (dev_path == NULL))
    {
        return -1;
    }

    ret = fwrite(data1, 1, size1, config->log);

    if (ret != size1)
        mct_log(LOG_WARNING, "Wrote less data than specified\n");

    ret = fwrite(data2, 1, size2, config->log);

    if (ret != size2)
        mct_log(LOG_WARNING, "Wrote less data than specified\n");

    ret = fwrite(data3, 1, size3, config->log);

    if (ret != size3)
        mct_log(LOG_WARNING, "Wrote less data than specified\n");

    return ferror(config->log);
}

/**
 * mct_logstorage_sync_on_msg
 *
 * sync data to disk.
 *
 * @param config        MctLogStorageFilterConfig
 * @param file_config   User configurations for log file
 * @param dev_path      Storage device path
 * @param status        Strategy flag
 * @return 0 on success, -1 on error
 */
int mct_logstorage_sync_on_msg(MctLogStorageFilterConfig *config,
                               MctLogStorageUserConfig *file_config,
                               char *dev_path,
                               int status)
{
    int ret;

    (void)file_config;  /* satisfy compiler */
    (void)dev_path;

    if (config == NULL)
        return -1;

    if (status == MCT_LOGSTORAGE_SYNC_ON_MSG) { /* sync on every message */
        ret = fflush(config->log);

        if (ret != 0)
            mct_log(LOG_ERR, "fflush failed\n");

    }

    return 0;
}

/**
 * mct_logstorage_prepare_msg_cache
 *
 * Prepare the log file for a certain filer. If log file not open or log
 * files max size reached, open a new file.
 * Create a memory area to cache data.
 *
 * @param config        MctLogStorageFilterConfig
 * @param file_config   User configurations for log file
 * @param dev_path      Storage device path
 * @param log_msg_size  Size of log message
 * @param newest_file_info   Info of newest files for corresponding filename
 * @return 0 on success, -1 on error
 */
int mct_logstorage_prepare_msg_cache(MctLogStorageFilterConfig *config,
                                     MctLogStorageUserConfig *file_config,
                                     char *dev_path,
                                     int log_msg_size,
                                     MctNewestFileName *newest_file_info )
{
    if ((config == NULL) || (file_config == NULL) ||
            (dev_path == NULL) || (newest_file_info == NULL))
        return -1;

    /* check if newest file info is available
     * + working file name is NULL => update directly to newest file
     * + working file name is not NULL: check if
     * ++ wrap_ids are different from each other or
     * ++ newest file name <> working file name
     */
    if (newest_file_info->newest_file) {
        if (config->working_file_name &&
                ((config->wrap_id != newest_file_info->wrap_id) ||
                (strcmp(newest_file_info->newest_file, config->working_file_name) != 0))) {
            free(config->working_file_name);
            config->working_file_name = NULL;
        }
        if (config->working_file_name == NULL) {
            config->working_file_name = strdup(newest_file_info->newest_file);
            config->wrap_id = newest_file_info->wrap_id;
        }
    }

    /* Combinations allowed: on Daemon_Exit with on Demand,File_Size with Daemon_Exit
     *  File_Size with on Demand, Specific_Size with Daemon_Exit,Specific_Size with on Demand
     * Combination not allowed : File_Size with Specific_Size
     */
    /* check for combinations of specific_size and file_size strategy */
    if ((MCT_OFFLINE_LOGSTORAGE_IS_STRATEGY_SET(config->sync, MCT_LOGSTORAGE_SYNC_ON_SPECIFIC_SIZE) > 0) &&
        ((MCT_OFFLINE_LOGSTORAGE_IS_STRATEGY_SET(config->sync, MCT_LOGSTORAGE_SYNC_ON_FILE_SIZE)) > 0)) {
        mct_log(LOG_WARNING, "wrong combination of sync strategies \n");
        return -1;
    }

    (void)log_msg_size; /* satisfy compiler */

    /* check specific size is smaller than file size */
    if ((MCT_OFFLINE_LOGSTORAGE_IS_STRATEGY_SET(config->sync,
                     MCT_LOGSTORAGE_SYNC_ON_SPECIFIC_SIZE) > 0) &&
                     (config->specific_size > config->file_size))
    {
        mct_log(LOG_ERR,
                "Cache size is larger than file size. "
                "Cannot prepare log file for ON_SPECIFIC_SIZE sync\n");
        return -1;
    }

    if (config->cache == NULL)
    {
        unsigned int cache_size = 0;

        /* check for sync_specific_size strategy */
        if (MCT_OFFLINE_LOGSTORAGE_IS_STRATEGY_SET(config->sync,
               MCT_LOGSTORAGE_SYNC_ON_SPECIFIC_SIZE) > 0)
        {
            cache_size = config->specific_size;
        }
        else  /* other cache strategies */
        {
            cache_size = config->file_size;
        }

        /* check total logstorage cache size */
        if ((g_logstorage_cache_size + cache_size +
             sizeof(MctLogStorageCacheFooter)) >
             g_logstorage_cache_max)
        {
            mct_vlog(LOG_ERR,
                     "%s: Max size of Logstorage Cache already used. (ApId=[%s] CtId=[%s]) \n",
                     __func__, config->apids, config->ctids);
            return -1;
        } else {
            mct_vlog(LOG_DEBUG,
                     "%s: Logstorage total: %d , requested cache size: %d, max: %d (ApId=[%s] CtId=[%s])\n",
                     __func__, g_logstorage_cache_size, cache_size,
                     g_logstorage_cache_max, config->apids, config->ctids);
        }

        /* create cache */
        config->cache = calloc(1, cache_size + sizeof(MctLogStorageCacheFooter));

        if (config->cache == NULL)
        {
            mct_log(LOG_CRIT,
                    "Cannot allocate memory for filter ring buffer\n");
        }
        else
        {
            /* update current used cache size */
            g_logstorage_cache_size += cache_size + sizeof(MctLogStorageCacheFooter);
        }
    }

    return 0;
}

/**
 * mct_logstorage_write_msg_cache
 *
 * Write the log message.
 *
 * @param config        MctLogStorageFilterConfig
 * @param file_config   User configurations for log file
 * @param dev_path      Storage device path
 * @param data1         header
 * @param size1         header size
 * @param data2         storage header
 * @param size2         storage header size
 * @param data3         payload
 * @param size3         payload size
 * @return 0 on success, -1 on error
 */
int mct_logstorage_write_msg_cache(MctLogStorageFilterConfig *config,
                                   MctLogStorageUserConfig *file_config,
                                   char *dev_path,
                                   unsigned char *data1,
                                   int size1,
                                   unsigned char *data2,
                                   int size2,
                                   unsigned char *data3,
                                   int size3)
{
    MctLogStorageCacheFooter *footer = NULL;
    int msg_size;
    int remain_cache_size;
    uint8_t *curr_write_addr = NULL;
    int ret = 0;
    unsigned int cache_size;

    if ((config == NULL) || (data1 == NULL) || (size1 < 0) || (data2 == NULL) ||
        (size2 < 0) || (data3 == NULL) || (size3 < 0) || (config->cache == NULL) ||
        (file_config == NULL) || (dev_path == NULL))
    {
        return -1;
    }

    if (MCT_OFFLINE_LOGSTORAGE_IS_STRATEGY_SET(config->sync,
                                     MCT_LOGSTORAGE_SYNC_ON_SPECIFIC_SIZE) > 0)
    {
        cache_size = config->specific_size;
    }
    else
    {
        cache_size = config->file_size;
    }

    footer = (MctLogStorageCacheFooter *)((uint8_t*)config->cache + cache_size);
    if (footer == NULL)
    {
        mct_log(LOG_ERR, "Cannot retrieve cache footer. Address is NULL\n");
        return -1;
    }
    msg_size = size1 + size2 + size3;
    remain_cache_size = cache_size - footer->offset;

    if (msg_size <= remain_cache_size) /* add at current position */
    {
        curr_write_addr = (uint8_t*)config->cache + footer->offset;
        footer->offset += msg_size;
        if (footer->wrap_around_cnt < 1) {
            footer->end_sync_offset = footer->offset;
        }

        /* write data to cache */
        memcpy(curr_write_addr, data1, size1);
        curr_write_addr += size1;
        memcpy(curr_write_addr, data2, size2);
        curr_write_addr += size2;
        memcpy(curr_write_addr, data3, size3);
    }

    /*
     * In case the msg_size is equal to remaining cache size,
     * the message is still written in cache.
     * Then whole cache data is synchronized to file.
     */
    if (msg_size >= remain_cache_size)
    {
        /*check for message size exceeds cache size for specific_size strategy */
        if ((unsigned int) msg_size > cache_size)
        {
            mct_log(LOG_WARNING, "Message is larger than cache. Discard.\n");
            return -1;
        }

         /*sync to file for specific_size or file_size  */
         if (MCT_OFFLINE_LOGSTORAGE_IS_STRATEGY_SET(config->sync,
                                                    MCT_LOGSTORAGE_SYNC_ON_FILE_SIZE) > 0)
         {
             ret = config->mct_logstorage_sync(config,
                                               file_config,
                                               dev_path,
                                               MCT_LOGSTORAGE_SYNC_ON_FILE_SIZE);
             if (ret != 0)
             {
                 mct_log(LOG_ERR,"mct_logstorage_sync: Unable to sync.\n");
                 return -1;
             }
         }
         else if (MCT_OFFLINE_LOGSTORAGE_IS_STRATEGY_SET(config->sync,
                                                         MCT_LOGSTORAGE_SYNC_ON_SPECIFIC_SIZE) > 0)
         {

             ret = config->mct_logstorage_sync(config,
                                               file_config,
                                               dev_path,
                                               MCT_LOGSTORAGE_SYNC_ON_SPECIFIC_SIZE);
             if (ret != 0)
             {
                 mct_log(LOG_ERR,"mct_logstorage_sync: Unable to sync.\n");
                 return -1;
             }
         }
         else if ((MCT_OFFLINE_LOGSTORAGE_IS_STRATEGY_SET(config->sync,
                                                         MCT_LOGSTORAGE_SYNC_ON_DEMAND) > 0) ||
                  (MCT_OFFLINE_LOGSTORAGE_IS_STRATEGY_SET(config->sync,
                                                         MCT_LOGSTORAGE_SYNC_ON_DAEMON_EXIT) > 0))
         {
             footer->wrap_around_cnt += 1;
         }

         if (msg_size > remain_cache_size)
         {
            /* start writing from beginning */
            footer->end_sync_offset = footer->offset;
            curr_write_addr = config->cache;
            footer->offset = msg_size;

            /* write data to cache */
            memcpy(curr_write_addr, data1, size1);
            curr_write_addr += size1;
            memcpy(curr_write_addr, data2, size2);
            curr_write_addr += size2;
            memcpy(curr_write_addr, data3, size3);
         }
    }


    return 0;
}

/**
 * mct_logstorage_sync_msg_cache
 *
 * sync data to disk.
 *
 * @param config        MctLogStorageFilterConfig
 * @param file_config   User configurations for log file
 * @param dev_path      Storage device path
 * @param status        Strategy flag
 * @return 0 on success, -1 on error
 */
int mct_logstorage_sync_msg_cache(MctLogStorageFilterConfig *config,
                                  MctLogStorageUserConfig *file_config,
                                  char *dev_path,
                                  int status)
{
    unsigned int cache_size;

    MctLogStorageCacheFooter *footer = NULL;

    if ((config == NULL) || (file_config == NULL) || (dev_path == NULL))
    {
        return -1;
    }

    /* sync only, if given strategy is set */
    if (MCT_OFFLINE_LOGSTORAGE_IS_STRATEGY_SET(config->sync, status) > 0)
    {
        if (config->cache == NULL)
        {
            mct_log(LOG_ERR,
                    "Cannot copy cache to file. Cache is NULL\n");
            return -1;
        }

        if (MCT_OFFLINE_LOGSTORAGE_IS_STRATEGY_SET(config->sync,
                                                   MCT_LOGSTORAGE_SYNC_ON_SPECIFIC_SIZE) > 0)
        {
            cache_size = config->specific_size;
        }
        else
        {
            cache_size = config->file_size;
        }

        footer = (MctLogStorageCacheFooter *)((uint8_t*)config->cache + cache_size);
        if (footer == NULL)
        {
            mct_log(LOG_ERR, "Cannot retrieve cache information\n");
            return -1;
        }

        /* sync cache data to file */
        if (footer->wrap_around_cnt < 1)
        {
            /* Sync whole cache */
            mct_logstorage_sync_to_file(config, file_config, dev_path, footer,
                                        footer->last_sync_offset, footer->offset);

        }
        else if ((footer->wrap_around_cnt == 1) &&
                 (footer->offset < footer->last_sync_offset))
        {
            /* sync (1) footer->last_sync_offset to footer->end_sync_offset,
             * and (2) footer->last_sync_offset (= 0) to footer->offset */
            mct_logstorage_sync_to_file(config, file_config, dev_path, footer,
                                        footer->last_sync_offset, footer->end_sync_offset);
            footer->last_sync_offset = 0;
            mct_logstorage_sync_to_file(config, file_config, dev_path, footer,
                                        footer->last_sync_offset, footer->offset);
        }
        else
        {
            /* sync (1) footer->offset + index to footer->end_sync_offset,
             * and (2) footer->last_sync_offset (= 0) to footer->offset */
            mct_logstorage_sync_to_file(config, file_config, dev_path, footer,
                                        footer->offset, footer->end_sync_offset);
            footer->last_sync_offset = 0;
            mct_logstorage_sync_to_file(config, file_config, dev_path, footer,
                                        footer->last_sync_offset, footer->offset);
        }

        /* Initialize cache if needed */
        if ((status == MCT_LOGSTORAGE_SYNC_ON_SPECIFIC_SIZE) ||
            (status == MCT_LOGSTORAGE_SYNC_ON_FILE_SIZE))
        {
            /* clean ring buffer and reset footer information */
            memset(config->cache, 0,
                   cache_size + sizeof(MctLogStorageCacheFooter));
        }

        if (status == MCT_LOGSTORAGE_SYNC_ON_FILE_SIZE)
        {
            /* Close log file */
            if (config->log != NULL) {
                fclose(config->log);
                config->log = NULL;
                config->current_write_file_offset = 0;
            }
        }
    }
    return 0;
}
