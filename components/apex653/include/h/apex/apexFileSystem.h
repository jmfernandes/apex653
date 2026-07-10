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

#define MAX_FILE_NAME_LENGTH PATH_MAX

typedef char FILE_NAME_TYPE [MAX_FILE_NAME_LENGTH];
typedef APEX_INTEGER FILE_ID_TYPE;
typedef APEX_INTEGER FILE_ERRNO_TYPE;

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

#ifdef __cplusplus
}
#endif

#endif /* POSIX_APEX_FILESYSTEM_H */