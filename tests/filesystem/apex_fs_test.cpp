// Tests for the public OPEN_NEW_FILE / CLOSE_FILE API. Unlike apex_fs_internal_test.cpp, this
// file links apex653::apex653 normally -- OPEN_NEW_FILE/CLOSE_FILE are extern "C" and exported,
// so there's no need for the #include-the-.cpp trick here.
#include <apexFileSystem.h> // Declares APEX FileSystem API functions

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace {

// FILE_NAME_TYPE is a raw array, and functions cannot return array types, so this wraps it in a
// struct purely to make literal names usable as rvalues at each call site below.
struct FileName {
    FILE_NAME_TYPE value{};
};

FileName make_name(const std::string& name) {
    FileName result;
    std::strncpy(result.value, name.c_str(), sizeof(result.value) - 1);
    return result;
}

} // namespace

// Base fixture: initializes the file system once per test, and tracks every file this test opens
// (by id and by path) so TearDown can close and delete all of them regardless of whether the
// test passed, failed, or aborted partway through via a fatal ASSERT_*. GoogleTest always runs
// TearDown after the test body, even on a fatal assertion failure, so cleanup is dependable.
class ApexFileSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        RETURN_CODE_TYPE rc = NO_ERROR;
        apexFileSystemInit(&rc);
        ASSERT_TRUE(rc == NO_ERROR || rc == NO_ACTION);
    }

    void TearDown() override {
        for (FILE_ID_TYPE id : openFileIds_) {
            RETURN_CODE_TYPE rc = NO_ERROR;
            FILE_ERRNO_TYPE err = 0;
            CLOSE_FILE(id, &rc, &err); // best-effort; closing an already-closed id is a safe no-op
        }
        for (const std::string& path : createdPaths_) {
            std::remove(path.c_str()); // REMOVE_FILE is still a stub, so clean up directly
        }
    }

    // Returns an absolute path under /tmp unique to the currently running test, so tests can't
    // collide with each other or with leftover files from a prior crashed run. Hashed rather than
    // built from the test's descriptive name: MAX_DIRECTORY_ENTRY_LENGTH caps a single path
    // component at 63 characters, which "<TestSuite>_<TestName><suffix>.txt" easily exceeds.
    static std::string uniquePath(const std::string& suffix = "") {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        const std::string testId =
            std::string(info->test_suite_name()) + "." + info->name() + suffix;
        const std::size_t hash = std::hash<std::string>{}(testId);

        char buf[32];
        std::snprintf(buf, sizeof(buf), "/tmp/apex653_%08zx.txt", hash & 0xFFFFFFFFu);
        return buf;
    }

    struct OpenOutcome {
        FILE_ID_TYPE fileId = -1;
        RETURN_CODE_TYPE returnCode = NO_ERROR;
        FILE_ERRNO_TYPE errnoValue = 0;
    };

    // Opens a new file at `path`, tracking it for cleanup regardless of the outcome.
    OpenOutcome openTracked(const std::string& path) {
        std::remove(path.c_str()); // leftover state from a prior crashed run shouldn't matter
        createdPaths_.push_back(path);

        FileName name = make_name(path);
        OpenOutcome outcome;
        OPEN_NEW_FILE(name.value, &outcome.fileId, &outcome.returnCode, &outcome.errnoValue);
        if (outcome.returnCode == NO_ERROR) {
            openFileIds_.push_back(outcome.fileId);
        }
        return outcome;
    }

private:
    std::vector<FILE_ID_TYPE> openFileIds_;
    std::vector<std::string> createdPaths_;
};

class OpenNewFileTest : public ApexFileSystemTest {};
class CloseFileTest : public ApexFileSystemTest {};
class ReadFileTest : public ApexFileSystemTest {};
class WriteFileTest : public ApexFileSystemTest {};
class SeekFileTest : public ApexFileSystemTest {};
class RemoveFileTest : public ApexFileSystemTest {};

// --- OPEN_NEW_FILE ------------------------------------------------------------------------------

TEST_F(OpenNewFileTest, SucceedsForANewAbsolutePath) {
    OpenOutcome result = openTracked(uniquePath());

    EXPECT_EQ(result.returnCode, NO_ERROR);
    EXPECT_GE(result.fileId, 0);
    EXPECT_EQ(result.errnoValue, 0);
}

