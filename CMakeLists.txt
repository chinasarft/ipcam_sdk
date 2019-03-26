cmake_minimum_required (VERSION 2.8)

project(rtmpsdk)

add_definitions("-Wall -g")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/rtmpdump/librtmp")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/fdk-aac/libAACenc/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/fdk-aac/libSYS/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/rtmpsdk")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib")

add_library(rtmp_sdk STATIC
    rtmpsdk/aac_encoder.h
    rtmpsdk/g711.h
    rtmpsdk/rtmp_publish.h
    rtmpsdk/aac_encoder.c
    rtmpsdk/g711.c
    rtmpsdk/main.c
    rtmpsdk/rtmp_publish.c
)

add_executable(rtmptest
    test/adts.h
    test/adts.c
    test/test.c
    test/flag.c
    test/flag.h
    test/fileinput.h
    test/fileinput.c
)

target_link_libraries(rtmptest rtmp_sdk rtmp fdk-aac pthread)
