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
