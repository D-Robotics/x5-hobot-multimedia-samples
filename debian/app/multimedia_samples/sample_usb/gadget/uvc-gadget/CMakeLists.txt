cmake_minimum_required(VERSION 3.0)
project(uvc-gadget)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_SYSTEM_NAME Linux)

set(BUILD_ENV_ROOT "$ENV{TOOLCHAIN_PATH}")

# Specify the cross compiler
set(CMAKE_C_COMPILER   ${BUILD_ENV_ROOT}/bin/aarch64-none-linux-gnu-gcc)

# Where is the target environment
# set(CMAKE_FIND_ROOT_PATH ${BUILD_ENV_ROOT}/target ${BUILD_ENV_ROOT}/host)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -Werror")

include_directories(${CMAKE_BINARY_DIR}/generated)

add_subdirectory(lib)
add_subdirectory(src)

# Generate config.h
configure_file(${CMAKE_SOURCE_DIR}/include/config.h.in
	       ${CMAKE_BINARY_DIR}/generated/config.h)
