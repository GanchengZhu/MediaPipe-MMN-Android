/*******************************************************************************
 * Copyright (C) 2023 Gancheng Zhu
 * Email: psycho@zju.edu.cn
 ******************************************************************************/

package dev.gancheng.mediapipe;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.os.Looper;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.TypedValue;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.util.Arrays;
import java.util.LinkedList;
import java.util.Locale;
import java.util.Queue;

public class VideoSurfaceView extends SurfaceView implements SurfaceHolder.Callback, Runnable{
    private float x;
    private float y;
    private SurfaceHolder holder;
    private volatile boolean isDraw = false;// 控制绘制的开关
    private Paint gazePaint;
    private Canvas mCanvas;
    private Paint cameraImgPaint;
    private Paint textPaint;
    private Paint fpsTextPaint;
    private float textSize = 64;
    private float[] fragmentW;
    private float[] fragmentH;
    private long lastTimeStamp;
    private long interval;
    private Bitmap mCacheBitmap;
    private float mScale;
    private volatile boolean hasGaze = false;
    private Queue<Long> timeStampRecord;
    private volatile float fps;

    public VideoSurfaceView(Context context) {
        super(context);
        init();
    }

    public VideoSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }


    private void init() {
        Log.d("surface view", "init");
        holder = this.getHolder();
        holder.addCallback(this);
        DisplayMetrics metrics = getResources().getDisplayMetrics();
        int stroke = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 2, metrics);
        gazePaint = new Paint();
        gazePaint.setAntiAlias(true);
        gazePaint.setDither(true);
        gazePaint.setColor(Color.GREEN);
        gazePaint.setStrokeWidth(stroke);
        gazePaint.setStyle(Paint.Style.STROKE);

        cameraImgPaint = new Paint();
        cameraImgPaint.setAlpha(255 * 2 / 10);

        textPaint = new Paint();
        textPaint.setColor(Color.BLACK);
        textPaint.setAntiAlias(true);
        textPaint.setStrokeWidth(50);
        textPaint.setTextSize(textSize);

        fpsTextPaint = new Paint();
        fpsTextPaint.setColor(Color.BLACK);
        fpsTextPaint.setAntiAlias(true);
        fpsTextPaint.setTextSize(36);

        setZOrderOnTop(true);
        setZOrderMediaOverlay(true);
        holder.setFormat(PixelFormat.TRANSPARENT);
        timeStampRecord = new LinkedList<>();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        isDraw = true;
        new Thread(this).start();
        float[] textWidth = new float[1];
        textPaint.getTextWidths("A", textWidth);
        Paint.FontMetrics fm = textPaint.getFontMetrics();
        float height1 = fm.descent - fm.ascent;
        float height2 = fm.bottom - fm.top + fm.leading;
        //其中 height1 表示文字的高度， height2表示行高
        fragmentW = initPos(getWidth(), textWidth[0], 3, true);
        fragmentW[0] += 100;
        fragmentW[2] -= 100;
        fragmentH = initPos(getHeight(), height2, 6, false);
        fragmentH[fragmentH.length - 1] -= 100;
        Log.d("Fragment", Arrays.toString(fragmentH));
    }


    protected float[] initPos(float layoutWOrH, float textSize, int fragmentN, boolean isX) {
        float factor;
        if (isX) {
            factor = (layoutWOrH - textSize) / (fragmentN - 1);
        } else {
            factor = (layoutWOrH - textSize) / (fragmentN - 1);
        }
        float[] list = new float[fragmentN];
        for (int i = 0; i < list.length; i++) {
            if (isX)
                list[i] = factor * i;
            else
                list[i] = factor * i + textSize;
        }
        return list;
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        isDraw = false;
    }

    @Override
    public void run() {
        Looper.prepare();
        while (isDraw) {
            drawUI();
        }
    }

    /**
     * 界面绘制
     */
