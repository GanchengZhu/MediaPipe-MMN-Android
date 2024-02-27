/*******************************************************************************
 * Copyright (C) 2023 Gancheng Zhu
 * Email: psycho@zju.edu.cn
 ******************************************************************************/

package dev.gancheng.mediapipe;

import android.content.res.AssetManager;
import android.view.Surface;

public class Facemesh {
    static {
        System.loadLibrary("MediaPipeMNN");
    }

    public native boolean loadModel(AssetManager assetManager);
    public native boolean openCamera(int facing);
    public native boolean closeCamera();
    public native boolean setOutputWindow(Surface surface);


}
