// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "ndkcamera.h"
#include "ncnn-20240102-android-vulkan-shared/arm64-v8a/include/ncnn/mat.h"
#include <string>
#include <opencv2/core/core.hpp>

//#include "mat.h"

static void onDisconnected(void *context, ACameraDevice *device) {
    LOGI("onDisconnected %p", device);
}

static void onError(void *context, ACameraDevice *device, int error) {
    LOGI("onError %p %d", device, error);
}

static void onImageAvailable(void *context, AImageReader *reader) {

    AImage *image = nullptr;
    media_status_t status = AImageReader_acquireLatestImage(reader, &image);

    if (status != AMEDIA_OK) {
        // error
        return;
    }

    int32_t format;
    AImage_getFormat(image, &format);

    // assert format == AIMAGE_FORMAT_YUV_420_888

    int32_t width = 0;
    int32_t height = 0;
    AImage_getWidth(image, &width);
    AImage_getHeight(image, &height);

    int32_t y_pixelStride = 0;
    int32_t u_pixelStride = 0;
    int32_t v_pixelStride = 0;
    AImage_getPlanePixelStride(image, 0, &y_pixelStride);
    AImage_getPlanePixelStride(image, 1, &u_pixelStride);
    AImage_getPlanePixelStride(image, 2, &v_pixelStride);

    int32_t y_rowStride = 0;
    int32_t u_rowStride = 0;
    int32_t v_rowStride = 0;
    AImage_getPlaneRowStride(image, 0, &y_rowStride);
    AImage_getPlaneRowStride(image, 1, &u_rowStride);
    AImage_getPlaneRowStride(image, 2, &v_rowStride);

    uint8_t *y_data = nullptr;
    uint8_t *u_data = nullptr;
    uint8_t *v_data = nullptr;
    int y_len = 0;
    int u_len = 0;
    int v_len = 0;
    AImage_getPlaneData(image, 0, &y_data, &y_len);
    AImage_getPlaneData(image, 1, &u_data, &u_len);
    AImage_getPlaneData(image, 2, &v_data, &v_len);

    if (u_data == v_data + 1 && v_data == y_data + width * height && y_pixelStride == 1 &&
        u_pixelStride == 2 && v_pixelStride == 2 && y_rowStride == width && u_rowStride == width &&
        v_rowStride == width) {
        // already nv21  :)
        ((NdkCamera *) context)->on_image((unsigned char *) y_data, (int) width, (int) height);
    } else {
        // construct nv21
        unsigned char *nv21 = new unsigned char[width * height + width * height / 2];
        {
            // Y
            unsigned char *yptr = nv21;
            for (int y = 0; y < height; y++) {
                const unsigned char *y_data_ptr = y_data + y_rowStride * y;
                for (int x = 0; x < width; x++) {
                    yptr[0] = y_data_ptr[0];
                    yptr++;
                    y_data_ptr += y_pixelStride;
                }
            }

            // UV
            unsigned char *uvptr = nv21 + width * height;
            for (int y = 0; y < height / 2; y++) {
                const unsigned char *v_data_ptr = v_data + v_rowStride * y;
                const unsigned char *u_data_ptr = u_data + u_rowStride * y;
                for (int x = 0; x < width / 2; x++) {
                    uvptr[0] = v_data_ptr[0];
                    uvptr[1] = u_data_ptr[0];
                    uvptr += 2;
                    v_data_ptr += v_pixelStride;
                    u_data_ptr += u_pixelStride;
                }
            }
        }

        ((NdkCamera *) context)->on_image((unsigned char *) nv21, (int) width, (int) height);

        delete[] nv21;
    }

    AImage_delete(image);
}

static void onSessionActive(void *context, ACameraCaptureSession *session) {
    LOGI("onSessionActive %p", session);
}

static void onSessionReady(void *context, ACameraCaptureSession *session) {
    LOGI("onSessionReady %p", session);
}

static void onSessionClosed(void *context, ACameraCaptureSession *session) {
    LOGI("onSessionClosed %p", session);
}

void onCaptureFailed(void *context, ACameraCaptureSession *session, ACaptureRequest *request,
                     ACameraCaptureFailure *failure) {
    LOGI("onCaptureFailed %p %p %p", session, request,
         failure);
}

