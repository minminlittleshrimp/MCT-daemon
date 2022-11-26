#ifndef _MCT_LOGSTORAGE_LIST_H_
#define _MCT_LOGSTORAGE_LIST_H_

/* Return 0 it the node as been added (or is already present) */
int logstorage_store_dev_info(const char *node, const char *path);
/* Returns the mount point if node has been found and deleted. NULL either */
char *logstorage_delete_dev_info(const char *node);

#endif
