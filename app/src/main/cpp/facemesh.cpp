//
// Created by zhugc on 2024-02-27.
//
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>
#include <android/log.h>

#include <jni.h>
#include <string>
#include <vector>
#include <codecvt>
#include <fstream>

#include "ndkcamera.h"
#include "ncnn-20240102-android-vulkan-shared/arm64-v8a/include/ncnn/benchmark.h"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "FaceDetector.h"
#include "FaceLandmarkDetector.h"


#if __ARM_NEON

#include <arm_neon.h>

#endif // __ARM_NEON

static mirror::FaceDetector *detector;
static mirror::FaceLandmarkDetector *landmarker;

static int draw_fps(cv::Mat &rgb) {
    // resolve moving average
    float avg_fps = 0.f;
    {
        static double t0 = 0.f;
        static float fps_history[10] = {0.f};

        double t1 = ncnn::get_current_time();
        if (t0 == 0.f) {
            t0 = t1;
            return 0;
        }

        float fps = 1000.f / (t1 - t0);
        t0 = t1;

        for (int i = 9; i >= 1; i--) {
            fps_history[i] = fps_history[i - 1];
        }
        fps_history[0] = fps;

        if (fps_history[9] == 0.f) {
            return 0;
        }

        for (int i = 0; i < 10; i++) {
            avg_fps += fps_history[i];
        }
        avg_fps /= 10.f;
    }

    char text[32];
    sprintf(text, "FPS=%.2f", avg_fps);

    int baseLine = 0;
    cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

    int y = 0;
    int x = rgb.cols - label_size.width;

    cv::rectangle(rgb, cv::Rect(cv::Point(x, y),
                                cv::Size(label_size.width, label_size.height + baseLine)),
                  cv::Scalar(255, 255, 255), -1);

    cv::putText(rgb, text, cv::Point(x, y + label_size.height),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));

    return 0;
}

static std::string UTF16StringToUTF8String(const char16_t *chars, size_t len) {
    std::u16string u16_string(chars, len);
    return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}
            .to_bytes(u16_string);
}

std::string JavaStringToString(JNIEnv *env, jstring str) {
    if (env == nullptr || str == nullptr) {
        return "";
    }
    const jchar *chars = env->GetStringChars(str, NULL);
    if (chars == nullptr) {
        return "";
    }
    std::string u8_string = UTF16StringToUTF8String(
            reinterpret_cast<const char16_t *>(chars), env->GetStringLength(str));
    env->ReleaseStringChars(str, chars);
    return u8_string;
}

std::string fdLoadFile(std::string path) {
    std::ifstream file(path, std::ios::in);
    if (file.is_open()) {
        file.seekg(0, file.end);
        int size = file.tellg();
        char *content = new char[size];
        file.seekg(0, file.beg);
        file.read(content, size);
        std::string fileContent;
        fileContent.assign(content, size);
        delete[] content;
        file.close();
        return fileContent;
    } else {
        return "";
    }
}

class MyNdkCamera : public NdkCamera {
public:
    MyNdkCamera();

    ~MyNdkCamera();

    void set_window(ANativeWindow *win);

    virtual void on_image(const cv::Mat &rgb) const;

private:
    ANativeWindow *win;
};

MyNdkCamera::MyNdkCamera() {
    win = 0;
}

MyNdkCamera::~MyNdkCamera() {
    if (win) {
        ANativeWindow_release(win);
    }
}

void MyNdkCamera::set_window(ANativeWindow *_win) {
    if (win) {
        ANativeWindow_release(win);
    }

    win = _win;
    ANativeWindow_acquire(win);
}

