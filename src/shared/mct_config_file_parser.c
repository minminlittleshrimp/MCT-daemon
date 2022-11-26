#include "mct_config_file_parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
#include "mct_common.h"
#include "mct-daemon_cfg.h"

/* internal defines */
#define MCT_CONFIG_FILE_NEW_SECTION 0x0a
#define MCT_CONFIG_FILE_NEW_DATA    0x0b


/* internal helper functions */

/**
 * mct_config_file_trim_line
 *
 * Trim all whitespace from a string
 *
 * @param line  String to remove whitespace from
 */
static void mct_config_file_trim_line(char *line)
{
    if (line == NULL)
        return;

    char *i = line;
    char *j = line;

    while (*j != '\0') {
        *i = *j++;

        if (!isspace(*i))
            i++;
    }

    *i = '\0';
}

/**
 * mct_config_file_ignore_line
 *
 * Check if a line has to be ignored, because it contains a comment or is empty
 *
 * @param line  Line of configuration file
 * @return 0 if ignore, -1 do not ignore
 */
static int mct_config_file_ignore_line(char *line)
{
    if ((line[0] == '#') || (line[0] == ';') || (line[0] == '\n') ||
        (line[0] == '\0'))
        return 0; /* ignore */
    else
        return -1; /* do not ignore */
}

/**
 * mct_config_file_is_section_name
 *
 * Check if section name already used
 *
 * @param file  MctConfigFile
 * @param name  Name of section
 * @return 0, section name not used, -1 section name already used
 */
static int mct_config_file_is_section_name(MctConfigFile *file, char *name)
{
    int i = 0;

    if ((file == NULL) || (name == NULL))
        return -1;

    for (i = 0; i < file->num_sections; i++) {
        MctConfigFileSection *s = &file->sections[i];

        if (strncmp(s->name, name, MCT_CONFIG_FILE_ENTRY_MAX_LEN) == 0)
            return -1;
    }

    return 0; /* section name not used */
}

/**
 * mct_config_file_set_section
 *
 * Store section in internal data structure
 *
 * @param file  MctConfigFile
 * @param name  Name of section
 * @return 0 on success, else -1
 */
static int mct_config_file_set_section(MctConfigFile *file, char *name)
{
    int section = file->num_sections;

    /* check if adding another section would exceed max number of sections */
    if (section >= MCT_CONFIG_FILE_SECTIONS_MAX) {
        mct_log(LOG_WARNING, "Cannot store more sections\n");
        return -1; /* reached max number of sections */
    }

    /* do not store section with same name again */
    if (mct_config_file_is_section_name(file, name) != 0) {
        mct_log(LOG_WARNING, "Cannot store section name again\n");
        return -1;
    }

    MctConfigFileSection *s = &file->sections[section];

    /* alloc data for entries */
    s->name = calloc(sizeof(char), MCT_CONFIG_FILE_ENTRY_MAX_LEN + 1);

    if (s->name == NULL) {
        mct_log(LOG_ERR, "Cannot allocate memory for internal data structure\n");
        return -1;
    }

    s->keys = calloc(sizeof(char), MCT_CONFIG_FILE_ENTRY_MAX_LEN * MCT_CONFIG_FILE_KEYS_MAX + 1);

    if (s->keys == NULL) {
        free(s->name);
        s->name = NULL;
        mct_log(LOG_ERR, "Cannot allocate memory for internal data structure\n");
        return -1;
    }

    strncpy(file->sections[section].name, name, MCT_CONFIG_FILE_ENTRY_MAX_LEN);
    file->num_sections += 1;
    return 0;
}

/**
 * mct_config_file_set_section_data
 *
 * Store data pair of a section
 *
 * @param file MctConfigFile
 * @param str1 string used for key
 * @param str2 string used for value
 * @return 0 on success, else -1
 */
static int mct_config_file_set_section_data(MctConfigFile *file, char *str1, char *str2)
{
    MctConfigKeyData **tmp = NULL;

    if ((file == NULL) || (str1 == NULL) || (str2 == NULL))
        return -1;

    MctConfigFileSection *s = &file->sections[file->num_sections - 1];
    int key_number = s->num_entries;

    if (key_number + 1 >= MCT_CONFIG_FILE_KEYS_MAX) {
        mct_log(LOG_WARNING, "Cannot store more keys in section\n");
        return -1; /* reached max number of keys per section */
    }

    /* copy data into structure */
    strncpy(&s->keys[key_number * MCT_CONFIG_FILE_ENTRY_MAX_LEN], str1, MCT_CONFIG_FILE_ENTRY_MAX_LEN);

    if (s->list == NULL) {
        /* creating a list if it doesnt exists */
        s->list = malloc(sizeof(MctConfigKeyData));

        if (s->list == NULL) {
            mct_log(LOG_WARNING, "Could not allocate initial memory to list \n");
            return -1;
        }

        tmp = &s->list;
    }
    else {
        tmp = &s->list;

        while (*(tmp) != NULL)
            tmp = &(*tmp)->next;

        /* Adding new entry to the list */
        *tmp = malloc(sizeof(MctConfigKeyData));

        if (*tmp == NULL) {
            mct_log(LOG_WARNING, "Could not allocate memory to list \n");
            return -1;
        }
    }

    (*tmp)->key = strdup(str1);
    (*tmp)->data = strdup(str2);
    (*tmp)->next = NULL;

    s->num_entries += 1;

    return 0;
}

