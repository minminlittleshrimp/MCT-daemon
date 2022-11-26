#ifndef _DLT_CONFIG_FILE_PARSER_H_
#define _DLT_CONFIG_FILE_PARSER_H_


/* definitions */
#define DLT_CONFIG_FILE_PATH_MAX_LEN       100 /* absolute path including filename */
#define DLT_CONFIG_FILE_ENTRY_MAX_LEN      100 /* Entry for section, key and value */
#define DLT_CONFIG_FILE_LINE_MAX_LEN       210
#define DLT_CONFIG_FILE_SECTIONS_MAX       125
#define DLT_CONFIG_FILE_KEYS_MAX            25 /* Maximal keys per section */

typedef struct DltConfigKeyData
{
    char *key;
    char *data;
    struct DltConfigKeyData *next;
} DltConfigKeyData;

/* Config file section structure */
typedef struct
{
    int num_entries;          /* number of entries */
    char *name;               /* name of section */
    char *keys;               /* keys */
    DltConfigKeyData *list;
} DltConfigFileSection;

typedef struct
{
    int num_sections;               /* number of sections */
    DltConfigFileSection *sections; /* sections */
} DltConfigFile;

/**
 * mct_config_file_init
 *
 * Load the configuration file and stores all data in
 * internal data structures.
 *
 * @param file_name File to be opened
 * @return          Pointer to DltConfigFile object or NULL on error
 */
DltConfigFile *mct_config_file_init(char *file_name);

/**
 * mct_config_file_release
 *
 * Release config file and frees all internal data. Has to be called after
 * after all data is read.
 *
 * @param file      DltConfigFile
 */
void mct_config_file_release(DltConfigFile *file);

/**
 * mct_config_file_get_section_name
 *
 * Get name of section number.
 *
 * @param[in]  file      DltConfigFile
 * @param[in]  num       Number of section
 * @param[out] name      Section name
 * @return     0 on success, else -1
 */
int mct_config_file_get_section_name(const DltConfigFile *file,
                                     int num,
                                     char *name);

/**
 * mct_config_file_get_num_sections
 *
 * Get the number of sections inside configuration file
 *
 * @param[in]  file     DltConfigFile
 * @param[out] num      Number of sections inside configuration file
 * @return     0 on success, else -1
 */
int mct_config_file_get_num_sections(const DltConfigFile *file, int *num);

/**
 * mct_config_file_get_value
 *
 * Get value of key in specified section.
 *
 * @param[in]   file      DltConfigFile
 * @param[in]   section   Name of section
 * @param[in]   key       Key
 * @param[out]  value     Value
 * @return      0 on success, else -1
 */
int mct_config_file_get_value(const DltConfigFile *file,
                              const char *section,
                              const char *key,
                              char *value);

/**
 * mct_config_file_check_section_name_exists
 *
 * Get name of section number.
 *
 * @param[in]  file      DltConfigFile
 * @param[in]  name      Section name
 * @return     0 on success/exist, else -1
 */
int mct_config_file_check_section_name_exists(const DltConfigFile *file,
                                             const char *name);
#endif
