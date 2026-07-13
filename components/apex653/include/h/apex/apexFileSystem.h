/// @file apexFileSystem.h
/// @brief Declares POSIX ARINC 653 APEX API file system types and definitions.
///
/// @section arinc653_description ARINC653 File System Description.
///
/// Reproduced from Avionics Application Software Standard Interface Part 2
/// Extended Services ARINC Specification 653 PW.
///
/// The File System service provides a mechanism for partitions to access
/// persistent storage through a standardized interface.

#ifndef POSIX_APEX_FILESYSTEM_H
#define POSIX_APEX_FILESYSTEM_H

#include <apexType.h> // Declares APEX common types
#include <errno.h> // Defines POSIX error values
#include <limits.h> // Defines PATH_MAX

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAX_ATOMICITY
#define MAX_ATOMICITY 0x7FFFF000
#endif

#define MAX_FILE_NAME_LENGTH PATH_MAX
#define MAX_DIRECTORY_ENTRY_LENGTH 64 /* 63 char + null */

typedef char FILE_NAME_TYPE[MAX_FILE_NAME_LENGTH];
typedef APEX_INTEGER FILE_ID_TYPE;
typedef APEX_INTEGER FILE_ERRNO_TYPE;
typedef APEX_LONG_INTEGER FILE_SIZE_TYPE;
typedef APEX_ACCESS_MODE_TYPE FILE_MODE_TYPE;
typedef char DIRECTORY_ENTRY_TYPE[MAX_DIRECTORY_ENTRY_LENGTH];
typedef APEX_INTEGER DIRECTORY_ID_TYPE;

// Deliberately not named SEEK_SET/SEEK_CUR/SEEK_END: those are macros defined by <unistd.h>,
// and reusing those names here is silently hijacked by the preprocessor in any translation unit
// that includes both headers -- the compiler never even sees these as enum references.
typedef enum
{
    FROM_FILE_START = 0,
    FROM_FILE_CURRENT = 1,
    FROM_FILE_END = 2
} FILE_SEEK_TYPE;

typedef enum
{
    UNSET = 0,
    SET = 1
} TIME_SET_TYPE;

typedef struct
{
    APEX_INTEGER TM_SEC;
    APEX_INTEGER TM_MIN;
    APEX_INTEGER TM_HOUR;
    APEX_INTEGER TM_MDAY;
    APEX_INTEGER TM_MON;
    APEX_INTEGER TM_YEAR;
    APEX_INTEGER TM_WDAY;
    APEX_INTEGER TM_YDAY;
    APEX_INTEGER TM_ISDST;
    APEX_INTEGER TM_IS_SET;
} COMPOSITE_TIME_TYPE;

typedef struct
{
    COMPOSITE_TIME_TYPE CREATE_TIME;
    COMPOSITE_TIME_TYPE LAST_UPDATE;
    FILE_SIZE_TYPE POSITION;
    FILE_SIZE_TYPE SIZE;
    APEX_INTEGER NB_OF_CHANGES;
    APEX_INTEGER NB_OF_WRITE_ERRORS;
} FILE_STATUS_TYPE;

/// @brief Initializes the APEX file system tables. Must be called exactly once.
///
/// @param[out] RETURN_CODE Status of the request.
extern void apexFileSystemInit(
    RETURN_CODE_TYPE* RETURN_CODE
);

/// @brief Creates and opens a new file for writing.
///
/// ARINC 653 Part 2 §3.2.5.1. Corresponds to the POSIX creat() function.
///
/// @param FILE_NAME Name of the file to create.
/// @param[out] FILE_ID Identifier of the newly opened file.
/// @param[out] RETURN_CODE Status of the request.
/// @param[out] ERRNO POSIX errno set when RETURN_CODE indicates failure.
extern void OPEN_NEW_FILE(
    FILE_NAME_TYPE FILE_NAME,
    FILE_ID_TYPE* FILE_ID,
    RETURN_CODE_TYPE* RETURN_CODE,
    FILE_ERRNO_TYPE* ERRNO
);

extern void CLOSE_FILE(
    FILE_ID_TYPE FILE_ID,
    RETURN_CODE_TYPE* RETURN_CODE,
    FILE_ERRNO_TYPE* ERRNO
);

extern void READ_FILE(
    FILE_ID_TYPE FILE_ID,
    MESSAGE_ADDR_TYPE MESSAGE_ADDR,
    MESSAGE_SIZE_TYPE IN_LENGTH,
    MESSAGE_SIZE_TYPE* OUT_LENGTH,
    RETURN_CODE_TYPE* RETURN_CODE,
    FILE_ERRNO_TYPE* ERRNO
);

extern void WRITE_FILE(
    FILE_ID_TYPE FILE_ID,
    MESSAGE_ADDR_TYPE MESSAGE_ADDR,
    MESSAGE_SIZE_TYPE LENGTH,
    RETURN_CODE_TYPE* RETURN_CODE,
    FILE_ERRNO_TYPE* ERRNO
);

extern void SEEK_FILE(
    FILE_ID_TYPE FILE_ID,
    FILE_SIZE_TYPE OFFSET,
    FILE_SEEK_TYPE WHENCE,
    FILE_SIZE_TYPE* POSITION,
    RETURN_CODE_TYPE* RETURN_CODE,
    FILE_ERRNO_TYPE* ERRNO
);

extern void REMOVE_FILE(
    FILE_NAME_TYPE FILE_NAME,
    RETURN_CODE_TYPE* RETURN_CODE,
    FILE_ERRNO_TYPE* ERRNO
);

#ifdef __cplusplus
}
#endif

#endif /* POSIX_APEX_FILESYSTEM_H */