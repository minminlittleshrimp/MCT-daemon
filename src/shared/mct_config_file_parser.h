#ifndef _MCT_CONFIG_FILE_PARSER_H_
#define _MCT_CONFIG_FILE_PARSER_H_


/* definitions */
#define MCT_CONFIG_FILE_PATH_MAX_LEN       100 /* absolute path including filename */
#define MCT_CONFIG_FILE_ENTRY_MAX_LEN      100 /* Entry for section, key and value */
#define MCT_CONFIG_FILE_LINE_MAX_LEN       210
#define MCT_CONFIG_FILE_SECTIONS_MAX       125
#define MCT_CONFIG_FILE_KEYS_MAX            25 /* Maximal keys per section */

typedef struct MctConfigKeyData
{
    char *key;
    char *data;
    struct MctConfigKeyData *next;
} MctConfigKeyData;

/* Config file section structure */
typedef struct
{
    int num_entries;          /* number of entries */
    char *name;               /* name of section */
    char *keys;               /* keys */
    MctConfigKeyData *list;
} MctConfigFileSection;

typedef struct
{
    int num_sections;               /* number of sections */
    MctConfigFileSection *sections; /* sections */
} MctConfigFile;

/**
 * mct_config_file_init
 *
 * Load the configuration file and stores all data in
 * internal data structures.
 *
 * @param file_name File to be opened
 * @return          Pointer to MctConfigFile object or NULL on error
 */
MctConfigFile *mct_config_file_init(char *file_name);

/**
 * mct_config_file_release
 *
 * Release config file and frees all internal data. Has to be called after
 * after all data is read.
 *
 * @param file      MctConfigFile
 */
void mct_config_file_release(MctConfigFile *file);

/**
 * mct_config_file_get_section_name
 *
 * Get name of section number.
 *
 * @param[in]  file      MctConfigFile
 * @param[in]  num       Number of section
 * @param[out] name      Section name
 * @return     0 on success, else -1
 */
int mct_config_file_get_section_name(const MctConfigFile *file,
                                     int num,
                                     char *name);

/**
 * mct_config_file_get_num_sections
 *
 * Get the number of sections inside configuration file
 *
 * @param[in]  file     MctConfigFile
 * @param[out] num      Number of sections inside configuration file
 * @return     0 on success, else -1
 */
int mct_config_file_get_num_sections(const MctConfigFile *file, int *num);

/**
 * mct_config_file_get_value
 *
 * Get value of key in specified section.
 *
 * @param[in]   file      MctConfigFile
 * @param[in]   section   Name of section
 * @param[in]   key       Key
 * @param[out]  value     Value
 * @return      0 on success, else -1
 */
int mct_config_file_get_value(const MctConfigFile *file,
                              const char *section,
                              const char *key,
                              char *value);

/**
 * mct_config_file_check_section_name_exists
 *
 * Get name of section number.
 *
 * @param[in]  file      MctConfigFile
 * @param[in]  name      Section name
 * @return     0 on success/exist, else -1
 */
int mct_config_file_check_section_name_exists(const MctConfigFile *file,
                                             const char *name);
#endif
