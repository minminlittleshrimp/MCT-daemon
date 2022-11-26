#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <sys/time.h>
#include <math.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <time.h>
#include <dirent.h>
#include <syslog.h>
#include <sys/stat.h>

#include "mct_user.h"
#include "mct_user_shared.h"
#include "mct_user_shared_cfg.h"
#include "mct_user_cfg.h"

extern MctUser mct_user;
extern sem_t mct_mutex;
static int mct_hp_ctid_cnt = 0;
static int mct_nw_trace_hp_cfg = -1;
MctContext context_injection_callback;

int mct_ext_injection_macro_callback(uint32_t service_id, void *data,
                                     uint32_t length);

void mct_ext_set_ringbuf_header(MctExtBuff *pMctExtBuff)
{
    MctExtBuffHeader *pBufHeader;

    if (pMctExtBuff == NULL) {
        return;
    }

    /* initialize ring buffer */
    pBufHeader = (MctExtBuffHeader *)pMctExtBuff->addr;
    pBufHeader->write_count = 0;
    pBufHeader->size = pMctExtBuff->size;

    /* initialize common log header */
    memcpy(pBufHeader->ecuid, mct_user.ecuID, sizeof(pBufHeader->ecuid));
    pBufHeader->htyp = MCT_HTYP_PROTOCOL_VERSION1;

    /* send ecu id */
    if (mct_user.with_ecu_id) {
        pBufHeader->htyp |= MCT_HTYP_WEID;
    }

    /* send timestamp */
    if (mct_user.with_timestamp) {
        pBufHeader->htyp |= MCT_HTYP_WTMS;
    }

    /* send session id */
    if (mct_user.with_session_id) {
        pBufHeader->htyp |= MCT_HTYP_WSID;
    }

    if (mct_user.verbose_mode) {
        /* In verbose mode, send extended header */
        pBufHeader->htyp |= MCT_HTYP_UEH;
    } else {
        /* In non-verbose, send extended header if desired */
        if (mct_user.use_extended_header_for_non_verbose) {
            pBufHeader->htyp |= MCT_HTYP_UEH;
        }
    }

#if (BYTE_ORDER == BIG_ENDIAN)
    pBufHeader->htyp |= MCT_HTYP_MSBF;
#endif
    pBufHeader->msin = MCT_EXT_LOG_HEADER_MSIN;
    pBufHeader->noar = MCT_EXT_LOG_HEADER_NOAR;
    memcpy(pBufHeader->apid, mct_user.appID, sizeof(pBufHeader->apid));
    memcpy(pBufHeader->ctid, pMctExtBuff->ctid, sizeof(pBufHeader->ctid));
    pBufHeader->type_info_header = MCT_TYPE_INFO_STRG | MCT_SCOD_ASCII;
    pBufHeader->type_info_payload = MCT_TYPE_INFO_RAWD;

    return;
}

int mct_ext_check_extension(char *filename, char *extension)
{
    int ret = 0;
    int body_len;
    int ext_len;

    ext_len = strlen(extension);
    body_len = strlen(filename) - ext_len;

    if (body_len < 0) {
        ret = 1;
    } else {
        filename += body_len;

        if (strncmp(filename, extension, ext_len) != 0) {
            ret = 1;
        }
    }

    return ret;
}