TEST_F(OpenNewFileTest, ActuallyCreatesARegularFileOnDisk) {
    const std::string path = uniquePath();
    OpenOutcome result = openTracked(path);
    ASSERT_EQ(result.returnCode, NO_ERROR);

    struct stat st {};
    ASSERT_EQ(stat(path.c_str(), &st), 0);
    EXPECT_TRUE(S_ISREG(st.st_mode));
}

TEST_F(OpenNewFileTest, FailsWhenFileAlreadyExists) {
    const std::string path = uniquePath();
    OpenOutcome first = openTracked(path);
    ASSERT_EQ(first.returnCode, NO_ERROR);

    // Deliberately not using openTracked() again here: it calls std::remove() on entry, which
    // would delete the file the line above just created, defeating the point of this test.
    FileName name = make_name(path);
    FILE_ID_TYPE fileId = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    OPEN_NEW_FILE(name.value, &fileId, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EEXIST);
}

TEST_F(OpenNewFileTest, FailsWhenPathIsADirectory) {
    FileName name = make_name("/tmp");
    FILE_ID_TYPE fileId = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;

    OPEN_NEW_FILE(name.value, &fileId, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EISDIR);
}

TEST_F(OpenNewFileTest, FailsForARelativePath) {
    FileName name = make_name("relative.txt");
    FILE_ID_TYPE fileId = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;

    OPEN_NEW_FILE(name.value, &fileId, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EINVAL);
}

TEST_F(OpenNewFileTest, FailsWhenFileIdIsNull) {
    FileName name = make_name(uniquePath());
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;

    OPEN_NEW_FILE(name.value, nullptr, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    std::remove(name.value); // defensive; this path should reject before ever calling open()
}

TEST_F(OpenNewFileTest, FailsWhenErrnoIsNull) {
    FileName name = make_name(uniquePath());
    FILE_ID_TYPE fileId = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;

    OPEN_NEW_FILE(name.value, &fileId, &rc, nullptr);

    EXPECT_EQ(rc, INVALID_PARAM);
    std::remove(name.value); // defensive; this path should reject before ever calling open()
}

TEST_F(OpenNewFileTest, DoesNotCrashWhenReturnCodeIsNull) {
    FileName name = make_name(uniquePath());
    FILE_ID_TYPE fileId = -1;
    FILE_ERRNO_TYPE err = 0;

    OPEN_NEW_FILE(name.value, &fileId, nullptr, &err); // must not dereference the null RETURN_CODE
    SUCCEED();
    std::remove(name.value); // defensive; this path should reject before ever calling open()
}

TEST_F(OpenNewFileTest, FailsWithInvalidConfigWhenTableIsFull) {
    // MAX_OPEN_FILES is private to apexFileSystem.cpp, not part of the public ABI, so this is
    // kept in sync with it manually. Assumes no other test currently holds a slot open -- true
    // as long as every test's fixture correctly closes what it opens in TearDown.
    constexpr int kMaxOpenFiles = 128;

    for (int i = 0; i < kMaxOpenFiles; ++i) {
        OpenOutcome result = openTracked(uniquePath("_slot_" + std::to_string(i)));
        ASSERT_EQ(result.returnCode, NO_ERROR) << "failed to fill slot " << i;
    }

    FileName name = make_name(uniquePath("_overflow"));
    FILE_ID_TYPE fileId = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    OPEN_NEW_FILE(name.value, &fileId, &rc, &err);

    EXPECT_EQ(rc, INVALID_CONFIG);
    EXPECT_EQ(err, EMFILE);
}

// --- CLOSE_FILE ----------------------------------------------------------------------------------

TEST_F(CloseFileTest, SucceedsForAnOpenFile) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);

    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    CLOSE_FILE(opened.fileId, &rc, &err);

    EXPECT_EQ(rc, NO_ERROR);
    EXPECT_EQ(err, 0);
}

TEST_F(CloseFileTest, FailsForANegativeFileId) {
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    CLOSE_FILE(-1, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EBADF);
}

TEST_F(CloseFileTest, FailsForAnOutOfRangeFileId) {
    constexpr int kMaxOpenFiles = 128; // must match MAX_OPEN_FILES in apexFileSystem.cpp
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    CLOSE_FILE(kMaxOpenFiles, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EBADF);
}