void MyNdkCamera::on_image(const cv::Mat &rgb) const {
    // face mesh
    mirror::ImageHead in;
    in.data = rgb.data;
    in.height = rgb.rows;
    in.width = rgb.cols;
    in.width_step = rgb.step[0];
    in.pixel_format = mirror::PixelFormat::RGB;

    auto start = std::chrono::high_resolution_clock::now(); // 记录开始时间
    mirror::RotateType type = mirror::RotateType::CLOCKWISE_ROTATE_0;
    std::vector<mirror::ObjectInfo> objects;
    detector->Detect(in, type, objects);

    landmarker->Detect(in, type, objects);
    auto end = std::chrono::high_resolution_clock::now(); // 记录结束时间
    std::chrono::duration<double> elapsed = end - start; // 计算时间差
    __android_log_print(ANDROID_LOG_INFO, "NdkCamera", "rotate执行时间: %.2f", elapsed.count() * 1000);


    for (const auto &object: objects) {
        cv::rectangle(rgb, cv::Point2f(object.rect.left, object.rect.top),
                      cv::Point2f(object.rect.right, object.rect.bottom),
                      cv::Scalar(255, 0, 255), 2);
        for (int i = 0; i < object.landmarks.size(); ++i) {
            cv::Point pt = cv::Point((int) object.landmarks[i].x, (int) object.landmarks[i].y);
            cv::circle(rgb, pt, 2, cv::Scalar(255, 255, 0));
            cv::putText(rgb, std::to_string(i), pt, 1, 1.0, cv::Scalar(255, 0, 255));
        }
        LOGI("%lu",  object.landmarks3d.size());
        for (int i = 0; i < object.landmarks3d.size(); ++i) {
            cv::Point pt = cv::Point((int) object.landmarks3d[i].x, (int) object.landmarks3d[i].y);
            cv::circle(rgb, pt, 2, cv::Scalar(255, 255, 0));
        }
        cv::putText(rgb, std::to_string(object.score),
                    cv::Point2f(object.rect.left, object.rect.top + 20), 1, 1.0,
                    cv::Scalar(0, 255, 255));
    }


    // render to window
    int target_width = ANativeWindow_getWidth(win);
    int target_height = ANativeWindow_getHeight(win);
//     int target_format = ANativeWindow_getFormat(win);

    // crop to target aspect ratio
    cv::Mat rgb_roi;
    {
        int w = rgb.cols;
        int h = rgb.rows;

        cv::Rect roi;

        if (target_width * h > target_height * w) {
            roi.width = w;
            roi.height = w * target_height / target_width;
            roi.x = 0;
            roi.y = (h - roi.height) / 2;
        } else {
            roi.height = h;
            roi.width = h * target_width / target_height;
            roi.x = (w - roi.width) / 2;
            roi.y = 0;
        }

        rgb_roi = rgb(roi).clone();
    }

    // Live Slove
    {


        draw_fps(rgb_roi);
    }


    ANativeWindow_setBuffersGeometry(win, rgb_roi.cols, rgb_roi.rows,
                                     AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM);

    ANativeWindow_Buffer buf;
    ANativeWindow_lock(win, &buf, NULL);

    //__android_log_print(ANDROID_LOG_WARN, "ncnn", "on_image %d %d -> %d %d -> %d %d -> %d %d %d", rgb.cols, rgb.rows, target_width, target_height, rgb_roi.cols, rgb_roi.rows, buf.width, buf.height, buf.stride);

    // scale to target size
    if (buf.format == AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM ||
        buf.format == AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM) {
        for (int y = 0; y < rgb_roi.rows; y++) {
            const unsigned char *ptr = rgb_roi.ptr<const unsigned char>(y);
            unsigned char *outptr = (unsigned char *) buf.bits + buf.stride * 4 * y;

            int x = 0;
#if __ARM_NEON
            for (; x + 7 < rgb_roi.cols; x += 8) {
                uint8x8x3_t _rgb = vld3_u8(ptr);
                uint8x8x4_t _rgba;
                _rgba.val[0] = _rgb.val[0];
                _rgba.val[1] = _rgb.val[1];
                _rgba.val[2] = _rgb.val[2];
                _rgba.val[3] = vdup_n_u8(255);
                vst4_u8(outptr, _rgba);

                ptr += 24;
                outptr += 32;
            }
#endif // __ARM_NEON
            for (; x < rgb_roi.cols; x++) {
                outptr[0] = ptr[0];
                outptr[1] = ptr[1];
                outptr[2] = ptr[2];
                outptr[3] = 255;

                ptr += 3;
                outptr += 4;
            }
        }
    }

    ANativeWindow_unlockAndPost(win);
}

static MyNdkCamera *g_camera = 0;

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "JNI_OnLoad");

    g_camera = new MyNdkCamera;

    return JNI_VERSION_1_4;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "JNI_OnUnload");

    {


    }

    delete g_camera;
    g_camera = 0;
}

// public native boolean loadModel(AssetManager mgr, int modelid, int cpugpu);
JNIEXPORT jboolean JNICALL
Java_dev_gancheng_mediapipe_Facemesh_loadModel(JNIEnv *env, jobject thiz, jobject assetManager) {
//    std::string proto = fdLoadFile(JavaStringToString(env,proto));
//    std::string model = fdLoadFile(JavaStringToString(env,model));
    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
    detector = new mirror::FaceDetector();
    landmarker = new mirror::FaceLandmarkDetector();
    bool detectorLoaded = detector->LoadModel(mgr, "face_detection_short_range_fp16.mnn");
    bool landmarkerLoaded = landmarker->LoadModel(mgr, "face_landmarks_detector_fp16.mnn");
    detector->setFormat(mirror::PixelFormat::RGB);
    landmarker->setFormat(mirror::PixelFormat::RGB);
    return detectorLoaded && landmarkerLoaded;
}

// public native boolean openCamera(int facing);
JNIEXPORT jboolean JNICALL
Java_dev_gancheng_mediapipe_Facemesh_openCamera(JNIEnv *env, jobject thiz, jint facing) {
    if (facing < 0 || facing > 1)
        return JNI_FALSE;

    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "openCamera %d", facing);

    g_camera->open((int) facing);

    return JNI_TRUE;
}

// public native boolean closeCamera();
JNIEXPORT jboolean JNICALL
Java_dev_gancheng_mediapipe_Facemesh_closeCamera(JNIEnv *env, jobject thiz) {
    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "closeCamera");

    g_camera->close();

    return JNI_TRUE;
}

// public native boolean setOutputWindow(Surface surface);
JNIEXPORT jboolean JNICALL
Java_dev_gancheng_mediapipe_Facemesh_setOutputWindow(JNIEnv *env, jobject thiz, jobject surface) {
    ANativeWindow *win = ANativeWindow_fromSurface(env, surface);

    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "setOutputWindow %p", win);

    g_camera->set_window(win);

    return JNI_TRUE;
}

}
