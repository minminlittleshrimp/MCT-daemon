#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <sys/uio.h> /* writev() */

#include "mct_common.h"

#define COMMAND_SIZE        1024    /* Size of command */
#define FILENAME_SIZE       1024    /* Size of filename */
#define MCT_EXTENSION       "mct"
#define MCT_CONVERT_WS      "/tmp/mct_convert_workspace/"

/**
 * Print usage information of tool.
 */
void usage()
{
    char version[MCT_CONVERT_TEXTBUFSIZE];

    mct_get_version(version, 255);

    printf("Usage: mct-convert [options] [commands] file1 [file2]\n");
    printf("Read MCT files, print MCT messages as ASCII and store the messages again.\n");
    printf("Use filters to filter MCT messages.\n");
    printf("Use Ranges and Output file to cut MCT files.\n");
    printf("Use two files and Output file to join MCT files.\n");
    printf("%s \n", version);
    printf("Commands:\n");
    printf("  -h            Usage\n");
    printf("  -a            Print MCT file; payload as ASCII\n");
    printf("  -x            Print MCT file; payload as hex\n");
    printf("  -m            Print MCT file; payload as hex and ASCII\n");
    printf("  -s            Print MCT file; only headers\n");
    printf("  -o filename   Output messages in new MCT file\n");
    printf("Options:\n");
    printf("  -v            Verbose mode\n");
    printf("  -c            Count number of messages\n");
    printf("  -f filename   Enable filtering of messages\n");
    printf("  -b number     First messages to be handled\n");
    printf("  -e number     Last message to be handled\n");
    printf("  -w            Follow mct file while file is increasing\n");
    printf("  -t            Handling input compressed files (tar.gz)\n");
}

char *get_filename_ext(const char *filename)
{
    if (filename == NULL)
            fprintf(stderr, "ERROR: %s: invalid arguments\n", __FUNCTION__);

    char *dot = strrchr(filename, '.');
    if(!dot || dot == filename)
        return "";
    return dot + 1;
}

void empty_dir(const char *dir)
{
    struct dirent **files = { 0 };
    struct stat st;
    uint32_t n = 0;
    char tmp_filename[FILENAME_SIZE] = { 0 };
    uint32_t i;

    if (dir == NULL)
        fprintf(stderr, "ERROR: %s: invalid arguments\n", __FUNCTION__);

    if (stat(dir, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            n = (uint32_t) scandir(dir, &files, NULL, alphasort);

            /* Do not include /. and /.. */
            if (n < 2)
                fprintf(stderr, "ERROR: Failed to scan %s with error %s\n",
                        dir, strerror(errno));
            else if (n == 2)
                printf("%s is already empty\n", dir);
            else {
                for (i = 2; i < n; i++) {
                    memset(tmp_filename, 0, FILENAME_SIZE);
                    snprintf(tmp_filename, FILENAME_SIZE, "%s%s", dir, files[i]->d_name);

                    if (remove(tmp_filename) != 0)
                        fprintf(stderr, "ERROR: Failed to delete %s with error %s\n",
                                tmp_filename, strerror(errno));
                }
                if (files) {
                    for (i = 0; i < n ; i++)
                        if (files[i]) {
                            free(files[i]);
                            files[i] = NULL;
                        }
                    free(files);
                    files = NULL;
                }
            }
        }
        else
            fprintf(stderr, "ERROR: %s is not a directory\n", dir);
    }
    else
        fprintf(stderr, "ERROR: Failed to stat %s with error %s\n", dir, strerror(errno));
}

/**
 * Main function of tool.
 */
