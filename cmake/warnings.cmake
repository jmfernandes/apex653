# Shared compiler warning/diagnostic flags, applied via apex653_set_warnings(<target>).

function(apex653_set_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        )
        if(CMAKE_CXX_FLAGS MATCHES "-fcontracts")
            # GCC's -fcontracts (see docs/DESIGN.md) causes it to misreport any parameter not
            # named in that function's own [[pre:]]/[[post:]] as unused, even when the body
            # clearly uses it. This must come after -Wextra above (which implies
            # -Wunused-parameter) in the same call, since flags are order-sensitive and a later
            # -Wextra would otherwise re-enable what this suppresses.
            target_compile_options(${target} PRIVATE -Wno-unused-parameter)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /W4)
    endif()
endfunction()