int mct_ext_parse_filename(char *filename, char *ctid, char *apid, int *pid,
                           int *bknum)
{
    int ret = 0;
    char *rp;
    uint8_t count;
    char apid_old[MCT_ID_SIZE + 1];
    char ctid_old[MCT_ID_SIZE + 1];
    char pid_ascii[MCT_EXT_PID_MAX_LEN + 1];
    char bknum_ascii[MCT_EXT_BKNUM_LEN + 1];
    int bknum_detect = 0;

    /* mct_ctid_apid_pid_bknum.xxx */
    /* skip to ctid */
    rp = filename + sizeof(MCT_EXT_CTID_LOG_FILE_PREFIX) - 1
        + sizeof(MCT_EXT_CTID_LOG_FILE_DELIMITER) - 1;

    /* get ctid */
    for (count = 0; count < MCT_ID_SIZE + 1; count++) {
        if (*rp == 0) {
            return MCT_RETURN_ERROR;
        }

        ret = memcmp(rp, MCT_EXT_CTID_LOG_FILE_DELIMITER,
                     sizeof(MCT_EXT_CTID_LOG_FILE_DELIMITER) - 1);

        if (ret == 0) {
            break;
        }

        ctid_old[count] = *rp;
        rp++;
    }

    if (count == MCT_ID_SIZE + 1) {
        return MCT_RETURN_ERROR;
    }

    ctid_old[count] = 0;
    rp += (sizeof(MCT_EXT_CTID_LOG_FILE_DELIMITER) - 1);

    /* get apid */
    for (count = 0; count < MCT_ID_SIZE + 1; count++) {
        if (*rp == 0) {
            return MCT_RETURN_ERROR;
        }

        ret = memcmp(rp, MCT_EXT_CTID_LOG_FILE_DELIMITER,
                     sizeof(MCT_EXT_CTID_LOG_FILE_DELIMITER) - 1);

        if (ret == 0) {
            break;
        }

        apid_old[count] = *rp;
        rp++;
    }

    if (count == MCT_ID_SIZE + 1) {
        return MCT_RETURN_ERROR;
    }

    apid_old[count] = 0;
    rp += (sizeof(MCT_EXT_CTID_LOG_FILE_DELIMITER) - 1);

    /* check pid */
    for (count = 0; count < MCT_EXT_PID_MAX_LEN + 1; count++) {
        if (*rp == 0) {
            return MCT_RETURN_ERROR;
        }

        ret = memcmp(rp, MCT_EXT_CTID_LOG_PERIOD,
                     sizeof(MCT_EXT_CTID_LOG_PERIOD) - 1);

        if (ret == 0) {
            bknum_detect = 0; /* Rear char is not backup number */
            break;
        }

        ret = memcmp(rp, MCT_EXT_CTID_LOG_FILE_BKNUM_DELI,
                     sizeof(MCT_EXT_CTID_LOG_FILE_BKNUM_DELI) - 1);

        if (ret == 0) {
            bknum_detect = 1; /* Rear char is backup number */
            break;
        }

        pid_ascii[count] = *rp;
        rp++;
    }

    if (count == MCT_EXT_PID_MAX_LEN + 1) {
        return MCT_RETURN_ERROR;
    }

    pid_ascii[count] = 0;
    *pid = atoi(pid_ascii);
    rp += (sizeof(MCT_EXT_CTID_LOG_FILE_BKNUM_DELI) - 1);

    if (bknum_detect == 1) {
        /* get backup number */
        for (count = 0; count < MCT_EXT_BKNUM_LEN + 1; count++) {
            if (*rp == 0) {
                return MCT_RETURN_ERROR;
            }

            ret = memcmp(rp, MCT_EXT_CTID_LOG_PERIOD,
                         sizeof(MCT_EXT_CTID_LOG_PERIOD) - 1);

            if (ret == 0) {
                break;
            }

            bknum_ascii[count] = *rp;
            rp++;
        }

        if (count == MCT_EXT_BKNUM_LEN + 1) {
            return MCT_RETURN_ERROR;
        }

        bknum_ascii[count] = 0;
        *bknum = atoi(bknum_ascii);
    } else {
        *bknum = -1;
    }

    if ((strncmp(ctid, ctid_old, MCT_ID_SIZE) == 0)
        && (strncmp(apid, apid_old, MCT_ID_SIZE) == 0)) {
        return MCT_RETURN_OK;
    }

    return MCT_RETURN_ERROR;
}

