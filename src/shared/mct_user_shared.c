#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/uio.h> /* writev() */

#include "mct_user_shared.h"
#include "mct_user_shared_cfg.h"

DltReturnValue mct_user_set_userheader(DltUserHeader *userheader, uint32_t mtype)
{
    if (userheader == 0)
        return DLT_RETURN_ERROR;

    if (mtype <= 0)
        return DLT_RETURN_ERROR;

    userheader->pattern[0] = 'D';
    userheader->pattern[1] = 'U';
    userheader->pattern[2] = 'H';
    userheader->pattern[3] = 1;
    userheader->message = mtype;

    return DLT_RETURN_OK;
}

int mct_user_check_userheader(DltUserHeader *userheader)
{
    if (userheader == 0)
        return -1;

    return (userheader->pattern[0] == 'D') &&
           (userheader->pattern[1] == 'U') &&
           (userheader->pattern[2] == 'H') &&
           (userheader->pattern[3] == 1);
}

DltReturnValue mct_user_log_out2(int handle, void *ptr1, size_t len1, void *ptr2, size_t len2)
{
    struct iovec iov[2];
    uint32_t bytes_written;

    if (handle <= 0)
        /* Invalid handle */
        return DLT_RETURN_ERROR;

    iov[0].iov_base = ptr1;
    iov[0].iov_len = len1;
    iov[1].iov_base = ptr2;
    iov[1].iov_len = len2;

    bytes_written = (uint32_t) writev(handle, iov, 2);

    if (bytes_written != (len1 + len2))
        return DLT_RETURN_ERROR;

    return DLT_RETURN_OK;
}

DltReturnValue mct_user_log_out3(int handle, void *ptr1, size_t len1, void *ptr2, size_t len2, void *ptr3, size_t len3)
{
    struct iovec iov[3];
    uint32_t bytes_written;

    if (handle <= 0)
        /* Invalid handle */
        return DLT_RETURN_ERROR;

    iov[0].iov_base = ptr1;
    iov[0].iov_len = len1;
    iov[1].iov_base = ptr2;
    iov[1].iov_len = len2;
    iov[2].iov_base = ptr3;
    iov[2].iov_len = len3;

    bytes_written = (uint32_t) writev(handle, iov, 3);

    if (bytes_written != (len1 + len2 + len3)) {
        switch (errno) {
        case EBADF:
        {
            return DLT_RETURN_PIPE_ERROR;     /* EBADF - handle not open */
            break;
        }
        case EPIPE:
        {
            return DLT_RETURN_PIPE_ERROR;     /* EPIPE - pipe error */
            break;
        }
        case EAGAIN:
        {
            return DLT_RETURN_PIPE_FULL;     /* EAGAIN - data could not be written */
            break;
        }
        default:
        {
            break;
        }
        }

        return DLT_RETURN_ERROR;
    }

    return DLT_RETURN_OK;
}