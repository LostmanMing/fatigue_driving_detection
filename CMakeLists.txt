cmake_minimum_required(VERSION 3.18)
project(fatigue_driving_detection)

set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(TARGET_SOC "rk356x")
set(CMAKE_C_COMPILER "aarch64")
set(LIB_ARCH "aarch64")
# rknn api
set(RKNN_API_PATH ${CMAKE_SOURCE_DIR}/runtime/RK356X/${CMAKE_SYSTEM_NAME}/librknn_api)
#set(RKNN_RT_LIB /usr/lib/librknnrt.so)
set(RKNN_RT_LIB ${RKNN_API_PATH}/${LIB_ARCH}/librknnrt.so)

#rga
#set(RGA_PATH ${CMAKE_SOURCE_DIR}/RK356X/include/librga)
set(RGA_PATH ${CMAKE_SOURCE_DIR}/librga/include)
set(RGA_LIB ${CMAKE_SOURCE_DIR}/librga/libs/Linux/gcc-aarch64/librga.so)
message(11)


# opencv
#set(OpenCV_DIR /usr/local/arm/share/OpenCV)
find_package(OpenCV REQUIRED)
message(STATUS "Opnecv ;ibrary status: ")
message(STATUS "> version: ${OpenCV_VERSION} ")
message(STATUS "libraries: ${OpenCV_LIBS} ")
message(STATUS "> include: ${OpenCV_INCLUDE_DIRS}  ")
find_package(CURL REQUIRED)

include(FindPkgConfig)
pkg_check_modules(GST     REQUIRED gstreamer-1.0)
pkg_check_modules(GSTAPP  REQUIRED gstreamer-app-1.0)
pkg_check_modules(GLIB    REQUIRED glib-2.0)
pkg_check_modules(GFLAGS  REQUIRED gflags)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
pkg_check_modules(CURL REQUIRED libcurl)


message(STATUS "GST:   ${GST_INCLUDE_DIRS},${GST_LIBRARY_DIRS},${GST_LIBRARIES}")
message(STATUS "GSTAPP:${GSTAPP_INCLUDE_DIRS},${GSTAPP_LIBRARY_DIRS},${GSTAPP_LIBRARIES}")
message(STATUS "GLIB:  ${GLIB_INCLUDE_DIRS},${GLIB_LIBRARY_DIRS},${GLIB_LIBRARIES}")
message(STATUS "JSON:  ${JSON_INCLUDE_DIRS},${JSON_LIBRARY_DIRS},${JSON_LIBRARIES}")
message(STATUS "GFLAGS:${GFLAGS_INCLUDE_DIRS},${GFLAGS_LIBRARY_DIRS},${GFLAGS_LIBRARIES}")


message(STATUS ${RKNN_API_PATH}/${LIB_ARCH}/librknnrt.so)
include_directories(${RKNN_API_PATH}/include)
# include_directories(${CMAKE_SOURCE_DIR}/3rdparty)
include_directories( ${RGA_PATH})
include_directories( ${CMAKE_SOURCE_DIR}/include)
#include_directories(/usr/local/arm/include)
include_directories(/usr/include/aarch64-linux-gnu)

include_directories(
        ${PROJECT_SOURCE_DIR}/inc
        ${GST_INCLUDE_DIRS}
        ${GSTAPP_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS}
        ${GFLAGS_INCLUDE_DIRS}
        ${JSONCPP_INCLUDE_DIRS}
        ${OpenCV_INCLUDE_DIRS}
)
message(  ${OpenCV_LIBS})

link_directories(
        ${GST_LIBRARY_DIRS}
        ${GSTAPP_LIBRARY_DIRS}
        ${GLIB_LIBRARY_DIRS}
        ${GFLAGS_LIBRARY_DIRS}
        ${JSONCPP_LIBRARY_DIRS}
        ${OpenCV_LIBS}
)


add_executable(fatigue_driving_detection
        src/main.cpp
        src/StreamMgr.cpp
        src/VideoMgr.cpp
        include/SeqProcesser.hpp
        src/Yolov6Base.cpp
        src/Yolov6EM.cpp
        src/Yolov6PP.cpp
        src/postprocess.cpp
        )
message(${CURL_INCLUDE_DIRS})
target_include_directories(fatigue_driving_detection PRIVATE ${CURL_INCLUDE_DIRS})

target_link_libraries(fatigue_driving_detection
        ${GST_LIBRARIES}
        ${GSTAPP_LIBRARIES}
        ${GLIB_LIBRARIES}
        ${GFLAGS_LIBRARIES}
        ${JSONCPP_LIBRARIES}
        ${RKNN_RT_LIB}
        ${OpenCV_LIBS}
        pthread
        gstpbutils-1.0
        CURL::libcurl
        )

add_executable(testnpu
        src/test_npu.cpp
        src/Yolov6Base.cpp
        src/Yolov6EM.cpp
        src/Yolov6PP.cpp
        src/postprocess.cpp
        )
target_link_libraries(testnpu
        ${RKNN_RT_LIB}
        ${RGA_LIB}
        ${OpenCV_LIBS}
        -lpthread
        )