TEST_F(CloseFileTest, FailsWhenCalledTwiceOnTheSameFileId) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);

    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    CLOSE_FILE(opened.fileId, &rc, &err);
    ASSERT_EQ(rc, NO_ERROR);

    rc = NO_ERROR;
    err = 0;
    CLOSE_FILE(opened.fileId, &rc, &err); // already closed

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EBADF);
}

TEST_F(CloseFileTest, FailsWhenErrnoIsNull) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);

    RETURN_CODE_TYPE rc = NO_ERROR;
    CLOSE_FILE(opened.fileId, &rc, nullptr);

    EXPECT_EQ(rc, INVALID_PARAM);
}

TEST_F(CloseFileTest, DoesNotCrashWhenReturnCodeIsNull) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);

    FILE_ERRNO_TYPE err = 0;
    CLOSE_FILE(opened.fileId, nullptr, &err); // must not dereference the null RETURN_CODE
    SUCCEED();
}

// --- READ_FILE -----------------------------------------------------------------------------------

TEST_F(ReadFileTest, ReadsBackBytesWrittenAfterSeekingToStart) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);

    APEX_BYTE payload[] = {'h', 'e', 'l', 'l', 'o'};
    RETURN_CODE_TYPE writeRc = NO_ERROR;
    FILE_ERRNO_TYPE writeErr = 0;
    WRITE_FILE(opened.fileId, payload, sizeof(payload), &writeRc, &writeErr);
    ASSERT_EQ(writeRc, NO_ERROR); // position is now past the end of what was just written

    FILE_SIZE_TYPE pos = -1;
    RETURN_CODE_TYPE seekRc = NO_ERROR;
    FILE_ERRNO_TYPE seekErr = 0;
    SEEK_FILE(opened.fileId, 0, FROM_FILE_START, &pos, &seekRc, &seekErr);
    ASSERT_EQ(seekRc, NO_ERROR);
    ASSERT_EQ(pos, 0);

    APEX_BYTE buffer[8] = {};
    MESSAGE_SIZE_TYPE outLength = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    READ_FILE(opened.fileId, buffer, sizeof(payload), &outLength, &rc, &err);

    EXPECT_EQ(rc, NO_ERROR);
    EXPECT_EQ(outLength, static_cast<MESSAGE_SIZE_TYPE>(sizeof(payload)));
    EXPECT_EQ(std::memcmp(buffer, payload, sizeof(payload)), 0);
}

TEST_F(ReadFileTest, SucceedsWithZeroBytesOnAnEmptyFile) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);

    APEX_BYTE buffer[16] = {};
    MESSAGE_SIZE_TYPE outLength = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    READ_FILE(opened.fileId, buffer, sizeof(buffer), &outLength, &rc, &err);

    EXPECT_EQ(rc, NO_ERROR);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(outLength, 0);
}

TEST_F(ReadFileTest, FailsForANegativeFileId) {
    APEX_BYTE buffer[16] = {};
    MESSAGE_SIZE_TYPE outLength = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    READ_FILE(-1, buffer, sizeof(buffer), &outLength, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EBADF);
}

TEST_F(ReadFileTest, FailsForAnOutOfRangeFileId) {
    constexpr int kMaxOpenFiles = 128; // must match MAX_OPEN_FILES in apexFileSystem.cpp
    APEX_BYTE buffer[16] = {};
    MESSAGE_SIZE_TYPE outLength = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    READ_FILE(kMaxOpenFiles, buffer, sizeof(buffer), &outLength, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EBADF);
}

TEST_F(ReadFileTest, FailsWhenFileIsNotInUse) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);
    RETURN_CODE_TYPE closeRc = NO_ERROR;
    FILE_ERRNO_TYPE closeErr = 0;
    CLOSE_FILE(opened.fileId, &closeRc, &closeErr);
    ASSERT_EQ(closeRc, NO_ERROR);

    APEX_BYTE buffer[16] = {};
    MESSAGE_SIZE_TYPE outLength = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    READ_FILE(opened.fileId, buffer, sizeof(buffer), &outLength, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EBADF);
}

TEST_F(ReadFileTest, FailsWhenOutLengthIsNull) {
    APEX_BYTE buffer[16] = {};
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    READ_FILE(0, buffer, sizeof(buffer), nullptr, &rc, &err); // slot 0 need not be open: this
                                                               // check runs before any table access
    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EINVAL);
}

