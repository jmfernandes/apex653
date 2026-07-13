
#include <apexFileSystem.h> // Declares APEX FileSystem API functions
#include <apexPthreadUtils.h> // Declares PthreadPrioInheritMutex
#include <sys/stat.h> // Defines fstat and struct stat
#include <fcntl.h> // Defines file options and open
#include <dirent.h> // Defines DIR
#include <unistd.h> // Defines fsync and other POSIX system calls
#include <atomic> // Defines std::atomic
#include <cstring> // Defines memset
#include <mutex> // Defines std::unique_lock, std::try_to_lock
#include <optional> // Defines std::optional
#include <ranges> // Defines std::views::split
#include <string_view> // Defines std::string_view
#include <type_traits> // Defines std::is_trivially_copyable_v

// This file zeroes these types with memset (see below) instead of assigning a value-initialized
// instance, which is only safe if they have no vtable, no non-trivial constructor, and no member
// that owns a resource. Enforce that assumption at compile time instead of relying on it silently.
static_assert(std::is_trivially_copyable_v<FILE_STATUS_TYPE>);
static_assert(std::is_trivially_copyable_v<COMPOSITE_TIME_TYPE>);

#define MAX_OPEN_FILES 128
#define MAX_OPEN_DIRS 16

typedef struct
{
    FILE_STATUS_TYPE status;
    FILE_MODE_TYPE mode;
    FILE_ID_TYPE fd;
    APEX_INTEGER inUse;
    char fileName[MAX_FILE_NAME_LENGTH];
    PthreadPrioInheritMutex mutex;
} InternalFileEntry;

typedef struct
{
    DIR* dir;
    DIRECTORY_ID_TYPE id;
    APEX_INTEGER inUse;
    char dirName[MAX_FILE_NAME_LENGTH];
    PthreadPrioInheritMutex mutex;
} InternalDirEntry;

// Mutexes are constructed (with PTHREAD_PRIO_INHERIT) as part of this static array's own
// initialization, before main() runs, so apexFileSystemInit only needs to reset table contents.
static InternalFileEntry g_fileTable[MAX_OPEN_FILES];
static InternalDirEntry g_dirTable[MAX_OPEN_DIRS];
static std::atomic<bool> g_fileTableInit{false};
static std::atomic<bool> g_dirTableInit{false};

void apexFileSystemInit(
    RETURN_CODE_TYPE* RETURN_CODE
)
{
    if (NULL == RETURN_CODE)
    {
        return;
    }

    if (g_fileTableInit.load() && g_dirTableInit.load())
    {
        *RETURN_CODE = NO_ACTION;
        return;
    }

    for (std::size_t i = 0; i < MAX_OPEN_FILES; i++)
    {
        g_fileTable[i].fd = -1;
        g_fileTable[i].mode = READ;
        memset(&g_fileTable[i].status, 0, sizeof(FILE_STATUS_TYPE));
        g_fileTable[i].status.CREATE_TIME.TM_IS_SET = UNSET;
        g_fileTable[i].status.LAST_UPDATE.TM_IS_SET = UNSET;
        g_fileTable[i].inUse = 0;
        g_fileTable[i].fileName[0] = '\0';
    }
    g_fileTableInit.store(true);

    for (std::size_t i = 0; i < MAX_OPEN_DIRS; i++)
    {
        g_dirTable[i].dir = NULL;
        g_dirTable[i].id = -1;
        g_dirTable[i].inUse = 0;
        g_dirTable[i].dirName[0] = '\0';
    }
    g_dirTableInit.store(true);

    *RETURN_CODE = NO_ERROR;
    return;
}

