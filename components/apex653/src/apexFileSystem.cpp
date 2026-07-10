
#include <apexFileSystem.h> // Declares APEX FileSystem API functions
#include <cstring> // Defines memset
#include <pthread.h> // Defines pthread_mutex_t

#define MAX_OPEN_FILES 128
#define MAX_OPEN_DIRS 16

typedef struct
{
    FILE_STATUS_TYPE status;
    FILE_MODE_TYPE mode;
    FILE_ID_TYPE id;
    APEX_INTEGER inUse;
    char fileName[MAX_FILE_NAME_LENGTH];
    pthread_mutex_t mutex;
} InternalFileEntry; 

static InternalFileEntry g_fileTable[MAX_OPEN_FILES];
static int g_fileTableInit = 0;

void apexFileSystemInit(
    RETURN_CODE_TYPE* RETURN_CODE
)
{
    if (NULL == RETURN_CODE)
    {
        return;
    }

    if (g_fileTableInit)
    {
        *RETURN_CODE = NO_ACTION;
        return;
    }

    pthread_mutexattr_t mtxAttr;
    pthread_mutexattr_init(&mtxAttr);
    pthread_mutexattr_setprotocol(&mtxAttr, PTHREAD_PRIO_INHERIT);

    for (int i = 0; i < MAX_OPEN_FILES; i++)
    {
        g_fileTable[i].id = -1;
        g_fileTable[i].mode = READ;
        memset(&g_fileTable[i].status, 0, sizeof(FILE_STATUS_TYPE));
        g_fileTable[i].status.CREATE_TIME.TM_IS_SET = UNSET;
        g_fileTable[i].status.LAST_UPDATE.TM_IS_SET = UNSET;
        g_fileTable[i].inUse = 0;
        g_fileTable[i].fileName[0] = '\0';
        pthread_mutex_init(&g_fileTable[i].mutex, &mtxAttr);
    }
    g_fileTableInit = 1;

    pthread_mutexattr_destroy(&mtxAttr);
    *RETURN_CODE = NO_ERROR;
    return;
}

void OPEN_NEW_FILE(FILE_NAME_TYPE FILE_NAME,
    FILE_ID_TYPE *FILE_ID,
    RETURN_CODE_TYPE *RETURN_CODE,
    FILE_ERRNO_TYPE *ERRNO)
{
    *FILE_ID = -1;
    *RETURN_CODE = NO_ERROR;
    *ERRNO = 0;
    return;
}

void CLOSE_FILE(FILE_ID_TYPE FILE_ID,
    RETURN_CODE_TYPE *RETURN_CODE,
    FILE_ERRNO_TYPE* ERRNO)
{
    *RETURN_CODE = NO_ERROR;
    *ERRNO = 0;
    return;
}

void READ_FILE(FILE_ID_TYPE FILE_ID,
    MESSAGE_ADDR_TYPE MESSAGE_ADDR,
    MESSAGE_SIZE_TYPE IN_LENGTH,
    MESSAGE_SIZE_TYPE *OUT_LENGTH,
    RETURN_CODE_TYPE *RETURN_CODE,
    FILE_ERRNO_TYPE *ERRNO)
{
    *OUT_LENGTH = 0;
    *RETURN_CODE = NO_ERROR;
    *ERRNO = 0;
    return;
}

void WRITE_FILE(FILE_ID_TYPE FILE_ID,
    MESSAGE_ADDR_TYPE MESSAGE_ADDR,
    MESSAGE_SIZE_TYPE LENGTH,
    RETURN_CODE_TYPE *RETURN_CODE,
    FILE_ERRNO_TYPE *ERRNO)
{
    *RETURN_CODE = NO_ERROR;
    *ERRNO = 0;
    return;
}

void REMOVE_FILE(FILE_NAME_TYPE FILE_NAME,
    RETURN_CODE_TYPE *RETURN_CODE,
    FILE_ERRNO_TYPE *ERRNO)
{
    *RETURN_CODE = NO_ERROR;
    *ERRNO = 0;
    return;
}