void mct_ext_backup_ringbuf(MctExtBuff *pMctExtBuff)
{
    DIR *dir;
    struct dirent *dent;
    struct stat sb;
    int ret;
    int pid;
    int bknum;
    int bknum_max = -1;
    int target_detect = 0;
    int log_len;
    char log_file_path[MCT_PATH_MAX] = {'\0'};
    char apid[MCT_ID_SIZE + 1] = {'\0'};
    char ctid[MCT_ID_SIZE + 1] = {'\0'};
    char target_log_extention[] = "log";
    char rename_file[MCT_EXT_CTID_LOG_FILEPATH_LEN + 1] = {'\0'};
    char rename_filepath[MCT_PATH_MAX] = {'\0'};
    char old_log_filepath[MCT_PATH_MAX] = {'\0'};

    mct_set_id(ctid, pMctExtBuff->ctid);
    mct_set_id(apid, mct_user.appID);

    dir = opendir(MCT_EXT_CTID_LOG_DIRECTORY);

    if (dir == NULL) {
        return;
    }

    while ((dent = readdir(dir)) != NULL) {
        snprintf(log_file_path, MCT_PATH_MAX, "%s%s",
                 MCT_EXT_CTID_LOG_DIRECTORY, dent->d_name);

        if ((stat(log_file_path, &sb) < 0) && (errno != ENOENT)) {
            mct_log(LOG_DEBUG, " Don't find file ERROR\n");
            continue;
        }

        if (!S_ISREG(sb.st_mode)) {
            mct_log(LOG_DEBUG, " No file\n");
            continue;
        }

        /* check extention(if not ".log", then check next file) */
        ret = mct_ext_check_extension(dent->d_name, target_log_extention);

        if (ret == 1) {
            mct_log(LOG_DEBUG, " Not log file\n");
            continue;
        }

        /* check matching for target CTID and APID and get pid and backup number */
        ret = mct_ext_parse_filename(dent->d_name, ctid, apid, &pid, &bknum);

        if (ret == MCT_RETURN_OK) {
            mct_log(LOG_DEBUG, "Same Ctid and Apid, previous log file\n");

            if (bknum_max < bknum) {
                bknum_max = bknum;
            }

            if (bknum == -1) {
                target_detect = 1;
                log_len = strlen(dent->d_name);
                strncpy(rename_file, dent->d_name,
                        MCT_EXT_CTID_LOG_FILEPATH_LEN);

                if (MCT_EXT_CTID_LOG_FILEPATH_LEN
                    < (log_len
                       - (sizeof(MCT_EXT_CTID_LOG_FILE_EXTENSION) - 1))) {
                    mct_log(LOG_ERR, "MAX length of filename is over\n");
                    return;
                }

                rename_file[log_len
                            - (sizeof(MCT_EXT_CTID_LOG_FILE_EXTENSION) - 1)] = 0;
            }
        }
    }

    closedir(dir);

    if (target_detect != 0) {
        snprintf(rename_filepath, MCT_PATH_MAX, "%s%s%s",
                 MCT_EXT_CTID_LOG_DIRECTORY, rename_file,
                 MCT_EXT_CTID_LOG_FILE_EXTENSION);

        if (bknum_max == MCT_EXT_BKNUM_MAX) {
            mct_log(LOG_DEBUG, " Previous log filename remove!\n");
            remove(rename_filepath);
        } else {
            mct_log(LOG_DEBUG, " Previous log filename change!\n");
            bknum_max = bknum_max + 1;
            snprintf(old_log_filepath, MCT_PATH_MAX,
                     "%s%s_%d%s", MCT_EXT_CTID_LOG_DIRECTORY, rename_file,
                     bknum_max, MCT_EXT_CTID_LOG_FILE_EXTENSION);
            rename(rename_filepath, old_log_filepath);
        }
    }

    return;
}

void mct_ext_get_log_filepath(char *filepath_p, char *filename_p,
                              char *extention_p)
{
    filepath_p[0] = 0;
    strncat(filepath_p, MCT_EXT_CTID_LOG_DIRECTORY,
            sizeof(MCT_EXT_CTID_LOG_DIRECTORY) - 1);
    strncat(filepath_p, filename_p, strlen(filename_p));
    strncat(filepath_p, extention_p, strlen(extention_p));
    return;
}

