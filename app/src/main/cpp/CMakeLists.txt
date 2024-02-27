cmake_minimum_required(VERSION 3.10)
project(MediaPipeMNN)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp")
if (DEFINED ANDROID_NDK_MAJOR AND ${ANDROID_NDK_MAJOR} GREATER 20)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-openmp")
endif ()
## opencv-mobile
#set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/opencv/sdk/native/jni)
#find_package(OpenCV REQUIRED core imgproc)

aux_source_directory(. SRC_LIST)

add_library( # Sets the name of the library.
        MediaPipeMNN

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        ${SRC_LIST})


#find_library(log-lib log)
#find_library(camera-lib camera2ndk)
#find_library(android-lib android)
#find_library(jnigraphics-lib jnigraphics)


# opencv
#include_directories(
#        ${CMAKE_SOURCE_DIR}/opencv/include/
#)
#add_library(libopencv_java4 STATIC IMPORTED)
#set_target_properties(
#        libopencv_java4
#        PROPERTIES IMPORTED_LOCATION
#        ${CMAKE_SOURCE_DIR}/opencv/${ANDROID_ABI}/libopencv_java4.so
#)
set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/opencv-mobile-4.9.0-android/sdk/native/jni)
find_package(OpenCV REQUIRED)

#target_link_libraries(your_jni_target )

# ncnn
set(ncnn_DIR ${CMAKE_SOURCE_DIR}/ncnn-20240102-android-vulkan-shared/${ANDROID_ABI}/lib/cmake/ncnn)
find_package(ncnn REQUIRED)

# mnn
include_directories(${CMAKE_SOURCE_DIR}/mnn_2.3.1_android_armv7_armv8_cpu_opencl_vulkan/include)
#include_directories(${CMAKE_SOURCE_DIR}/mnn/include/MNN/expr/)
#include_directories(${CMAKE_SOURCE_DIR}/mnn/include/MNN/plugin/)
add_library(libMNN STATIC IMPORTED)
add_library(libMNN_CL STATIC IMPORTED)
add_library(libMNN_Express STATIC IMPORTED)
add_library(libMNN_Vulkan STATIC IMPORTED)
add_library(libmnncore STATIC IMPORTED)
set_target_properties(
        libMNN
        libMNN_CL
        libMNN_Express
        libMNN_Vulkan
        libmnncore
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/mnn/${ANDROID_ABI}/libMNN.so
        ${CMAKE_SOURCE_DIR}/mnn/${ANDROID_ABI}/libMNN_CL.so
        ${CMAKE_SOURCE_DIR}/mnn/${ANDROID_ABI}/libMNN_Express.so
        ${CMAKE_SOURCE_DIR}/mnn/${ANDROID_ABI}/libMNN_Vulkan.so
        ${CMAKE_SOURCE_DIR}/mnn/${ANDROID_ABI}/libmnncore.so
)


# tnn
#set(TNN_DIR ${CMAKE_SOURCE_DIR}/tnn-v0.3.0-android)
#include_directories(${TNN_DIR}/include/)
#include_directories(${TNN_DIR}/include/tnn/)
#add_library(TNN SHARED IMPORTED)
#set_target_properties(TNN PROPERTIES IMPORTED_LOCATION ${TNN_DIR}/${ANDROID_ABI}/libTNN.so)

#add_library(GazeTracker SHARED jni_interface.cpp NanoDet.cpp NanoDet.h)

#target_link_libraries(gazeGazeTracker MNN ${OpenCV_LIBS} libopencv_java4 camera2ndk mediandk)

target_link_libraries( # Specifies the target library.
        MediaPipeMNN
        ncnn
        camera2ndk
        mediandk
        android
        jnigraphics
        log
        ${OpenCV_LIBS}
        libMNN libMNN_CL libMNN_Express libMNN_Vulkan libmnncore)