/**
 * mct_config_file_has_section
 *
 * Check if a certain line in config file is a section header
 *
 * @param line  Line in configuration file
 * @return 0 if section header, else -1
 */
static int mct_config_file_line_has_section(char *line)
{
    (void)line; /* avoid compiler warnings */

    if (line[0] == '[') /* section found */
        return 0;
    else
        return -1;
}

/**
 * mct_config_file_get_section_name_from_string
 *
 * Extract section name from line
 *
 * @param line  Line in configuration file containing a section header
 * @param name  Section name
 * @return 0 on success, else -1
 */
static int mct_config_file_get_section_name_from_string(char *line, char *name)
{
    int i = 0;
    int j = 0;

    if ((line == NULL) || (name == NULL))
        return -1;

    for (i = 0; i < MCT_CONFIG_FILE_ENTRY_MAX_LEN; i++) {
        if ((line[i] == '[') || isspace(line[i]))
            continue;
        else if ((line[i] == ']') || (line[i] == '\n') || (line[i] == '\0'))
            break;
        else
            name[j++] = line[i];
    }

    return 0;
}

/**
 * mct_config_file_get_key_value
 *
 * Get key and value from a line of configuration file
 *
 * @param line      Line on configuration file
 * @param[out] str1 String to be used as key
 * @param[out] str2 String to be used as value
 * @return 0 on success, else -1
 */
static int mct_config_file_get_key_value(char *line, char *str1, char *str2)
{
    char *delimiter = "=";
    char *ptr;
    char *save_ptr;

    if ((line == NULL) || (str1 == NULL) || (str2 == NULL))
        return -1;

    ptr = strtok_r(line, delimiter, &save_ptr);

    if (ptr != NULL) { /* get key */
        strncpy(str1, ptr, MCT_CONFIG_FILE_ENTRY_MAX_LEN - 1);
        str1[MCT_CONFIG_FILE_ENTRY_MAX_LEN - 1] = '\0';
    } else {
        return -1;
    }

    ptr = strtok_r(NULL, delimiter, &save_ptr);

    if (ptr != NULL) {
        strncpy(str2, ptr, MCT_CONFIG_FILE_ENTRY_MAX_LEN - 1);
        str2[MCT_CONFIG_FILE_ENTRY_MAX_LEN - 1] = '\0';
    } else {
        return -1;
    }

    return 0;
}

/**
 * mct_config_file_read_line
 *
 * Read line from configuration file
 *
 * @param       line Line from configuration file
 * @param[out]  str1 String contains section header or key
 * @param[out]  str2 String contains value or is empty
 * @return 0 on success, else -1
 */
static int mct_config_file_read_line(char *line, char *str1, char *str2)
{
    if ((line == NULL) || (str1 == NULL) || (str2 == NULL))
        return -1;

    /* reset values to zero */
    memset(str1, 0, MCT_CONFIG_FILE_ENTRY_MAX_LEN);
    memset(str2, 0, MCT_CONFIG_FILE_ENTRY_MAX_LEN);

    /* check if line contains a section */
    if ((mct_config_file_line_has_section(line)) == 0) {
        /* retrieve section name */
        if (mct_config_file_get_section_name_from_string(line, str1) != 0)
            return -1;

        return MCT_CONFIG_FILE_NEW_SECTION;
    }

    /* copy strings as key value pair into str1, str2 */
    if (mct_config_file_get_key_value(line, str1, str2) != 0)
        return -1;

    return MCT_CONFIG_FILE_NEW_DATA;
}

/**
 * mct_config_file_read_file
 *
 * Read configuration file line by line and fill internal structures
 *
 * @param file MctConfigFile
 * @param hdl  FILE handle of opened configuration file
 */
static void mct_config_file_read_file(MctConfigFile *file, FILE *hdl)
{
    int ret = 0;
    char line[MCT_CONFIG_FILE_LINE_MAX_LEN] = { '\0' };
    char str1[MCT_CONFIG_FILE_ENTRY_MAX_LEN] = { '\0' };
    char str2[MCT_CONFIG_FILE_ENTRY_MAX_LEN] = { '\0' };
    int line_number = 0;
    int is_section_valid = -1; /* to check if section name is given twice or invalid */

    /* read configuration file line by line */
    while (fgets(line, MCT_CONFIG_FILE_LINE_MAX_LEN, hdl) != NULL) {
        line_number++;

        /* ignore empty and comment lines */
        if (mct_config_file_ignore_line(line) == 0)
            continue;

        /* trim line end */
        mct_config_file_trim_line(line);

        /* parse content of line */
        ret = mct_config_file_read_line(line, str1, str2);

        switch (ret) {
        case MCT_CONFIG_FILE_NEW_SECTION:     /* store str1 as new section */
            is_section_valid = -1;

            if ((ret = mct_config_file_set_section(file, str1)) == 0)
                is_section_valid = 0;

            break;
        case MCT_CONFIG_FILE_NEW_DATA:     /* store str1 and str2 as new data for section */

            if (is_section_valid == 0)
                ret = mct_config_file_set_section_data(file, str1, str2);

            break;
        default:     /* something is wrong with the line */
            mct_vlog(LOG_WARNING, "Line (%d) \"%s\" is invalid\n", line_number,
                     line);
        }
    }
}

