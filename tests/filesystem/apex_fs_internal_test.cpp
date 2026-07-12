// This test exercises apexFileSystem.cpp's internal (anonymous-namespace) helper functions --
// validateFilename, checkNullParameters, checkFileInit, getCurrentCompositeTime, and the
// TableEntry/findFreeSlot template -- which have internal linkage by design (see
// docs/DESIGN.md, "Internal linkage for file-local helpers") and are therefore invisible to any
// other translation unit. #including the .cpp directly makes this file the same translation
// unit as production code for test purposes, without weakening that linkage guarantee in the
// real library build: this test binary does not link apex653::apex653, and libapex653.a itself
// is completely unaffected by anything below.
//
// OPEN_NEW_FILE and CLOSE_FILE are intentionally not tested here yet.
#include "../../components/apex653/src/apexFileSystem.cpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <iterator>
#include <string>
#include <thread>

namespace {

// FILE_NAME_TYPE is a raw array, and functions cannot return array types, so this wraps it in a
// struct purely to make literal names usable as rvalues at each call site below.
struct FileName {
    FILE_NAME_TYPE value{};
};

FileName make_name(const char* name) {
    FileName result;
    std::strncpy(result.value, name, sizeof(result.value) - 1);
    return result;
}

} // namespace

// --- validateFilename ------------------------------------------------------------------------

TEST(ValidateFilename, AcceptsAWellFormedAbsolutePath) {
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    bool isNominal = true;

    validateFilename(make_name("/tmp/round_trip.txt").value, &rc, &err, isNominal);

    EXPECT_TRUE(isNominal);
    EXPECT_EQ(rc, NO_ERROR); // untouched on success
    EXPECT_EQ(err, 0);
}

TEST(ValidateFilename, RejectsNullFileName) {
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    bool isNominal = true;

    validateFilename(nullptr, &rc, &err, isNominal);

    EXPECT_FALSE(isNominal);
    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EINVAL);
}

TEST(ValidateFilename, RejectsEmptyFileName) {
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    bool isNominal = true;

    validateFilename(make_name("").value, &rc, &err, isNominal);

    EXPECT_FALSE(isNominal);
    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EINVAL);
}

TEST(ValidateFilename, RejectsRelativePath) {
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    bool isNominal = true;

    validateFilename(make_name("relative/path.txt").value, &rc, &err, isNominal);

    EXPECT_FALSE(isNominal);
    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, EINVAL);
}

TEST(ValidateFilename, RejectsNameAtOrOverMaxFileNameLength) {
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    bool isNominal = true;

    std::string tooLong = "/" + std::string(MAX_FILE_NAME_LENGTH, 'a');
    FILE_NAME_TYPE name{};
    std::strncpy(name, tooLong.c_str(), sizeof(name) - 1);

    validateFilename(name, &rc, &err, isNominal);

    EXPECT_FALSE(isNominal);
    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, ENAMETOOLONG);
}

TEST(ValidateFilename, RejectsDirectoryComponentAtOrOverMaxLength) {
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    bool isNominal = true;

    std::string longComponent(MAX_DIRECTORY_ENTRY_LENGTH, 'b');
    FILE_NAME_TYPE name{};
    std::strncpy(name, ("/" + longComponent).c_str(), sizeof(name) - 1);

    validateFilename(name, &rc, &err, isNominal);

    EXPECT_FALSE(isNominal);
    EXPECT_EQ(rc, INVALID_PARAM);
    EXPECT_EQ(err, ENAMETOOLONG);
}

TEST(ValidateFilename, ToleratesRepeatedSlashes) {
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    bool isNominal = true;

    validateFilename(make_name("//tmp//round_trip.txt").value, &rc, &err, isNominal);

    EXPECT_TRUE(isNominal);
}

TEST(ValidateFilename, IsANoOpWhenAlreadyNotNominal) {
    RETURN_CODE_TYPE rc = INVALID_CONFIG; // arbitrary sentinel, must stay untouched
    FILE_ERRNO_TYPE err = 99;             // arbitrary sentinel, must stay untouched
    bool isNominal = false;

    validateFilename(nullptr, &rc, &err, isNominal); // would otherwise set INVALID_PARAM/EINVAL

    EXPECT_FALSE(isNominal);
    EXPECT_EQ(rc, INVALID_CONFIG);
    EXPECT_EQ(err, 99);
}

// --- checkNullParameters -----------------------------------------------------------------------

TEST(CheckNullParameters, AcceptsNonNullReturnCodeAndErrno) {
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    bool isNominal = true;

    checkNullParameters(&rc, &err, isNominal);

    EXPECT_TRUE(isNominal);
}