TEST_F(ReadFileTest, FailsWhenMessageAddrIsNull) {
    MESSAGE_SIZE_TYPE outLength = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    READ_FILE(0, nullptr, 16, &outLength, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EINVAL);
}

TEST_F(ReadFileTest, FailsWhenInLengthIsNotPositive) {
    APEX_BYTE buffer[16] = {};
    MESSAGE_SIZE_TYPE outLength = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    READ_FILE(0, buffer, 0, &outLength, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EINVAL);
}

TEST_F(ReadFileTest, FailsWhenInLengthExceedsMaxAtomicity) {
    APEX_BYTE buffer[16] = {};
    MESSAGE_SIZE_TYPE outLength = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    READ_FILE(0, buffer, MAX_ATOMICITY + 1, &outLength, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EFBIG);
}

TEST_F(ReadFileTest, FailsWhenErrnoIsNull) {
    APEX_BYTE buffer[16] = {};
    MESSAGE_SIZE_TYPE outLength = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    READ_FILE(0, buffer, sizeof(buffer), &outLength, &rc, nullptr);

    EXPECT_EQ(rc, INVALID_PARAM);
}

TEST_F(ReadFileTest, DoesNotCrashWhenReturnCodeIsNull) {
    APEX_BYTE buffer[16] = {};
    MESSAGE_SIZE_TYPE outLength = -1;
    FILE_ERRNO_TYPE err = 0;
    READ_FILE(0, buffer, sizeof(buffer), &outLength, nullptr, &err); // must not deref null RETURN_CODE
    SUCCEED();
}

// --- WRITE_FILE ----------------------------------------------------------------------------------
//
// entry->mode is unconditionally READ_WRITE for every file OPEN_NEW_FILE creates -- there's no
// public way today to open a file in read-only mode -- so WRITE_FILE's "mode == READ" rejection
// branch is unreachable through this public-API test file. Revisit once a mode-selecting open
// exists.

TEST_F(WriteFileTest, ActuallyWritesBytesToDisk) {
    const std::string path = uniquePath();
    OpenOutcome opened = openTracked(path);
    ASSERT_EQ(opened.returnCode, NO_ERROR);

    APEX_BYTE payload[] = {'h', 'i', '!'};
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    WRITE_FILE(opened.fileId, payload, sizeof(payload), &rc, &err);
    ASSERT_EQ(rc, NO_ERROR);
    ASSERT_EQ(err, 0);

    RETURN_CODE_TYPE closeRc = NO_ERROR;
    FILE_ERRNO_TYPE closeErr = 0;
    CLOSE_FILE(opened.fileId, &closeRc, &closeErr);
    ASSERT_EQ(closeRc, NO_ERROR);

    FILE* raw = std::fopen(path.c_str(), "rb");
    ASSERT_NE(raw, nullptr);
    char buffer[8] = {};
    const std::size_t bytesRead = std::fread(buffer, 1, sizeof(payload), raw);
    std::fclose(raw);

    ASSERT_EQ(bytesRead, sizeof(payload));
    EXPECT_EQ(std::memcmp(buffer, payload, sizeof(payload)), 0);
}

TEST_F(WriteFileTest, FailsForANegativeFileId) {
    APEX_BYTE payload[] = {'x'};
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    WRITE_FILE(-1, payload, sizeof(payload), &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EBADF);
}

TEST_F(WriteFileTest, FailsForAnOutOfRangeFileId) {
    constexpr int kMaxOpenFiles = 128; // must match MAX_OPEN_FILES in apexFileSystem.cpp
    APEX_BYTE payload[] = {'x'};
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    WRITE_FILE(kMaxOpenFiles, payload, sizeof(payload), &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EBADF);
}

TEST_F(WriteFileTest, FailsWhenFileIsNotInUse) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);
    RETURN_CODE_TYPE closeRc = NO_ERROR;
    FILE_ERRNO_TYPE closeErr = 0;
    CLOSE_FILE(opened.fileId, &closeRc, &closeErr);
    ASSERT_EQ(closeRc, NO_ERROR);

    APEX_BYTE payload[] = {'x'};
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    WRITE_FILE(opened.fileId, payload, sizeof(payload), &rc, &err);

    EXPECT_EQ(rc, NOT_AVAILABLE);
    EXPECT_EQ(err, EACCES);
}