int mct_ext_check_log_dir(void)
{
    int ret = MCT_RETURN_OK;
    struct stat sb;

    if (stat(MCT_EXT_CTID_LOG_DIRECTORY, &sb) < 0) {
        if (stat(MCT_EXT_CTID_MCT_DIRECTORY, &sb) < 0) {
            if (mkdir(MCT_EXT_CTID_MCT_DIRECTORY, MCT_EXT_CTID_MKDIR_MODE) < 0) {
                mct_log(LOG_ERR, "mkdir fail..\n");
                ret = MCT_RETURN_ERROR;
            } else {
                if (mkdir(MCT_EXT_CTID_LOG_DIRECTORY, MCT_EXT_CTID_MKDIR_MODE)
                    < 0) {
                    mct_log(LOG_ERR, "mkdir fail..\n");
                    ret = MCT_RETURN_ERROR;
                }
            }
        } else {
            if (mkdir(MCT_EXT_CTID_LOG_DIRECTORY, MCT_EXT_CTID_MKDIR_MODE) < 0) {
                mct_log(LOG_ERR, "mkdir fail..\n");
                ret = MCT_RETURN_ERROR;
            }
        }
    }

    return ret;
}

int mct_ext_make_ringbuf(MctExtBuff *pMctExtBuff)
{
    int ret;
    char log_filepath[MCT_EXT_CTID_LOG_FILEPATH_LEN + 1];
    int fd;
    int ret_val;
    const char c = '\0';
    ssize_t write_ret;
    void *addr = NULL;

    ret = mct_ext_check_log_dir();

    if (ret != MCT_RETURN_OK) {
        return ret;
    }

    mct_ext_get_log_filepath(log_filepath, pMctExtBuff->log_filename,
                             MCT_EXT_CTID_LOG_FILE_EXTENSION);

    /* if there are log files of same apid and same ctid, create backup log file */
    mct_ext_backup_ringbuf(pMctExtBuff);

    ret = MCT_RETURN_ERROR;
    fd = open(log_filepath, O_CREAT | O_RDWR, S_IRWXU | S_IRWXO);

    if (fd != -1) {
        ret_val = lseek(fd, (pMctExtBuff->size - 1), SEEK_SET);

        if (ret_val != -1) {
            write_ret = write(fd, &c, 1);

            if (write_ret != -1) {
                addr = mmap(NULL, pMctExtBuff->size, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, 0);

                if (addr != MAP_FAILED) {
                    ret = MCT_RETURN_OK;
                    pMctExtBuff->addr = addr;
                }
            }
        }

        close(fd);
    }

    if (ret != MCT_RETURN_OK) {
        mct_log(LOG_ERR, "log file operation failed..\n");
    }

    return ret;
}

void mct_ext_decide_log_filename(MctExtBuff *pMctExtBuff)
{
    char apid_str[MCT_ID_SIZE + 1];
    char pid_str[MCT_EXT_PID_MAX_LEN + 1];

    /* get string of user apid and pid */
    memset(apid_str, 0, sizeof(apid_str));
    mct_set_id(apid_str, mct_user.appID);
    sprintf(pid_str, "%d", getpid());

    /* log file name decide */
    pMctExtBuff->log_filename[0] = 0;
    strncat(pMctExtBuff->log_filename, MCT_EXT_CTID_LOG_FILE_PREFIX,
            sizeof(MCT_EXT_CTID_LOG_FILE_PREFIX) - 1); /* mct    */
    strncat(pMctExtBuff->log_filename, MCT_EXT_CTID_LOG_FILE_DELIMITER,
            sizeof(MCT_EXT_CTID_LOG_FILE_DELIMITER) - 1); /* _      */
    strncat(pMctExtBuff->log_filename, pMctExtBuff->ctid,
            strlen(pMctExtBuff->ctid)); /* <ctid> */
    strncat(pMctExtBuff->log_filename, MCT_EXT_CTID_LOG_FILE_DELIMITER,
            sizeof(MCT_EXT_CTID_LOG_FILE_DELIMITER) - 1);           /* _      */
    strncat(pMctExtBuff->log_filename, apid_str, strlen(apid_str)); /* <apid> */
    strncat(pMctExtBuff->log_filename, MCT_EXT_CTID_LOG_FILE_DELIMITER,
            sizeof(MCT_EXT_CTID_LOG_FILE_DELIMITER) - 1);         /* _      */
    strncat(pMctExtBuff->log_filename, pid_str, strlen(pid_str)); /* <pid>  */
    return;
}