void onCaptureSequenceCompleted(void *context, ACameraCaptureSession *session, int sequenceId,
                                int64_t frameNumber) {
    LOGI("onCaptureSequenceCompleted %p %d %ld",
         session, sequenceId, frameNumber);
}

void onCaptureSequenceAborted(void *context, ACameraCaptureSession *session, int sequenceId) {
    LOGI("onCaptureSequenceAborted %p %d", session,
         sequenceId);
}

void onCaptureCompleted(void *context, ACameraCaptureSession *session, ACaptureRequest *request,
                        const ACameraMetadata *result) {
//     LOGI("onCaptureCompleted %p %p %p", session, request, result);
}

NdkCamera::NdkCamera() {
    camera_facing = 0;
    camera_orientation = 0;

    camera_manager = nullptr;
    camera_device = nullptr;
    image_reader = nullptr;
    image_reader_surface = nullptr;
    image_reader_target = nullptr;
    capture_request = nullptr;
    capture_session_output_container = nullptr;
    capture_session_output = nullptr;
    capture_session = nullptr;


    // setup imagereader and its surface
    {
        AImageReader_new(640, 480, AIMAGE_FORMAT_YUV_420_888, /*maxImages*/2, &image_reader);

        AImageReader_ImageListener listener;
        listener.context = this;
        listener.onImageAvailable = onImageAvailable;

        AImageReader_setImageListener(image_reader, &listener);

        AImageReader_getWindow(image_reader, &image_reader_surface);

        ANativeWindow_acquire(image_reader_surface);
    }
}

NdkCamera::~NdkCamera() {
    close();

    if (image_reader) {
        AImageReader_delete(image_reader);
        image_reader = nullptr;
    }

    if (image_reader_surface) {
        ANativeWindow_release(image_reader_surface);
        image_reader_surface = nullptr;
    }
}

int NdkCamera::open(int _camera_facing) {
    LOGI("open");

    camera_facing = _camera_facing;

    camera_manager = ACameraManager_create();

    // find front camera
    std::string camera_id;
    {
        ACameraIdList *camera_id_list = nullptr;
        ACameraManager_getCameraIdList(camera_manager, &camera_id_list);

        for (int i = 0; i < camera_id_list->numCameras; ++i) {
            const char *id = camera_id_list->cameraIds[i];
            ACameraMetadata *camera_metadata = nullptr;
            ACameraManager_getCameraCharacteristics(camera_manager, id, &camera_metadata);

            // query facing
            acamera_metadata_enum_android_lens_facing_t facing = ACAMERA_LENS_FACING_FRONT;
            {
                ACameraMetadata_const_entry e = {0};
                ACameraMetadata_getConstEntry(camera_metadata, ACAMERA_LENS_FACING, &e);
                facing = (acamera_metadata_enum_android_lens_facing_t) e.data.u8[0];
            }

            if (camera_facing == 0 && facing != ACAMERA_LENS_FACING_FRONT) {
                ACameraMetadata_free(camera_metadata);
                continue;
            }

            if (camera_facing == 1 && facing != ACAMERA_LENS_FACING_BACK) {
                ACameraMetadata_free(camera_metadata);
                continue;
            }

            camera_id = id;

            // query orientation
            int orientation = 0;
            {
                ACameraMetadata_const_entry e = {0};
                ACameraMetadata_getConstEntry(camera_metadata, ACAMERA_SENSOR_ORIENTATION, &e);

                orientation = (int) e.data.i32[0];
            }

            camera_orientation = orientation;

            ACameraMetadata_free(camera_metadata);

            break;
        }

        ACameraManager_deleteCameraIdList(camera_id_list);
    }

    LOGI("open %s %d", camera_id.c_str(),
         camera_orientation);

    // open camera
    {
        ACameraDevice_StateCallbacks camera_device_state_callbacks;
        camera_device_state_callbacks.context = this;
        camera_device_state_callbacks.onDisconnected = onDisconnected;
        camera_device_state_callbacks.onError = onError;

        ACameraManager_openCamera(camera_manager, camera_id.c_str(), &camera_device_state_callbacks,
                                  &camera_device);
    }

    // capture request
    {
        ACameraDevice_createCaptureRequest(camera_device, TEMPLATE_PREVIEW, &capture_request);

        ACameraOutputTarget_create(image_reader_surface, &image_reader_target);
        ACaptureRequest_addTarget(capture_request, image_reader_target);
    }

    // capture session
    {
        ACameraCaptureSession_stateCallbacks camera_capture_session_state_callbacks;
        camera_capture_session_state_callbacks.context = this;
        camera_capture_session_state_callbacks.onActive = onSessionActive;
        camera_capture_session_state_callbacks.onReady = onSessionReady;
        camera_capture_session_state_callbacks.onClosed = onSessionClosed;

        ACaptureSessionOutputContainer_create(&capture_session_output_container);

        ACaptureSessionOutput_create(image_reader_surface, &capture_session_output);

        ACaptureSessionOutputContainer_add(capture_session_output_container,
                                           capture_session_output);

        ACameraDevice_createCaptureSession(camera_device, capture_session_output_container,
                                           &camera_capture_session_state_callbacks,
                                           &capture_session);

        ACameraCaptureSession_captureCallbacks camera_capture_session_capture_callbacks;
        camera_capture_session_capture_callbacks.context = this;
        camera_capture_session_capture_callbacks.onCaptureStarted = 0;
        camera_capture_session_capture_callbacks.onCaptureProgressed = 0;
        camera_capture_session_capture_callbacks.onCaptureCompleted = onCaptureCompleted;
        camera_capture_session_capture_callbacks.onCaptureFailed = onCaptureFailed;
        camera_capture_session_capture_callbacks.onCaptureSequenceCompleted = onCaptureSequenceCompleted;
        camera_capture_session_capture_callbacks.onCaptureSequenceAborted = onCaptureSequenceAborted;
        camera_capture_session_capture_callbacks.onCaptureBufferLost = 0;

        ACameraCaptureSession_setRepeatingRequest(capture_session,
                                                  &camera_capture_session_capture_callbacks, 1,
                                                  &capture_request, nullptr);
    }

    return 0;
}

