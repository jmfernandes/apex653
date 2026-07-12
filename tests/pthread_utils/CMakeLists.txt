add_executable(apex653_pthread_utils_tests
    apex_pthread_utils_test.cpp
)

target_link_libraries(apex653_pthread_utils_tests PRIVATE
    apex653::apex653
    GTest::gtest_main
)

apex653_set_warnings(apex653_pthread_utils_tests)

include(GoogleTest)
gtest_discover_tests(apex653_pthread_utils_tests)