int mct_ext_ringbuf_init(MctContext *handle, const char *contextid,
                         MctTraceBufType trace_buf_type)
{
    int ret = MCT_RETURN_ERROR;
    MctExtBuff *pMctExtBuff;

    /* get ringbuffer info table and link to injection_table */
    pMctExtBuff = malloc(sizeof(MctExtBuff));

    if (pMctExtBuff == NULL) {
        return ret;
    }

    mct_user.mct_ll_ts[handle->log_level_pos].MctExtBuff_ptr = pMctExtBuff;

    /* Store context id and filename */
    pMctExtBuff->ctid[4] = 0;
    mct_set_id(pMctExtBuff->ctid, contextid);
    mct_ext_decide_log_filename(pMctExtBuff);

    /* ring buffer size decide */
    if (trace_buf_type == MCT_TRACE_BUF_SMALL) {
        pMctExtBuff->size = MCT_EXT_BUF_SIZE_SMALL * mct_nw_trace_hp_cfg;
    } else {
        pMctExtBuff->size = MCT_EXT_BUF_SIZE_LARGE * mct_nw_trace_hp_cfg;
    }

    /* create empty file and mmap */
    ret = mct_ext_make_ringbuf(pMctExtBuff);

    if (ret != MCT_RETURN_OK) {
        mct_user.mct_ll_ts[handle->log_level_pos].MctExtBuff_ptr =
            (MctExtBuff *)NULL;
        free(pMctExtBuff);
        return ret;
    }

    /* set ringbuf header */
    mct_ext_set_ringbuf_header(pMctExtBuff);

    return ret;
}

int mct_register_context_hp(MctContext *handle,
                            const char *contextid,
                            const char *description,
                            MctTraceBufType trace_buf_type)
{
    char *env_p;
    int ret = MCT_RETURN_ERROR;

    /* get configuration at once */
    MCT_SEM_LOCK();

    if (mct_nw_trace_hp_cfg == -1) {
        env_p = getenv(MCT_NW_TRACE_HP_CFG_ENV);

        if (env_p != NULL) {
            mct_nw_trace_hp_cfg = atoi(env_p);

            if ((mct_nw_trace_hp_cfg > MCT_EXT_BUF_MAX_NUM)
                || (mct_nw_trace_hp_cfg < MCT_EXT_BUF_DEFAULT_NUM)) {
                mct_log(LOG_DEBUG, "configuration invalid value, set default.\n");
                mct_nw_trace_hp_cfg = MCT_EXT_BUF_DEFAULT_NUM;
            }
        } else {
            mct_nw_trace_hp_cfg = MCT_EXT_BUF_DEFAULT_NUM;
        }
    }

    MCT_SEM_FREE();

    if (mct_nw_trace_hp_cfg != 0) {
        MCT_SEM_LOCK();

        /* check hp ctid limit */
        if (mct_hp_ctid_cnt >= MCT_EXT_CTID_MAX) {
            MCT_SEM_FREE();
            mct_log(LOG_ERR, "no more entry new hp ctid -> return.\n");
            return ret;
        }

        mct_hp_ctid_cnt++;
        MCT_SEM_FREE();

        /* check first API call */
        if (mct_hp_ctid_cnt == 1) {
            mct_log(LOG_DEBUG, "first API call\n");
            /* at first API call, regist INJECTION callback */
            mct_register_context(&context_injection_callback,
                                 MCT_CT_EXT_CB,
                                 "MCT Extention API Injection Callback");
            mct_register_injection_callback(&context_injection_callback,
                                            MCT_EX_INJECTION_CODE,
                                            mct_ext_injection_macro_callback);
        }

        /* register context */
        ret = mct_register_context(handle, contextid, description);

        if (ret == MCT_RETURN_OK) {
            /* make ringbuffer */
            ret = mct_ext_ringbuf_init(handle, contextid, trace_buf_type);

            if (ret != MCT_RETURN_OK) {
                /* count down hp context */
                mct_hp_ctid_cnt--;
            }
        }
    } else {
        /* register context */
        ret = mct_register_context(handle, contextid, description);
    }

    return ret;
}

