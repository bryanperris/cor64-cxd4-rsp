cmake_minimum_required(VERSION 3.1)

project(rsp C)

# Prevents the shared library being called librsp
set(CMAKE_SHARED_LIBRARY_PREFIX "")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message("Using 64-bit compiler options")

    set(CMAKE_C_FLAGS "\
        -masm=intel \
        -fPIC \
        -DPLUGIN_API_VERSION=0x0101 \
        -DARCH_MIN_SSE2 \
        -march=native \
        -mstackrealign \
        -Wall \
        -pedantic \
        -Wall -Wshadow -Wredundant-decls -Wextra -Wcast-align -Wcast-qual \
        -Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op \
        -Wmissing-include-dirs -Wstrict-overflow=1 -Wundef -Wno-unused \
        -Wno-variadic-macros -Wno-parentheses -fdiagnostics-show-option"
    )
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message("Using 32-bit compiler options")

    set(CMAKE_C_FLAGS "\
        -masm=intel \
        -DPLUGIN_API_VERSION=0x0101 \
        -DARCH_MIN_SSE2 \
        -march=native \
        -mstackrealign \
        -Wall \
        -pedantic \
        -Wall -Wshadow -Wredundant-decls -Wextra -Wcast-align -Wcast-qual \
        -Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op \
        -Wmissing-include-dirs -Wstrict-overflow=1 -Wundef -Wno-unused \
        -Wno-variadic-macros -Wno-parentheses -fdiagnostics-show-option"
    )
endif()

add_library(
    ${CMAKE_PROJECT_NAME} SHARED
    module.c
    su.c
    vu/vu.c
    vu/multiply.c
    vu/add.c
    vu/select.c
    vu/logical.c
    vu/divide.c
)