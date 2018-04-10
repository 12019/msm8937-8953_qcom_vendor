/*
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

package com.qualcomm.qti.ipass;

import android.app.Activity;
import android.view.Menu;
import android.view.MenuItem;
import android.content.Intent;
import android.graphics.RectF;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.content.Context;
import android.view.TextureView;
import android.graphics.SurfaceTexture;
import android.widget.Toast;
import android.os.HandlerThread;
import android.view.Gravity;

import java.util.Arrays;
import java.nio.ByteBuffer;

import com.qualcomm.qti.ipass.R;
import android.hardware.iris.IrisManager;
import android.hardware.iris.Iris;
import android.os.CancellationSignal;

import android.graphics.Rect;
import android.util.Size;

public class SimpleActivity extends Activity  implements IrisView.IrisViewListner{

    private static final String TAG = "IrisActivity";
    boolean mStarted = false;
    private Button mBtnStart;
    private Toast mToast;

    private enum Status {
        STOPPED, VERIFYING
    };

    private Status mCurrentStatus;

    private final Handler mHandler = new Handler();
    private TextView mTextViewInfo;

    private IrisManager mIrisManager;
    CancellationSignal mCancelSignal;
    private IrisView mIrisView;
    private IrisPreview2 mPreview;
    private Size mDefaultSensorSize = new Size(4160, 3120);
    private Rect mDefaultPreviewRect = new Rect(460, 2180, 2180, 3040);

    @Override
    public void onSurfaceAvailable() {
        // This is called every time the surface returns to the foreground
        Log.v(TAG, "Surface created");
    }

    @Override
    public void onSurfaceDestroyed() {
        Log.v(TAG, "Surface destroyed");
        mPreview.stop();
    }

    public class TestAuthenticationCallback extends IrisManager.AuthenticationCallback {
        @Override
        public void onAuthenticationError(int errorCode, CharSequence errString) {
            postInfo("Authenticate error, err code=" + errorCode +" "+errString);
            mCurrentStatus = Status.STOPPED;
            updateStartButton();
            mPreview.stop();
            mToast.cancel();
        }

        @Override
        public void onAuthenticationHelp(int helpCode, CharSequence helpString) {
            if (helpCode != 0) {
                if (helpCode == IrisManager.IRIS_HELP_IR_SENSOR_DONE)
                    mPreview.stop();

                mToast.setText(helpString);
                mToast.show();
            } else {
                mToast.cancel();
            }
        }

        @Override
        public void onAuthenticationSucceeded(IrisManager.AuthenticationResult result) {
            mCurrentStatus = Status.STOPPED;
            updateStartButton();
            mPreview.stop();
            postInfo("Authenticate successful");
            mToast.cancel();
        }

        @Override
        public void onAuthenticationFailed() {
            postInfo("Fail to Authenticate");
            mCurrentStatus = Status.STOPPED;
            updateStartButton();
            mPreview.stop();
            mToast.cancel();
        }
    };

    private void updateStartButton() {
        if (mCurrentStatus == Status.STOPPED) {
            mBtnStart.setEnabled(true);
        } else {
            mBtnStart.setEnabled(false);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        mPreview.init();
    }

    @Override
    protected void onPause() {
        mPreview.deinit();
        if (mCancelSignal != null)
            mCancelSignal.cancel();
        super.onPause();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.simple);

        IrisSensorConfiguration sensorConfig = new IrisSensorConfiguration(mDefaultSensorSize, mDefaultPreviewRect);
        mDefaultSensorSize = sensorConfig.getSensorSize();
        mDefaultPreviewRect = sensorConfig.getPreviewRect();

        mIrisManager = (IrisManager)getSystemService(Context.IRIS_SERVICE);

        mTextViewInfo = (TextView) findViewById(R.id.textViewInfo);
        mIrisView = (IrisView)findViewById(R.id.texture);
        mIrisView.setCalback(this);

        mIrisView.setCropWindow(mDefaultPreviewRect.left, mDefaultPreviewRect.top, mDefaultPreviewRect.width(), mDefaultPreviewRect.height());
        mIrisView.setPreviewSize(mDefaultSensorSize.getWidth(), mDefaultSensorSize.getHeight());

        mCurrentStatus = Status.STOPPED;

        mBtnStart = (Button) findViewById(R.id.btnStart);
        mBtnStart.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mCurrentStatus == Status.STOPPED) {
                    handle_authenticate();
                    updateStartButton();
                }
            }
        });

        mPreview = new IrisPreview2(this);
        mToast = Toast.makeText(this, "", Toast.LENGTH_SHORT);
        mToast.setGravity(Gravity.TOP|Gravity.LEFT, 0, 0);
    }


    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        return onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        return super.onOptionsItemSelected(item);
    }

    public void handle_authenticate() {
        if (!mIrisManager.hasEnrolledIris()) {
            postInfo("No enrolled iris");
            return;
        }

        TestAuthenticationCallback authenticationResult = new TestAuthenticationCallback();
        mCancelSignal = new CancellationSignal();

        mPreview.start(mIrisView.getSurface());
        mIrisManager.authenticate(null, mCancelSignal, 0, authenticationResult, null);
        mCurrentStatus = Status.VERIFYING;
        postInfo("Authenticating...");
    }

    void postInfo(final String info) {
        mTextViewInfo.setText(info);
    }
}
