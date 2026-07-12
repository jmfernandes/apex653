# apex_fs_internal_test.cpp #includes apexFileSystem.cpp directly to reach its anonymous-namespace
# helper functions (see that file for why) - so this target must NOT also link apex653::apex653,
# which would define OPEN_NEW_FILE, CLOSE_FILE, etc. a second time and fail to link.
find_package(Threads REQUIRED)

add_executable(apex653_filesystem_internal_tests
    apex_fs_internal_test.cpp
)

target_include_directories(apex653_filesystem_internal_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/components/apex653/include/h/apex
)

target_link_libraries(apex653_filesystem_internal_tests PRIVATE
    Threads::Threads
    GTest::gtest_main
)

apex653_set_warnings(apex653_filesystem_internal_tests)

include(GoogleTest)
gtest_discover_tests(apex653_filesystem_internal_tests)

# apex_fs_test.cpp exercises OPEN_NEW_FILE/CLOSE_FILE, the public extern "C" API, so unlike the
# internal test above it links apex653::apex653 normally instead of #including the .cpp.
add_executable(apex653_filesystem_tests
    apex_fs_test.cpp
)

target_link_libraries(apex653_filesystem_tests PRIVATE
    apex653::apex653
    GTest::gtest_main
)

apex653_set_warnings(apex653_filesystem_tests)

gtest_discover_tests(apex653_filesystem_tests)