/**
 * mct_config_file_find_section
 *
 * Find a section
 *
 * @param file      MctConfigFile
 * @param section   Name of section
 * @return number of section on success, else -1
 */
static int mct_config_file_find_section(const MctConfigFile *file,
                                        const char *section)
{
    int i = 0;

    if ((file == NULL) || (section == NULL) || (file->num_sections <= 0)) {
        mct_log(LOG_WARNING, "Section cannot be found due to invalid parameters\n");
        return -1;
    }

    for (i = 0; i < file->num_sections; i++) {
        MctConfigFileSection *s = &file->sections[i];

        if (strncmp(s->name, section, MCT_CONFIG_FILE_ENTRY_MAX_LEN) == 0)
            return i;
    }

    return -1;
}

/************************** interface implementation ***************************/
MctConfigFile *mct_config_file_init(char *file_name)
{
    MctConfigFile *file;
    FILE *hdl = NULL;

    if ((file_name == NULL) || (strlen(file_name) >= MCT_PATH_MAX)) {
        mct_log(LOG_ERR, "Given configuration file invalid\n");
        return NULL;
    }

    file = calloc(sizeof(MctConfigFile), 1);

    if (file == NULL) {
        mct_log(LOG_ERR, "Setup internal data structure to parse config file failed\n");
        return NULL;
    }

    file->sections = calloc(sizeof(MctConfigFileSection), MCT_CONFIG_FILE_SECTIONS_MAX);

    /* open file */
    if ((hdl = fopen(file_name, "r")) == NULL) {
        mct_log(LOG_ERR, "Cannot open configuration file\n");
        free(file);
        return NULL;
    }

    mct_config_file_read_file(file, hdl);

    /* all information stored internally */
    fclose(hdl);

    return file;
}

void mct_config_file_release(MctConfigFile *file)
{
    int i = 0;

    if (file != NULL) {
        int max = file->num_sections;

        for (i = 0; i < max; i++) {
            MctConfigFileSection *s = &file->sections[i];
            MctConfigKeyData *node = file->sections[i].list;
            free(s->name);

            if (s->keys != NULL)
                free(s->keys);

            while (node) {
                MctConfigKeyData *tmp = node;
                node = node->next;
                free(tmp->key);
                free(tmp->data);
                free(tmp);
            }
        }

        free(file->sections);
        free(file);
    }
}

int mct_config_file_get_section_name(const MctConfigFile *file,
                                     int num,
                                     char *name)
{
    if ((file == NULL) || (name == NULL) || (num < 0) || (num >= file->num_sections))
        return -1;

    strncpy(name, (file->sections + num)->name, MCT_CONFIG_FILE_ENTRY_MAX_LEN);

    return 0;
}

int mct_config_file_get_num_sections(const MctConfigFile *file, int *num)
{
    if ((file == NULL) || (file->num_sections < 0))
        return -1;

    /*
     * Note: Since General section could be used in configuration file,
     * this number could be also containing General section.
     */
    *num = file->num_sections;

    return 0;
}

int mct_config_file_get_value(const MctConfigFile *file,
                              const char *section,
                              const char *key, char *value)
{
    MctConfigFileSection *s = NULL;
    MctConfigKeyData **tmp = NULL;
    int num_section = 0;

    if ((file == NULL) || (section == NULL) || (key == NULL) || (value == NULL))
        return -1;

    /* clean value */
    memset(value, 0, MCT_CONFIG_FILE_ENTRY_MAX_LEN);

    num_section = mct_config_file_find_section(file, section);

    if (num_section == -1)
        return -1;

    s = (file->sections + num_section);

    tmp = &s->list;

    while (*(tmp) != NULL) {
        if (strncmp((*tmp)->key, key, MCT_CONFIG_FILE_ENTRY_MAX_LEN) == 0) {
            strncpy(value, (*tmp)->data, MCT_CONFIG_FILE_ENTRY_MAX_LEN);
            return 0;
        }
        else { /* not found yet see list for more */
            tmp = &(*tmp)->next;
        }
    }

    mct_vlog(LOG_WARNING, "Entry does not exist in section: %s\n", key);
    return -1;
}

int mct_config_file_check_section_name_exists(const MctConfigFile *file,
                                             const char *name)
{
    int ret = 0;

    if ((file == NULL) || (file->num_sections <= 0) || (name == NULL))
        return -1;

    ret = mct_config_file_find_section(file, name);
    if (ret == -1)
        return ret;

    return 0;
}