/* Callback function */
void mct_ext_fix_hp_log(MctExtBuff *pMctExtBuff, uint8_t count)
{
    int ret = MCT_RETURN_ERROR;
    char log_filepath[MCT_EXT_CTID_LOG_FILEPATH_LEN + 1];
    char fix_filepath[MCT_EXT_CTID_FIX_FILEPATH_LEN + 1];

    MCT_SEM_LOCK();
    /* munmap ringbuffer */
    munmap(pMctExtBuff->addr, pMctExtBuff->size);

    /* change extension */
    mct_ext_get_log_filepath(log_filepath, pMctExtBuff->log_filename,
                             MCT_EXT_CTID_LOG_FILE_EXTENSION);
    mct_ext_get_log_filepath(fix_filepath, pMctExtBuff->log_filename,
                             MCT_EXT_CTID_FIX_FILE_EXTENSION);
    rename(log_filepath, fix_filepath);

    /* create next empty file and mmap */
    ret = mct_ext_make_ringbuf(pMctExtBuff);

    if (ret != MCT_RETURN_OK) {
        mct_user.mct_ll_ts[count].MctExtBuff_ptr = (MctExtBuff *)NULL;
        free(pMctExtBuff);
        MCT_SEM_FREE();
        return;
    }

    /* set ringbuf header */
    mct_ext_set_ringbuf_header(pMctExtBuff);
    MCT_SEM_FREE();
    return;
}

int mct_ext_injection_macro_callback(uint32_t service_id, void *data,
                                     uint32_t length)
{
    int ret;
    char *ctid_p;
    uint8_t count;
    uint32_t remain;
    char target_ctid[MCT_ID_SIZE + 1];
    uint8_t read_count;
    MctExtBuff *pMctExtBuff;

    /* check service_id */
    if (service_id != MCT_EX_INJECTION_CODE) {
        mct_log(LOG_ERR, "service_id is invalid -> return.\n");
        return MCT_RETURN_ERROR;
    }

    /* check callback parameters */
    if ((data == NULL) || (length == 0)) {
        mct_log(LOG_ERR, "parameter fail -> return.\n");
        return MCT_RETURN_ERROR;
    }

    /* target ctid check */
    ctid_p = (char *)data;
    remain = length;

    if (ctid_p[0] == MCT_EXT_CTID_WILDCARD) {
        /* wildcard */
        mct_log(LOG_DEBUG, "all ctid stop\n");

        for (count = 0; count < mct_user.mct_ll_ts_num_entries; count++) {
            if ((mct_user.mct_ll_ts[count].nrcallbacks == 0)
                && (mct_user.mct_ll_ts[count].MctExtBuff_ptr != NULL)) {
                pMctExtBuff = mct_user.mct_ll_ts[count].MctExtBuff_ptr;
                mct_ext_fix_hp_log(pMctExtBuff, count);
            }
        }
    } else {
        do {
            if (ctid_p[0] == '\0') {
                remain--;
                ctid_p++;
            } else {
                read_count = 0;

                do {
                    remain--;
                    read_count++;

                    if (ctid_p[read_count] == '\0') {
                        break;
                    }
                } while (remain);

                if (read_count <= MCT_ID_SIZE) {
                    memset(target_ctid, 0, (MCT_ID_SIZE + 1));
                    memcpy(target_ctid, ctid_p, read_count);

                    /* check target ctid running */
                    for (count = 0; count < mct_user.mct_ll_ts_num_entries;
                         count++) {
                        if ((mct_user.mct_ll_ts[count].nrcallbacks == 0)
                            && (mct_user.mct_ll_ts[count].MctExtBuff_ptr
                                != NULL)) {
                            pMctExtBuff =
                                mct_user.mct_ll_ts[count].MctExtBuff_ptr;
                            ret = strncmp(target_ctid, pMctExtBuff->ctid,
                                          MCT_ID_SIZE);

                            if (ret == 0) {
                                mct_log(LOG_DEBUG, "target ctid stop\n");
                                mct_ext_fix_hp_log(pMctExtBuff, count);
                            }
                        }
                    }
                }

                ctid_p += read_count;
            }
        } while (remain);
    }

    return MCT_RETURN_OK;
}