void NdkCamera::close() {
    LOGI("close");

    if (capture_session) {
        ACameraCaptureSession_stopRepeating(capture_session);
        ACameraCaptureSession_close(capture_session);
        capture_session = nullptr;
    }

    if (camera_device) {
        ACameraDevice_close(camera_device);
        camera_device = nullptr;
    }

    if (capture_session_output_container) {
        ACaptureSessionOutputContainer_free(capture_session_output_container);
        capture_session_output_container = nullptr;
    }

    if (capture_session_output) {
        ACaptureSessionOutput_free(capture_session_output);
        capture_session_output = nullptr;
    }

    if (capture_request) {
        ACaptureRequest_free(capture_request);
        capture_request = nullptr;
    }

    if (image_reader_target) {
        ACameraOutputTarget_free(image_reader_target);
        image_reader_target = nullptr;
    }

    if (camera_manager) {
        ACameraManager_delete(camera_manager);
        camera_manager = nullptr;
    }
}

void NdkCamera::on_image(const cv::Mat &rgb) const {
}

void NdkCamera::on_image(const unsigned char *nv21, int nv21_width, int nv21_height) const {
    // rotate nv21
    int w = 0;
    int h = 0;
    int rotate_type = 0;
    if (camera_orientation == 0) {
        w = nv21_width;
        h = nv21_height;
        rotate_type = camera_facing == 0 ? 2 : 1;
    }
    if (camera_orientation == 90) {
        w = nv21_height;
        h = nv21_width;
        rotate_type = camera_facing == 0 ? 5 : 6;
    }
    if (camera_orientation == 180) {
        w = nv21_width;
        h = nv21_height;
        rotate_type = camera_facing == 0 ? 4 : 3;
    }
    if (camera_orientation == 270) {
        w = nv21_height;
        h = nv21_width;
        rotate_type = camera_facing == 0 ? 7 : 8;
    }

    cv::Mat nv21_rotated(h + h / 2, w, CV_8UC1);
    ncnn::kanna_rotate_yuv420sp(nv21, nv21_width, nv21_height, nv21_rotated.data, w, h,
                                rotate_type);

//    // nv21_rotated to rgb
    cv::Mat rgb(h, w, CV_8UC3);
    ncnn::yuv420sp2rgb(nv21_rotated.data, w, h, rgb.data);

    on_image(rgb);
}

