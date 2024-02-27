//
// Created by zhugc on 2024-02-26.
//

#ifndef MEDIAPIPE_MMN_ANDROID_LOGGING_H
#define MEDIAPIPE_MMN_ANDROID_LOGGING_H

#include <android/log.h>

#ifndef LOG_TAG
#define LOG_TAG "MediaPipe-MNN-Android"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG ,__VA_ARGS__) // 定义LOGD类型
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG ,__VA_ARGS__) // 定义LOGI类型
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,LOG_TAG ,__VA_ARGS__) // 定义LOGW类型
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG ,__VA_ARGS__) // 定义LOGE类型
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL,LOG_TAG ,__VA_ARGS__) // 定义LOGF类型
#endif

#endif //MEDIAPIPE_MMN_ANDROID_LOGGING_H
