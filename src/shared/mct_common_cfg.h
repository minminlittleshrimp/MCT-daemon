#ifndef DLT_COMMON_CFG_H
#define DLT_COMMON_CFG_H

/*************/
/* Changable */
/*************/

/* Buffer length for temporary buffer */
#define DLT_COMMON_BUFFER_LENGTH 255

/* Number of ASCII chars to be printed in one line as HEX and as ASCII */
/* e.g. XX XX XX XX ABCD is DLT_COMMON_HEX_CHARS = 4 */
#define DLT_COMMON_HEX_CHARS  16

/* Length of line number */
#define DLT_COMMON_HEX_LINELEN 8

/* Length of one char */
#define DLT_COMMON_CHARLEN     1

/* Number of indices to be allocated at one, if no more indeces are left */
#define DLT_COMMON_INDEX_ALLOC       1000

/* If limited output is called,
 * this is the maximum number of characters to be printed out */
#define DLT_COMMON_ASCII_LIMIT_MAX_CHARS 20

/* This defines the dummy ECU ID set in storage header during import
 * of a message from a DLT file in RAW format (without storage header) */
#define DLT_COMMON_DUMMY_ECUID "ECU"


/************************/
/* Don't change please! */
/************************/

/* ASCII value for space */
#define DLT_COMMON_ASCII_CHAR_SPACE  32

/* ASCII value for tilde */
#define DLT_COMMON_ASCII_CHAR_TILDE 126

/* ASCII value for lesser than */
#define DLT_COMMON_ASCII_CHAR_LT     60

#endif /* DLT_COMMON_CFG_H */