TEST_F(WriteFileTest, FailsWhenMessageAddrIsNull) {
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    WRITE_FILE(0, nullptr, 8, &rc, &err); // slot 0 need not be open: checked before table access

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EINVAL);
}

TEST_F(WriteFileTest, FailsWhenLengthIsNotPositive) {
    APEX_BYTE payload[] = {'x'};
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    WRITE_FILE(0, payload, 0, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EINVAL);
}

TEST_F(WriteFileTest, FailsWhenLengthExceedsMaxAtomicity) {
    APEX_BYTE payload[] = {'x'};
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    WRITE_FILE(0, payload, MAX_ATOMICITY + 1, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EFBIG);
}

TEST_F(WriteFileTest, FailsWhenErrnoIsNull) {
    APEX_BYTE payload[] = {'x'};
    RETURN_CODE_TYPE rc = NO_ERROR;
    WRITE_FILE(0, payload, sizeof(payload), &rc, nullptr);

    EXPECT_EQ(rc, INVALID_PARAM);
}

TEST_F(WriteFileTest, DoesNotCrashWhenReturnCodeIsNull) {
    APEX_BYTE payload[] = {'x'};
    FILE_ERRNO_TYPE err = 0;
    WRITE_FILE(0, payload, sizeof(payload), nullptr, &err); // must not deref null RETURN_CODE
    SUCCEED();
}

// --- SEEK_FILE -----------------------------------------------------------------------------------

TEST_F(SeekFileTest, SeeksFromFileStart) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);
    APEX_BYTE payload[] = {'A', 'B', 'C', 'D', 'E'};
    RETURN_CODE_TYPE writeRc = NO_ERROR;
    FILE_ERRNO_TYPE writeErr = 0;
    WRITE_FILE(opened.fileId, payload, sizeof(payload), &writeRc, &writeErr);
    ASSERT_EQ(writeRc, NO_ERROR);

    FILE_SIZE_TYPE pos = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    SEEK_FILE(opened.fileId, 2, FROM_FILE_START, &pos, &rc, &err);

    EXPECT_EQ(rc, NO_ERROR);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(pos, 2);
}

TEST_F(SeekFileTest, SeeksFromFileCurrent) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);
    APEX_BYTE payload[] = {'A', 'B', 'C', 'D', 'E'};
    RETURN_CODE_TYPE writeRc = NO_ERROR;
    FILE_ERRNO_TYPE writeErr = 0;
    WRITE_FILE(opened.fileId, payload, sizeof(payload), &writeRc, &writeErr);
    ASSERT_EQ(writeRc, NO_ERROR); // position is now 5

    FILE_SIZE_TYPE pos = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    SEEK_FILE(opened.fileId, -3, FROM_FILE_CURRENT, &pos, &rc, &err);

    EXPECT_EQ(rc, NO_ERROR);
    EXPECT_EQ(pos, 2);
}

TEST_F(SeekFileTest, SeeksFromFileEnd) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);
    APEX_BYTE payload[] = {'A', 'B', 'C', 'D', 'E'};
    RETURN_CODE_TYPE writeRc = NO_ERROR;
    FILE_ERRNO_TYPE writeErr = 0;
    WRITE_FILE(opened.fileId, payload, sizeof(payload), &writeRc, &writeErr);
    ASSERT_EQ(writeRc, NO_ERROR);

    FILE_SIZE_TYPE pos = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    SEEK_FILE(opened.fileId, 0, FROM_FILE_END, &pos, &rc, &err);

    EXPECT_EQ(rc, NO_ERROR);
    EXPECT_EQ(pos, 5);
}

TEST_F(SeekFileTest, FailsWhenTargetPositionIsBeforeStart) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);

    FILE_SIZE_TYPE pos = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    SEEK_FILE(opened.fileId, -1, FROM_FILE_START, &pos, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EINVAL);
}

TEST_F(SeekFileTest, FailsWhenTargetPositionExceedsFileSize) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR); // empty file, size 0

    FILE_SIZE_TYPE pos = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    SEEK_FILE(opened.fileId, 1, FROM_FILE_START, &pos, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EINVAL);
}

TEST_F(SeekFileTest, FailsForANegativeFileId) {
    FILE_SIZE_TYPE pos = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    SEEK_FILE(-1, 0, FROM_FILE_START, &pos, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EBADF);
}