//    float [] fragmentH = new float[]{0, getWidth() / 2f - textSize / 2f, getWidth() - textSize};
    char a = 'A';

    public void drawUI() {

        try {
            mCanvas = holder.lockCanvas();
//            mCanvas.drawColor(Color.TRANSPARENT, PorterDuff.Mode.CLEAR);
            if (mCanvas != null) {
                mCanvas.drawRGB(255, 255, 255);
                if (mCacheBitmap != null) {
                    mCanvas.drawBitmap(mCacheBitmap, new Rect(0, 0, mCacheBitmap.getWidth(), mCacheBitmap.getHeight()),
                            new Rect((int) (mCanvas.getWidth() - mScale * mCacheBitmap.getWidth()) / 2,
                                    (int) (mCanvas.getHeight() - mScale * mCacheBitmap.getHeight()) / 2,
                                    (int) (mCanvas.getWidth() - mScale * mCacheBitmap.getWidth()) / 2 + (int) (mScale * mCacheBitmap.getWidth()),
                                    (int) (mCanvas.getHeight() - mScale * mCacheBitmap.getHeight()) / 2 + (int) (mScale * mCacheBitmap.getHeight())), cameraImgPaint);
                }

                for (int i = 0; i < 18; i++) {
                    mCanvas.drawText(String.valueOf((char) (a + i)), fragmentW[i % 3], fragmentH[i / 3], textPaint);
//                    mCanvas.drawText(String.valueOf((char) (a + i)),0,0, textPaint);
                }

                if (timeStampRecord!= null && fps != 0f) {
                    mCanvas.drawText(String.format(Locale.CHINA, "FPS: %.2f", fps),
                            50, 50, fpsTextPaint);
                }

                mCanvas.drawCircle(x, y, 10, gazePaint);
//            Log.d("SURFACE VIEW RUNING"," ");
            }
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            try {
                if (holder != null) {
                    holder.unlockCanvasAndPost(mCanvas);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }


    public SurfaceHolder getSurfaceHolder() {
        return holder;
    }

    @Override
    public void setX(float x) {
        this.x = x;
    }

    @Override
    public void setY(float y) {
        this.y = y;
    }

    public void setXY(float x, float y) {
        this.x = x;
        this.y = y;
    }

    public void setTimeStamp(long timeStamp) {
        interval = timeStamp - lastTimeStamp;
        lastTimeStamp = timeStamp;
    }


    public boolean isDraw() {
        return isDraw;
    }

    public void setDraw(boolean draw) {
        isDraw = draw;
    }

//    public void setMat(Mat imgMat) {
//        new Thread(() -> {
//            if (mCacheBitmap != null) {
//                Utils.matToBitmap(imgMat, mCacheBitmap);
//                imgMat.release();
//            }
//        }).start();
//    }

    public Bitmap getCacheBitmap() {
        return mCacheBitmap;
    }

    public void setCacheBitmap(Bitmap mCacheBitmap) {
        this.mCacheBitmap = mCacheBitmap;
    }

//    public void setCacheBitmap(Mat mat) {
//        if (mat != null && mat.cols() != 0 && mat.rows() != 0) {
//            Utils.matToBitmap(mat, this.mCacheBitmap);
//            mat.release();
//        }
//    }

    public float getScale() {
        return mScale;
    }

    public void setScale(float mScale) {
        this.mScale = mScale;
    }

    public void setGaze(float[] gazeResult) {
        if (gazeResult == null) {
            hasGaze = false;
            return;
        }
        hasGaze = true;
        x = gazeResult[0];
        y = gazeResult[1];
    }

    public void addTimeStamp(long timeStamp) {
        if (timeStampRecord.size() == 60) {
            timeStampRecord.poll();
            timeStampRecord.offer(timeStamp);
            long interval = (((LinkedList<Long>) timeStampRecord).peek() - ((LinkedList<Long>) timeStampRecord).getLast());
            fps = -timeStampRecord.size() / (interval / 1e9f);
        } else {
            timeStampRecord.offer(timeStamp);
        }
    }

}
