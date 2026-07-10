add_library(apex653 STATIC
    src/apexFileSystem.cpp
)
add_library(apex653::apex653 ALIAS apex653)

target_include_directories(apex653 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include/h/apex
)

apex653_set_warnings(apex653)