TEST_F(SeekFileTest, FailsForAnOutOfRangeFileId) {
    constexpr int kMaxOpenFiles = 128; // must match MAX_OPEN_FILES in apexFileSystem.cpp
    FILE_SIZE_TYPE pos = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    SEEK_FILE(kMaxOpenFiles, 0, FROM_FILE_START, &pos, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EBADF);
}

TEST_F(SeekFileTest, FailsWhenFileIsNotInUse) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);
    RETURN_CODE_TYPE closeRc = NO_ERROR;
    FILE_ERRNO_TYPE closeErr = 0;
    CLOSE_FILE(opened.fileId, &closeRc, &closeErr);
    ASSERT_EQ(closeRc, NO_ERROR);

    FILE_SIZE_TYPE pos = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    SEEK_FILE(opened.fileId, 0, FROM_FILE_START, &pos, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EBADF);
}

TEST_F(SeekFileTest, FailsWhenPositionIsNull) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);

    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    SEEK_FILE(opened.fileId, 0, FROM_FILE_START, nullptr, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
}

TEST_F(SeekFileTest, FailsWhenWhenceIsInvalid) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);

    FILE_SIZE_TYPE pos = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    SEEK_FILE(opened.fileId, 0, static_cast<FILE_SEEK_TYPE>(3), &pos, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EINVAL);
}

TEST_F(SeekFileTest, FailsWhenErrnoIsNull) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);

    FILE_SIZE_TYPE pos = -1;
    RETURN_CODE_TYPE rc = NO_ERROR;
    SEEK_FILE(opened.fileId, 0, FROM_FILE_START, &pos, &rc, nullptr);

    EXPECT_EQ(rc, INVALID_PARAM);
}

TEST_F(SeekFileTest, DoesNotCrashWhenReturnCodeIsNull) {
    OpenOutcome opened = openTracked(uniquePath());
    ASSERT_EQ(opened.returnCode, NO_ERROR);

    FILE_SIZE_TYPE pos = -1;
    FILE_ERRNO_TYPE err = 0;
    SEEK_FILE(opened.fileId, 0, FROM_FILE_START, &pos, nullptr, &err); // must not deref null RETURN_CODE
    SUCCEED();
}

// --- REMOVE_FILE ---------------------------------------------------------------------------------

TEST_F(RemoveFileTest, SucceedsForAnExistingFile) {
    const std::string path = uniquePath();
    OpenOutcome opened = openTracked(path);
    ASSERT_EQ(opened.returnCode, NO_ERROR);

    RETURN_CODE_TYPE closeRc = NO_ERROR;
    FILE_ERRNO_TYPE closeErr = 0;
    CLOSE_FILE(opened.fileId, &closeRc, &closeErr);
    ASSERT_EQ(closeRc, NO_ERROR);

    FileName name = make_name(path);
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    REMOVE_FILE(name.value, &rc, &err);

    EXPECT_EQ(rc, NO_ERROR);
    EXPECT_EQ(err, 0);

    struct stat st {};
    EXPECT_NE(stat(path.c_str(), &st), 0); // file should genuinely be gone from disk
}

TEST_F(RemoveFileTest, FailsForANonexistentPath) {
    FileName name = make_name(uniquePath("_never_created"));
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    REMOVE_FILE(name.value, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, ENOENT);
}

TEST_F(RemoveFileTest, FailsWhenPathIsADirectory) {
    FileName name = make_name("/tmp");
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    REMOVE_FILE(name.value, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EPERM);
}

TEST_F(RemoveFileTest, FailsForARelativePath) {
    FileName name = make_name("relative.txt");
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    REMOVE_FILE(name.value, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EINVAL);
}

TEST_F(RemoveFileTest, FailsWhenFileNameIsNull) {
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    REMOVE_FILE(nullptr, &rc, &err);

    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EINVAL);
}

TEST_F(RemoveFileTest, FailsWhenErrnoIsNull) {
    FileName name = make_name(uniquePath());
    RETURN_CODE_TYPE rc = NO_ERROR;
    REMOVE_FILE(name.value, &rc, nullptr);

    EXPECT_EQ(rc, INVALID_PARAM);
}

TEST_F(RemoveFileTest, DoesNotCrashWhenReturnCodeIsNull) {
    FileName name = make_name(uniquePath());
    FILE_ERRNO_TYPE err = 0;
    REMOVE_FILE(name.value, nullptr, &err); // must not dereference the null RETURN_CODE
    SUCCEED();
}