/* Trace functions */
int mct_ext_parameter_check(MctContext *handle,
                            MctNetworkTraceType nw_trace_type, uint16_t header_len, void *header,
                            uint16_t payload_len, void *payload)
{
    if (handle == NULL) {
        mct_log(LOG_ERR, "handle error\n");
        return MCT_RETURN_ERROR;
    }

    if ((nw_trace_type != MCT_NW_TRACE_HP0)
        && (nw_trace_type != MCT_NW_TRACE_HP1)) {
        mct_log(LOG_ERR, "nw_trace_type error\n");
        return MCT_RETURN_ERROR;
    }

    if (header_len != 0) {
        if (header == NULL) {
            mct_log(LOG_ERR, "header error\n");
            return MCT_RETURN_ERROR;
        }
    }

    if (payload_len != 0) {
        if (payload == NULL) {
            mct_log(LOG_ERR, "payload error\n");
            return MCT_RETURN_ERROR;
        }
    }

    return MCT_RETURN_OK;
}

void mct_ext_write_ring_buff_set_data(void *RingBuffHead_p,
                                      uint32_t RingBuffSize, uint32_t *write_count, void *data_p,
                                      uint32_t data_len)
{
    void *wp;
    uint32_t remain_buff_size;
    void *divide_data_p;
    uint32_t divide_data_len;

    wp = RingBuffHead_p + *write_count;
    remain_buff_size = RingBuffSize - *write_count;

    if (data_len > remain_buff_size) {
        mct_log(LOG_DEBUG, "step over end of buf!\n");

        /* set the first half of data to bottom */
        memcpy(wp, data_p, remain_buff_size);
        wp = RingBuffHead_p;
        divide_data_p = data_p + remain_buff_size;
        divide_data_len = data_len - remain_buff_size;

        /* set the remain data to top */
        memcpy(wp, divide_data_p, divide_data_len);
        *write_count = divide_data_len;
    } else {
        /* set normally */
        memcpy(wp, data_p, data_len);
        *write_count += data_len;
    }

    return;
}

