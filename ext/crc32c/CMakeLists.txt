set(crc32c_src_root ${PROJECT_SOURCE_DIR}/ext/crc32c)
message(STATUS "source dir: ${crc32c_src_root}")
include_directories(${crc32c_src_root}/include)
include_directories(${crc32c_src_root}/config/Linux-x86_64)

add_compile_options("-msse4.2")

file(GLOB SRC_ROOT_LIST
        ${crc32c_src_root}/src/*.cc
        ${crc32c_src_root}/src/*.h)

list(APPEND SRC_ROOT_LIST "${crc32c_src_root}/config/Linux-x86_64/crc32c/crc32c_config.h")

add_library(crc32c STATIC ${SRC_ROOT_LIST})