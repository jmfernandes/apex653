
#ifndef POSIX_APEX_FILESYSTEM_H
#define POSIX_APEX_FILESYSTEM_H

#include <apexType.h> // Declares APEX common types
#include <errno.h> // Defines POSIX error values
#include <limits.h> // Defines PATH_MAX

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_FILE_NAME_LENGTH PATH_MAX

typedef char FILE_NAME_TYPE [MAX_FILE_NAME_LENGTH];
typedef APEX_INTEGER FILE_ID_TYPE;
typedef APEX_INTEGER FILE_ERRNO_TYPE;

extern void OPEN_NEW_FILE(
    FILE_NAME_TYPE FILE_NAME,
    FILE_ID_TYPE* FILE_ID,
    RETURN_CODE_TYPE* RETURN_CODE,
    FILE_ERRNO_TYPE* ERRNO
);

#ifdef __cplusplus
}
#endif

#endif /* POSIX_APEX_FILESYSTEM_H */