TEST(CheckNullParameters, RejectsNullReturnCode) {
    FILE_ERRNO_TYPE err = 0;
    bool isNominal = true;

    checkNullParameters(nullptr, &err, isNominal); // must not dereference the null RETURN_CODE

    EXPECT_FALSE(isNominal);
}

TEST(CheckNullParameters, RejectsNullErrnoWhenReturnCodeIsValid) {
    RETURN_CODE_TYPE rc = NO_ERROR;
    bool isNominal = true;

    checkNullParameters(&rc, nullptr, isNominal);

    EXPECT_FALSE(isNominal);
    EXPECT_EQ(rc, INVALID_PARAM);
}

// --- checkFileInit -----------------------------------------------------------------------------

TEST(CheckFileInit, RejectsCallsBeforeInitThenAcceptsAfter) {
    // g_fileTableInit is process-global state, so this test deliberately does both halves in one
    // TEST() body rather than relying on GoogleTest's default (but not guaranteed-stable) source
    // order across separate tests.
    RETURN_CODE_TYPE rc = NO_ERROR;
    FILE_ERRNO_TYPE err = 0;
    bool isNominal = true;

    if (!g_fileTableInit.load()) {
        checkFileInit(&rc, &err, isNominal);
        EXPECT_FALSE(isNominal);
        EXPECT_EQ(rc, INVALID_MODE);
        EXPECT_EQ(err, EACCES);
    }

    RETURN_CODE_TYPE initRc = NO_ERROR;
    apexFileSystemInit(&initRc);
    ASSERT_TRUE(g_fileTableInit.load());

    rc = NO_ERROR;
    err = 0;
    isNominal = true;
    checkFileInit(&rc, &err, isNominal);

    EXPECT_TRUE(isNominal);
    EXPECT_EQ(rc, NO_ERROR);
    EXPECT_EQ(err, 0);
}

// --- getCurrentCompositeTime -------------------------------------------------------------------

TEST(GetCurrentCompositeTime, FillsInAPlausibleWallClockTime) {
    COMPOSITE_TIME_TYPE ct{};

    getCurrentCompositeTime(&ct);

    EXPECT_EQ(ct.TM_IS_SET, SET);
    EXPECT_GE(ct.TM_SEC, 0);
    EXPECT_LE(ct.TM_SEC, 60); // 60 to allow for a leap second
    EXPECT_GE(ct.TM_MIN, 0);
    EXPECT_LE(ct.TM_MIN, 59);
    EXPECT_GE(ct.TM_HOUR, 0);
    EXPECT_LE(ct.TM_HOUR, 23);
    EXPECT_GE(ct.TM_MON, 0);
    EXPECT_LE(ct.TM_MON, 11);
    EXPECT_GT(ct.TM_YEAR, 100); // years since 1900; this code did not ship before 2000
}

// --- TableEntry concept + findFreeSlot -----------------------------------------------------------

static_assert(TableEntry<InternalFileEntry>);
static_assert(TableEntry<InternalDirEntry>);
static_assert(!TableEntry<int>);

TEST(FindFreeSlot, FindsTheFirstFreeSlotInAnEmptyTable) {
    InternalFileEntry table[4]{};

    auto found = findFreeSlot(table, std::size(table));

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->index, 0u);
    EXPECT_TRUE(found->lock.owns_lock());
}

TEST(FindFreeSlot, SkipsSlotsAlreadyMarkedInUse) {
    InternalFileEntry table[4]{};
    table[0].inUse = 1;
    table[1].inUse = 1;

    auto found = findFreeSlot(table, std::size(table));

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->index, 2u);
}

TEST(FindFreeSlot, ReturnsNulloptWhenEveryEntryIsInUse) {
    InternalFileEntry table[4]{};
    for (auto& entry : table) {
        entry.inUse = 1;
    }

    auto found = findFreeSlot(table, std::size(table));

    EXPECT_FALSE(found.has_value());
}

TEST(FindFreeSlot, SkipsAContendedSlotEvenIfNotInUse) {
    InternalFileEntry table[4]{};

    std::atomic<bool> holderReady{false};
    std::thread holder([&table, &holderReady] {
        std::unique_lock<PthreadPrioInheritMutex> lock(table[0].mutex); // blocking lock
        holderReady = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });

    while (!holderReady.load()) {
        std::this_thread::yield();
    }

    auto found = findFreeSlot(table, std::size(table));

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->index, 1u); // slot 0 is free (inUse == 0) but contended, so it's skipped

    holder.join();
}
