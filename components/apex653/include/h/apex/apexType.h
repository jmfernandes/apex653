/*
 * Shared APEX fixed-width types (ARINC 653 Part 1), needed by every service's
 * C-linkage interface. This header must stay valid C (it is included by both
 * the C-linkage service headers and by C++ translation units), so no C++-only
 * constructs belong here.
 */
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

/* ARINC 653 Part 1 §3.5 return codes, common to every APEX service. */
typedef enum RETURN_CODE_TYPE {
    NO_ERROR = 0,      /* request valid and performed */
    NO_ACTION = 1,     /* status of the system unaffected by the request */
    NOT_AVAILABLE = 2, /* requested resource is not available */
    INVALID_PARAM = 3, /* invalid parameter specified in the request */
    INVALID_CONFIG = 4,/* parameter incompatible with configuration */
    INVALID_MODE = 5,  /* request incompatible with the current mode */
    TIMED_OUT = 6      /* time-out associated with the request expired */
} RETURN_CODE_TYPE;

#ifdef __cplusplus
}
#endif

#endif /* POSIX_APEX_TYPE_H */
