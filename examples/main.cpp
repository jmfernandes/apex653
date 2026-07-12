#include <apexFileSystem.h> // Declares APEX FileSystem API functions

#include <cstdio>
#include <cstring>

int main() {
    RETURN_CODE_TYPE return_code = NO_ERROR;

    apexFileSystemInit(&return_code);
    if (return_code != NO_ERROR) {
        std::fprintf(stderr, "apexFileSystemInit failed: RETURN_CODE=%d\n", return_code);
        return 1;
    }
    std::printf("apexFileSystemInit() -> RETURN_CODE=%d\n", return_code);

    FILE_NAME_TYPE file_name{};
    std::strncpy(file_name, "/tmp/apex653_example.txt", sizeof(file_name) - 1);
    std::remove(file_name); // OPEN_NEW_FILE uses O_EXCL, so start from a clean slate

    FILE_ID_TYPE file_id = -1;
    FILE_ERRNO_TYPE apex_errno = 0;

    OPEN_NEW_FILE(file_name, &file_id, &return_code, &apex_errno);

    std::printf("OPEN_NEW_FILE(\"%s\") -> RETURN_CODE=%d, FILE_ID=%d, ERRNO=%d\n",
                file_name, return_code, file_id, apex_errno);

    if (return_code == NO_ERROR) {
        CLOSE_FILE(file_id, &return_code, &apex_errno);
        std::printf("CLOSE_FILE(%d) -> RETURN_CODE=%d, ERRNO=%d\n", file_id, return_code, apex_errno);
    }

    return 0;
}
