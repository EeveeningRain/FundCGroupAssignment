#ifndef fileproperty
#define fileproperty

/* This function searches for a given filename. */
/* This function returns 0 if a file was sucessfully found and 1 if not. */
int filesearch(char *argv[]);

/* This function searches a file and returns its size in bytes. */
long filesize(char *argv[]);

#endif /* fileproperty.h */