namespace {

void getCurrentCompositeTime(COMPOSITE_TIME_TYPE* ct)
{
    time_t now = time(NULL);

    if (now == static_cast<time_t>(-1))
    {
        // Time source not available
        memset(ct, 0, sizeof(COMPOSITE_TIME_TYPE));
        ct->TM_IS_SET = UNSET;
        return;
    }

    struct tm tm_buf;
    if (localtime_r(&now, &tm_buf) == NULL)
    {
        // Local time not available
        memset(ct, 0, sizeof(COMPOSITE_TIME_TYPE));
        ct->TM_IS_SET = UNSET;
        return;
    }

    ct->TM_SEC = tm_buf.tm_sec;
    ct->TM_MIN = tm_buf.tm_min;
    ct->TM_HOUR = tm_buf.tm_hour;
    ct->TM_MDAY = tm_buf.tm_mday;
    ct->TM_MON = tm_buf.tm_mon;
    ct->TM_YEAR = tm_buf.tm_year;
    ct->TM_WDAY = tm_buf.tm_wday;
    ct->TM_YDAY = tm_buf.tm_yday;
    ct->TM_ISDST = tm_buf.tm_isdst;
    ct->TM_IS_SET = SET;
}

void validateFilename(FILE_NAME_TYPE fileName, RETURN_CODE_TYPE* rc, FILE_ERRNO_TYPE* err, bool& isNominal)
{
    if (!isNominal)
    {
        /* skip */
    }
    else if (fileName == NULL || fileName[0] == '\0')
    {
        *rc = INVALID_PARAM;
        *err = EINVAL; /* Invalid argument - ID: 22 */
        isNominal = false;
    }
    else
    {
        const std::string_view name(fileName);
        if (name.size() >= MAX_FILE_NAME_LENGTH)
        {
            *rc = INVALID_PARAM;
            *err = ENAMETOOLONG; /* Name too long - ID: 36 */
            isNominal = false;
        }
        else if (name.front() != '/')
        {
            *rc = INVALID_PARAM;
            *err = EINVAL; /* Invalid argument - ID: 22 */
            isNominal = false;
        }
        else
        {
            /* Check max directory component length */
            for (const auto component : name | std::views::split('/'))
            {
                const std::string_view piece(component.begin(), component.end());
                if (isNominal && piece.size() >= MAX_DIRECTORY_ENTRY_LENGTH)
                {
                    *rc = INVALID_PARAM;
                    *err = ENAMETOOLONG; /* Name too long - ID: 36 */
                    isNominal = false;
                }
            }
        }
    }
    return;
}

void checkFileInit(RETURN_CODE_TYPE* rc, FILE_ERRNO_TYPE* err, bool& isNominal)
{
    if (isNominal && !g_fileTableInit.load())
    {
        if (err != NULL)
        {
            *err = EACCES;
        }
        *rc = INVALID_MODE;
        isNominal = false;
    }
    return;
}

void checkNullParameters(RETURN_CODE_TYPE* rc, FILE_ERRNO_TYPE* err, bool& isNominal)
{
    if (NULL == rc)
    {
        isNominal = false;
    }
    if (isNominal && NULL == err)
    {
        *rc = INVALID_PARAM;
        isNominal = false;
    }
    return;
}

// Satisfied by any internal table entry (InternalFileEntry, InternalDirEntry, ...) that follows
// this file's per-slot locking convention: a PthreadPrioInheritMutex to try-lock, and an inUse
// flag to test once the lock is held.
template <typename Entry>
concept TableEntry = requires(Entry& entry) {
    { entry.mutex } -> std::same_as<PthreadPrioInheritMutex&>;
    { entry.inUse } -> std::convertible_to<bool>;
};

// A free slot found by findFreeSlot, still holding that slot's lock. The mutex is released when
// this (or the std::unique_lock inside it) goes out of scope.
template <TableEntry Entry>
struct FreeSlot
{
    std::size_t index;
    std::unique_lock<PthreadPrioInheritMutex> lock;
};

// Scans table[0, count) for a slot that is both uncontended (try-lock succeeds) and not in use,
// returning it still locked so the caller can fill it in without another thread racing in
// between finding the slot and claiming it. Shared by OPEN_NEW_FILE today and intended for an
// equivalent directory-open function later — both g_fileTable and g_dirTable satisfy TableEntry.
template <TableEntry Entry>
[[nodiscard]] std::optional<FreeSlot<Entry>> findFreeSlot(Entry* table, std::size_t count)
{
    for (std::size_t i = 0; i < count; i++)
    {
        std::unique_lock<PthreadPrioInheritMutex> candidate(table[i].mutex, std::try_to_lock);
        if (!candidate.owns_lock())
        {
            continue; // Slot is contended
        }
        if (table[i].inUse)
        {
            continue; // Slot owns mutex but entry is already in use
        }
        return FreeSlot<Entry>{i, std::move(candidate)};
    }
    return std::nullopt;
}

} // namespace