void mct_ext_write_ring_buff(MctExtBuff *pMctExtBuff,
                             MctNetworkTraceType nw_trace_type,
                             uint16_t header_len,
                             void *header,
                             uint16_t payload_len,
                             void *payload)
{
    MctExtBuffHeader *pBufHeader;
    void *RingBuffHead_p;
    uint32_t RingBuffSize;
    char MctStorageheader[4] = {0x44, 0x4C, 0x54, 0x01};
    struct timeval tv;
    struct timezone tz;
    uint16_t ringbuf_log_len;
    uint32_t seid;
    uint32_t tsmp;
    uint16_t maxlen;
    uint16_t write_header_len;
    uint16_t write_payload_len;

    pBufHeader = (MctExtBuffHeader *)pMctExtBuff->addr;
    RingBuffHead_p = ((void *)pBufHeader + sizeof(MctExtBuffHeader));
    RingBuffSize = pMctExtBuff->size - sizeof(MctExtBuffHeader);

    /* get pid */
    seid = getpid();
    seid = MCT_HTOBE_32(seid);

    /* get current time */
    gettimeofday(&tv, &tz);
    tsmp = mct_uptime();
    tsmp = MCT_HTOBE_32(tsmp);

    /* check nw_trace_type */
    if (nw_trace_type == MCT_NW_TRACE_HP0) {
        maxlen = MCT_EXT_BUF_LOGMAX_HP0;
    } else {
        maxlen = MCT_EXT_BUF_LOGMAX_HP1;
    }

    /* calculate length */
    if (header_len > maxlen) {
        write_header_len = maxlen;
        write_payload_len = 0;
    } else if ((header_len + payload_len) > maxlen) {
        write_header_len = header_len;
        write_payload_len = maxlen - header_len;
    } else {
        write_header_len = header_len;
        write_payload_len = payload_len;
    }

    ringbuf_log_len = (sizeof(MctStandardHeader)
                       + MCT_STANDARD_HEADER_EXTRA_SIZE(pBufHeader->htyp)
                       + sizeof(MctExtendedHeader) + sizeof(uint32_t) + sizeof(header_len)
                       + write_header_len + sizeof(uint32_t) + sizeof(payload_len)
                       + write_payload_len);
    ringbuf_log_len = (((((ringbuf_log_len) >> 8) & 0xff)
                        | (((ringbuf_log_len) << 8) & 0xff00)));

    /* Set MctStorageHeader */
    mct_ext_write_ring_buff_set_data(RingBuffHead_p, RingBuffSize,
                                     &pBufHeader->write_count, MctStorageheader,
                                     sizeof(MctStorageheader));
    mct_ext_write_ring_buff_set_data(RingBuffHead_p, RingBuffSize,
                                     &pBufHeader->write_count, &tv.tv_sec, sizeof(int32_t));
    mct_ext_write_ring_buff_set_data(RingBuffHead_p, RingBuffSize,
                                     &pBufHeader->write_count, &tv.tv_usec, sizeof(int32_t));

    /* Set MctStandardHeader */
    mct_ext_write_ring_buff_set_data(RingBuffHead_p, RingBuffSize,
                                     &pBufHeader->write_count, &ringbuf_log_len,
                                     sizeof(ringbuf_log_len));

    /* Set MctStandardHeaderExtra */
    if (MCT_IS_HTYP_WSID(pBufHeader->htyp)) {
        mct_ext_write_ring_buff_set_data(RingBuffHead_p, RingBuffSize,
                                         &pBufHeader->write_count, &seid, sizeof(seid));
    }

    if (MCT_IS_HTYP_WTMS(pBufHeader->htyp)) {
        mct_ext_write_ring_buff_set_data(RingBuffHead_p, RingBuffSize,
                                         &pBufHeader->write_count, &tsmp, sizeof(tsmp));
    }

    /* Set User LogData */
    mct_ext_write_ring_buff_set_data(RingBuffHead_p, RingBuffSize,
                                     &pBufHeader->write_count, &write_header_len,
                                     sizeof(write_header_len));
    mct_ext_write_ring_buff_set_data(RingBuffHead_p, RingBuffSize,
                                     &pBufHeader->write_count, header, write_header_len);
    mct_ext_write_ring_buff_set_data(RingBuffHead_p, RingBuffSize,
                                     &pBufHeader->write_count, &write_payload_len,
                                     sizeof(write_payload_len));
    mct_ext_write_ring_buff_set_data(RingBuffHead_p, RingBuffSize,
                                     &pBufHeader->write_count, payload, write_payload_len);

    return;
}

void mct_user_trace_network_hp(MctContext *handle,
                               MctNetworkTraceType nw_trace_type, uint16_t header_len, void *header,
                               uint16_t payload_len, void *payload)
{
    MctExtBuff *pMctExtBuff;
    int ret = 0;

    /* MCT_TRACE_NETWORK() */
    if (header == 0) {
        ret = mct_user_trace_network(handle, nw_trace_type, payload_len,
                                     payload, 0, 0);
    } else {
        nw_trace_type |= MCT_NW_TRACE_ASCII_OUT;
        ret = mct_user_trace_network(handle, nw_trace_type, header_len, header,
                                     0, 0);
        nw_trace_type &= ~MCT_NW_TRACE_ASCII_OUT;
        ret = mct_user_trace_network(handle, nw_trace_type, payload_len,
                                     payload, 0, 0);
    }

    if (ret != 0) {
        return;
    }

    pMctExtBuff = mct_user.mct_ll_ts[handle->log_level_pos].MctExtBuff_ptr;

    if (pMctExtBuff != NULL) {
        /* check paramters */
        if (mct_ext_parameter_check(handle, nw_trace_type, header_len, header,
                                    payload_len, payload) != MCT_RETURN_OK) {
            return;
        }

        /* ring buffer write */
        MCT_SEM_LOCK();
        mct_ext_write_ring_buff(pMctExtBuff, nw_trace_type, header_len, header,
                                payload_len, payload);
        MCT_SEM_FREE();
    }

    return;
}
