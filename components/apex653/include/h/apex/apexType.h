/// @file apexType.h
/// @brief Shared APEX fixed-width types (ARINC 653 Part 2), needed by every
///        service's C-linkage interface.
///
/// This header must stay valid C (it is included by both the C-linkage
/// service headers and by C++ translation units), so no C++-only constructs
/// belong here.

#ifndef POSIX_APEX_TYPE_H
#define POSIX_APEX_TYPE_H

#include <stdint.h> // Defines int32_t et all

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t APEX_INTEGER;
typedef int64_t APEX_LONG_INTEGER;
typedef uint8_t APEX_BYTE;
typedef char APEX_CHAR;

typedef APEX_BYTE* MESSAGE_ADDR_TYPE;
typedef APEX_INTEGER MESSAGE_SIZE_TYPE;
typedef APEX_INTEGER MESSAGE_RANGE_TYPE;

/// @brief ARINC 653 Part 2 §3.5 return codes, common to every APEX service.
typedef enum RETURN_CODE_TYPE {
    NO_ERROR = 0,       ///< Request valid and performed.
    NO_ACTION = 1,      ///< Status of the system unaffected by the request.
    NOT_AVAILABLE = 2,  ///< Requested resource is not available.
    INVALID_PARAM = 3,  ///< Invalid parameter specified in the request.
    INVALID_CONFIG = 4, ///< Parameter incompatible with configuration.
    INVALID_MODE = 5,   ///< Request incompatible with the current mode.
    TIMED_OUT = 6       ///< Time-out associated with the request expired.
} RETURN_CODE_TYPE;

#ifdef __cplusplus
}
#endif

#endif /* POSIX_APEX_TYPE_H */