int main(int argc, char *argv[])
{
    int vflag = 0;
    int cflag = 0;
    int aflag = 0;
    int sflag = 0;
    int xflag = 0;
    int mflag = 0;
    int wflag = 0;
    int tflag = 0;
    char *fvalue = 0;
    char *bvalue = 0;
    char *evalue = 0;
    char *ovalue = 0;

    int index;
    int c;

    MctFile file;
    MctFilter filter;

    int ohandle = -1;

    int num, begin, end;

    char text[MCT_CONVERT_TEXTBUFSIZE] = { 0 };

    /* For handling compressed files */
    char tmp_filename[FILENAME_SIZE] = { 0 };
    struct stat st;
    memset(&st, 0, sizeof(struct stat));
    struct dirent **files = { 0 };
    int n = 0;
    int i = 0;

    struct iovec iov[2];
    int bytes_written = 0;
    int syserr = 0;

    opterr = 0;

    while ((c = getopt (argc, argv, "vcashxmwtf:b:e:o:")) != -1) {
        switch (c)
        {
        case 'v':
        {
            vflag = 1;
            break;
        }
        case 'c':
        {
            cflag = 1;
            break;
        }
        case 'a':
        {
            aflag = 1;
            break;
        }
        case 's':
        {
            sflag = 1;
            break;
        }
        case 'x':
        {
            xflag = 1;
            break;
        }
        case 'm':
        {
            mflag = 1;
            break;
        }
        case 'w':
        {
            wflag = 1;
            break;
        }
        case 't':
        {
            tflag = 1;
            break;
        }
        case 'h':
        {
            usage();
            return -1;
        }
        case 'f':
        {
            fvalue = optarg;
            break;
        }
        case 'b':
        {
            bvalue = optarg;
            break;
        }
        case 'e':
        {
            evalue = optarg;
            break;
        }
        case 'o':
        {
            ovalue = optarg;
            break;
        }
        case '?':
        {
            if ((optopt == 'f') || (optopt == 'b') || (optopt == 'e') || (optopt == 'o'))
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);

            /* unknown or wrong option used, show usage information and terminate */
            usage();
            return -1;
        }
        default:
        {
            return -1;    /*for parasoft */
        }
        }
    }

    /* Initialize structure to use MCT file */
    mct_file_init(&file, vflag);

    /* first parse filter file if filter parameter is used */
    if (fvalue) {
        if (mct_filter_load(&filter, fvalue, vflag) < MCT_RETURN_OK) {
            mct_file_free(&file, vflag);
            return -1;
        }

        mct_file_set_filter(&file, &filter, vflag);
    }

    if (ovalue) {
        ohandle = open(ovalue, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); /* mode: wb */

        if (ohandle == -1) {
            mct_file_free(&file, vflag);
            fprintf(stderr, "ERROR: Output file %s cannot be opened!\n", ovalue);
            return -1;
        }
    }

    if (tflag) {
        /* Prepare the temp dir to untar compressed files */
        if (stat(MCT_CONVERT_WS, &st) == -1) {
            if (mkdir(MCT_CONVERT_WS, 0700) != 0) {
                fprintf(stderr,"ERROR: Cannot create temp dir %s!\n", MCT_CONVERT_WS);
                if (ovalue)
                    close(ohandle);

                return -1;
            }
        }
        else {
            if (S_ISDIR(st.st_mode))
                empty_dir(MCT_CONVERT_WS);
            else
                fprintf(stderr, "ERROR: %s is not a directory", MCT_CONVERT_WS);
        }

        for (index = optind; index < argc; index++) {
            /* Check extension of input file
             * If it is a compressed file, uncompress it
             */
            if (strcmp(get_filename_ext(argv[index]), MCT_EXTENSION) != 0) {
                syserr = mct_execute_command(NULL, "tar", "xf", argv[index], "-C", MCT_CONVERT_WS, NULL);
                if (syserr != 0)
                    fprintf(stderr, "ERROR: Failed to uncompress %s to %s with error [%d]\n",
                            argv[index], MCT_CONVERT_WS, WIFEXITED(syserr));
            }
            else {
                syserr = mct_execute_command(NULL, "cp", argv[index], MCT_CONVERT_WS, NULL);
                if (syserr != 0)
                    fprintf(stderr, "ERROR: Failed to copy %s to %s with error [%d]\n",
                            argv[index], MCT_CONVERT_WS, WIFEXITED(syserr));
            }

        }

        n = scandir(MCT_CONVERT_WS, &files, NULL, alphasort);
        if (n == -1) {
            fprintf(stderr,"ERROR: Cannot scan temp dir %s!\n", MCT_CONVERT_WS);
            if (ovalue)
                close(ohandle);

            return -1;
        }

        /* do not include ./ and ../ in the files */
        argc = optind + (n - 2);
    }

    for (index = optind; index < argc; index++) {
        if (tflag) {
            memset(tmp_filename, 0, FILENAME_SIZE);
            snprintf(tmp_filename, FILENAME_SIZE, "%s%s",
                    MCT_CONVERT_WS, files[index - optind + 2]->d_name);

            argv[index] = tmp_filename;
        }

        /* load, analyze data file and create index list */
        if (mct_file_open(&file, argv[index], vflag) >= MCT_RETURN_OK) {
            while (mct_file_read(&file, vflag) >= MCT_RETURN_OK) {
            }
        }

        if (aflag || sflag || xflag || mflag || ovalue) {
            if (bvalue)
                begin = atoi(bvalue);
            else
                begin = 0;

            if (evalue && (wflag == 0))
                end = atoi(evalue);
            else
                end = file.counter - 1;

            if ((begin < 0) || (begin >= file.counter)) {
                fprintf(stderr, "ERROR: Selected first message %d is out of range!\n", begin);
                if (ovalue)
                    close(ohandle);

                return -1;
            }

            if ((end < 0) || (end >= file.counter) || (end < begin)) {
                fprintf(stderr, "ERROR: Selected end message %d is out of range!\n", end);
                if (ovalue)
                    close(ohandle);

                return -1;
            }

            for (num = begin; num <= end; num++) {
                if (mct_file_message(&file, num, vflag) < MCT_RETURN_OK)
                    continue;

                if (xflag) {
                    printf("%d ", num);
                    if (mct_message_print_hex(&(file.msg), text, MCT_CONVERT_TEXTBUFSIZE, vflag) < MCT_RETURN_OK)
                        continue;
                }
                else if (aflag) {
                    printf("%d ", num);

                    if (mct_message_header(&(file.msg), text, MCT_CONVERT_TEXTBUFSIZE, vflag) < MCT_RETURN_OK)
                        continue;

                    printf("%s ", text);

                    if (mct_message_payload(&file.msg, text, MCT_CONVERT_TEXTBUFSIZE, MCT_OUTPUT_ASCII, vflag) < MCT_RETURN_OK)
                        continue;

                    printf("[%s]\n", text);
                }
                else if (mflag) {
                    printf("%d ", num);
                    if (mct_message_print_mixed_plain(&(file.msg), text, MCT_CONVERT_TEXTBUFSIZE, vflag) < MCT_RETURN_OK)
                        continue;
                }
                else if (sflag) {
                    printf("%d ", num);

                    if (mct_message_header(&(file.msg), text, MCT_CONVERT_TEXTBUFSIZE, vflag) < MCT_RETURN_OK)
                        continue;

                    printf("%s \n", text);
                }

                /* if file output enabled write message */
                if (ovalue) {
                    iov[0].iov_base = file.msg.headerbuffer;
                    iov[0].iov_len = (uint32_t) file.msg.headersize;
                    iov[1].iov_base = file.msg.databuffer;
                    iov[1].iov_len = (uint32_t) file.msg.datasize;

                    bytes_written =(int) writev(ohandle, iov, 2);

                    if (0 > bytes_written) {
                        printf("in main: writev(ohandle, iov, 2); returned an error!");
                        close(ohandle);
                        mct_file_free(&file, vflag);
                        return -1;
                    }
                }

                /* check for new messages if follow flag set */
                if (wflag && (num == end)) {
                    while (1) {
                        while (mct_file_read(&file, 0) >= 0){
                        }

                        if (end == (file.counter - 1)) {
                            /* Sleep if no new message was received */
                            struct timespec req;
                            req.tv_sec = 0;
                            req.tv_nsec = 100000000;
                            nanosleep(&req, NULL);
                        }
                        else {
                            /* set new end of log file and continue reading */
                            end = file.counter - 1;
                            break;
                        }
                    }
                }
            }
        }

        if (cflag) {
            printf("Total number of messages: %d\n", file.counter_total);

            if (file.filter)
                printf("Filtered number of messages: %d\n", file.counter);
        }
    }

    if (ovalue)
        close(ohandle);

    if (tflag) {
        empty_dir(MCT_CONVERT_WS);
        if (files) {
            for (i = 0; i < n ; i++)
                if (files[i])
                    free(files[i]);

            free(files);
        }
        rmdir(MCT_CONVERT_WS);
    }
    if (index == optind) {
        /* no file selected, show usage and terminate */
        fprintf(stderr, "ERROR: No file selected\n");
        usage();
        return -1;
    }

    mct_file_free(&file, vflag);

    return 0;
}
