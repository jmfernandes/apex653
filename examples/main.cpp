#include <apexFileSystem.h> // Declares APEX FileSystem API functions

#include <cstdio>
#include <cstring>

int main() {
    FILE_NAME_TYPE file_name{};
    std::strncpy(file_name, "example.txt", sizeof(file_name) - 1);

    FILE_ID_TYPE file_id = -1;
    RETURN_CODE_TYPE return_code = NO_ERROR;
    FILE_ERRNO_TYPE apex_errno = 0;

    OPEN_NEW_FILE(file_name, &file_id, &return_code, &apex_errno);

    std::printf("OPEN_NEW_FILE(\"%s\") -> RETURN_CODE=%d, FILE_ID=%d, ERRNO=%d\n",
                file_name, return_code, file_id, apex_errno);

    return 0;
}