void OPEN_NEW_FILE(FILE_NAME_TYPE FILE_NAME,
    FILE_ID_TYPE *FILE_ID,
    RETURN_CODE_TYPE *RETURN_CODE,
    FILE_ERRNO_TYPE *ERRNO)
{
    /* --- Parameter Validation -------------------------------------------- */
    bool isNominal = true;
    checkNullParameters(RETURN_CODE, ERRNO, isNominal);
    checkFileInit(RETURN_CODE, ERRNO, isNominal);
    validateFilename(FILE_NAME, RETURN_CODE, ERRNO, isNominal);

    if (isNominal && (FILE_ID == NULL))
    {
        *RETURN_CODE = INVALID_PARAM;
        isNominal = false;
    }
    
    /* --- Entry Validation ------------------------------------------------ */
    std::optional<std::size_t> slot;
    if (isNominal)
    {
        if (FILE_ID != NULL)
        {
            *FILE_ID = -1;
        }

        if (auto found = findFreeSlot(g_fileTable, static_cast<std::size_t>(MAX_OPEN_FILES)))
        {
            auto& [i, candidate] = *found; // candidate stays locked for the rest of this block
            InternalFileEntry* entry = &g_fileTable[i];
            slot = i;

            // This is a pre-open check that Linux kernel's open() won't distinguish.
            // POSIX open returns EEXIST for both existing files and existing directories.
            // The ARINC 653 spec requires EISDIR for directories.
            struct stat preStat;
            if (stat(FILE_NAME, &preStat) == 0)
            {
                if (S_ISDIR(preStat.st_mode))
                {
                    *RETURN_CODE = INVALID_PARAM;
                    *ERRNO = EISDIR; // Is a directory - ID: 21
                }
                else
                {
                    *RETURN_CODE = INVALID_PARAM;
                    *ERRNO = EEXIST; // File exists - ID: 17
                }
                isNominal = false;
            }

            /* --- File I/O -------------------------------------------- */
            int fd = -1;
            int savedErrno = errno;
            if (isNominal)
            {
                *ERRNO = 0;
                fd = open(FILE_NAME, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                savedErrno = errno;
            }
            if (isNominal && fd >= 0)
            {
                // Fill out data in the struct
                entry->fd = fd;
                entry->mode = READ_WRITE;
                strncpy(entry->fileName, FILE_NAME, MAX_FILE_NAME_LENGTH - 1);
                entry->fileName[MAX_FILE_NAME_LENGTH - 1] = '\0';
                memset(&entry->status, 0, sizeof(FILE_STATUS_TYPE));
                getCurrentCompositeTime(&entry->status.CREATE_TIME);
                getCurrentCompositeTime(&entry->status.LAST_UPDATE);
                entry->inUse = 1;
            }

            /* --- Result Processing ----------------------------------- */
            if (isNominal && fd < 0)
            {
                *ERRNO = static_cast<FILE_ERRNO_TYPE>(savedErrno);
                switch (savedErrno)
                {
                    case ENOTDIR:
                    case EISDIR:
                    case EEXIST:
                    case EROFS:
                    case EINVAL:
                    case ENAMETOOLONG:
                        *RETURN_CODE = INVALID_PARAM;
                        break;
                    case EACCES:
                    case ENOSPC:
                        *RETURN_CODE = INVALID_CONFIG;
                        break;
                    case EIO:
                        *RETURN_CODE = NOT_AVAILABLE;
                        break;
                    default:
                        *RETURN_CODE = INVALID_PARAM;
                        break;
                }
            }
            else if (isNominal)
            {
                // Success case
                *RETURN_CODE = NO_ERROR;
                *ERRNO = 0;
                *FILE_ID = static_cast<FILE_ID_TYPE>(*slot);
            }
        } // Destructor of found->lock releases the mutex here
    }

    // No free slot available.
    if (isNominal && !slot.has_value())
    {
        *RETURN_CODE = INVALID_CONFIG;
        *ERRNO = EMFILE; // Max files open - ID: 24
    }

    return;
}

void CLOSE_FILE(FILE_ID_TYPE FILE_ID,
    RETURN_CODE_TYPE *RETURN_CODE,
    FILE_ERRNO_TYPE* ERRNO)
{
    /* --- Parameter Validation -------------------------------------------- */
    bool isNominal = true;
    checkNullParameters(RETURN_CODE, ERRNO, isNominal);
    checkFileInit(RETURN_CODE, ERRNO, isNominal);
;

    if (isNominal && (FILE_ID < 0 || FILE_ID >= MAX_OPEN_FILES)) {
        *RETURN_CODE = INVALID_PARAM;
        *ERRNO = EBADF; // Bad file descriptor - ID: 9
        isNominal = false;
    }

    /* --- Entry Validation ------------------------------------------------ */
    // Per ARINC 653 Part 2 Section 3.2.5.3:
    // When FILE_ID has an operation in progress return NOT_AVAILABLE, EBUSY
    // Use trylock instead of lock for this purpose.
    InternalFileEntry* entry = NULL;
    std::unique_lock<PthreadPrioInheritMutex> lock;
    if (isNominal)
    {
        entry = &g_fileTable[FILE_ID];
        lock = std::unique_lock<PthreadPrioInheritMutex>(entry->mutex, std::try_to_lock);
        if (!lock.owns_lock())
        {
            *RETURN_CODE = NOT_AVAILABLE;
            *ERRNO = EBUSY; // File is busy - ID: 16
            isNominal = false;
        }
    }

    // Guard against other threads that may have closed the file OR
    // OPEN_FILE has not yet fully completed.
    if (isNominal && !entry->inUse)
    {
        *RETURN_CODE = INVALID_PARAM;
        *ERRNO = EBADF; // Bad file descriptor - ID: 9
        isNominal = false;
    }

    /* --- File I/O -------------------------------------------------------- */
    ssize_t result = -1;
    int savedErrno = errno;
    if (isNominal)
    {
        if (entry->mode == READ_WRITE)
        {
            fsync(entry->fd);
        }

        *ERRNO = 0;
        result = close(entry->fd);
        savedErrno = errno;

        entry->fd = -1;
        entry->fileName[0] = '\0';
        entry->inUse = 0;
    }

    /* --- Result Processing ----------------------------------------------- */
    if (isNominal && result != 0)
    {
        *ERRNO = static_cast<FILE_ERRNO_TYPE>(savedErrno);

        switch(savedErrno)
        {
            case EIO:
                *RETURN_CODE = NOT_AVAILABLE;
                break;
            default:
                *RETURN_CODE = NOT_AVAILABLE;
                break;
        }
    }
    else if (isNominal)
    {
        // Success case
        *RETURN_CODE = NO_ERROR;
        *ERRNO = 0;
    }

    // Entry mutex lock's destructor runs here, unlock iff it acquired the mutex.
    return;
}

void READ_FILE(FILE_ID_TYPE FILE_ID,
    MESSAGE_ADDR_TYPE MESSAGE_ADDR,
    MESSAGE_SIZE_TYPE IN_LENGTH,
    MESSAGE_SIZE_TYPE *OUT_LENGTH,
    RETURN_CODE_TYPE *RETURN_CODE,
    FILE_ERRNO_TYPE *ERRNO)
{
    /* --- Parameter Validation -------------------------------------------- */
    bool isNominal = true;
    checkNullParameters(RETURN_CODE, ERRNO, isNominal);
    checkFileInit(RETURN_CODE, ERRNO, isNominal);

    if (isNominal && (FILE_ID < 0 || FILE_ID >= MAX_OPEN_FILES))
    {
        *RETURN_CODE = INVALID_PARAM;
        *ERRNO = EBADF; // Bad file descriptor - ID: 9
        isNominal = false;
    }
    else if (isNominal && OUT_LENGTH == NULL)   
    {
        *RETURN_CODE = INVALID_PARAM;
        *ERRNO = EINVAL; // Invalid argument - ID: 22
        isNominal = false;
    }
    else if (isNominal && MESSAGE_ADDR == NULL)
    {
        *RETURN_CODE = INVALID_PARAM;
        *ERRNO = EINVAL; // Invalid argument - ID: 22
        isNominal = false;
    }
    else if (isNominal && IN_LENGTH <= 0)
    {
        *RETURN_CODE = INVALID_PARAM;
        *ERRNO = EINVAL; // Invalid argument - ID: 22
        isNominal = false;
    }
    else if (isNominal && IN_LENGTH > MAX_ATOMICITY)
    {
        *RETURN_CODE = INVALID_PARAM;
        *ERRNO = EFBIG; // File too large - ID: 27
        isNominal = false;
    }

    /* --- Entry Validation ------------------------------------------------ */
    // Per ARINC 653 Part 2 Section 3.2.5.3
    // When FILE_ID has an operation in progress return NOT_AVAILABLE, EBUSY
    // Use trylock instead of lock for this purpose.
    InternalFileEntry* entry = NULL;
    std::unique_lock<PthreadPrioInheritMutex> lock;

    if (isNominal)
    {
        entry = &g_fileTable[FILE_ID];
        lock = std::unique_lock<PthreadPrioInheritMutex>(entry->mutex, std::try_to_lock);
        if (!lock.owns_lock())
        {
            *RETURN_CODE = NOT_AVAILABLE;
            *ERRNO = EBUSY; // File is busy - ID: 16
            isNominal = false;
        }
    }

    // Guard against other threads that may have close the file.
    if (isNominal && !entry->inUse)
    {
        *RETURN_CODE = INVALID_PARAM;
        *ERRNO = EBADF; // Bad file descriptor - ID: 9
        isNominal = false;
    }

    // Guard against RESIZE_FILE shrinking file before READ
    if (isNominal)
    {
        off_t curPos = lseek(entry->fd, 0, SEEK_CUR);
        struct stat st;
        if (fstat(entry->fd, &st) == 0 && curPos > st.st_size)
        {
            *RETURN_CODE = NOT_AVAILABLE;
            *ERRNO = EOVERFLOW; // Position greater than file size - ID: 75
            isNominal = false;
        }
    }

    /* --- File I/O -------------------------------------------------------- */
    ssize_t result = -1;
    int savedErrno = errno;
    if (OUT_LENGTH != NULL)
    {
        *OUT_LENGTH = 0;
    }
    if (isNominal)
    {
        *ERRNO = 0;
        // read() returns the number of bytest read, which may be less than IN_LENGTH if we hit EOF.
        result = read(entry->fd, MESSAGE_ADDR, static_cast<size_t>(IN_LENGTH));
        savedErrno = errno;
    }

    /* --- Result Processing ----------------------------------------------- */
    if (isNominal && result < 0)
    {
        *ERRNO = static_cast<FILE_ERRNO_TYPE>(savedErrno);

        switch(savedErrno)
        {
            case EBADF:
                *RETURN_CODE = INVALID_PARAM;
                break;
            case ESTALE:
            case EIO:
                *RETURN_CODE = NOT_AVAILABLE;
                break;
            default:
                *RETURN_CODE = NOT_AVAILABLE;
                break;
        }
    }
    else if (isNominal)
    {
        // Success case.
        *RETURN_CODE = NO_ERROR;
        *ERRNO = 0;
        *OUT_LENGTH = static_cast<MESSAGE_SIZE_TYPE>(result);
    }

    // Entry mutex lock's destructor runs here, unlock iff it acquired the mutex.
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