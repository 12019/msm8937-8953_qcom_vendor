/*
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Not a Contribution.
 * Apache license notifications and license are retained
 * for attribution purposes only.
 */

/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

package com.android.settings.iris;

import android.content.Intent;
import android.content.res.ColorStateList;
import android.os.Bundle;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.util.Log;
import android.util.Size;
import android.graphics.Rect;
import android.view.Gravity;

import android.app.Activity;
import android.os.Handler;
import android.widget.Toast;

import com.android.settings.ChooseLockSettingsHelper;
import com.android.settings.R;

import android.util.Size;

/**
 * Activity which handles the actual enrolling for fingerprint.
 */
public class IrisEnrollEnrolling extends IrisEnrollBase 
    implements IrisView.IrisViewListner, IrisEnrollSidecar.Listener {

    private static final String TAG_SIDECAR = "sidecar";
    private static final String TAG = "IrisEnrollEnrolling";

    private static final int PROGRESS_BAR_MAX = 100;
    private static final int FINISH_DELAY = 250;

    private ProgressBar mProgressBar;
    private ProgressBar mProgressBar1;
    private TextView mStartMessage;
    private TextView mErrorText;
    private IrisEnrollSidecar mSidecar;
    private boolean mRestoring;

    private IrisView mIrisView;
    private IrisPreview2 mPreview;
    private final Handler mHandler = new Handler();
    private Toast mToast;
    private Size mDefaultSensorSize = new Size(4160, 3120);
    private Rect mDefaultPreviewRect = new Rect(460, 2180, 2180, 3040);

    @Override  
    public void onSurfaceAvailable() {

        Log.e(TAG, "onSurfaceAvailable");
        mHandler.post(mEnrollRunnable);
    }

    @Override
    public void onSurfaceDestroyed() {  
        Log.e(TAG, "onSurfaceDestroyed");
        mPreview.stop();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        IrisSensorConfiguration sensorConfig = new IrisSensorConfiguration(mDefaultSensorSize, mDefaultPreviewRect);
        mDefaultSensorSize = sensorConfig.getSensorSize();
        mDefaultPreviewRect = sensorConfig.getPreviewRect();
        super.onCreate(savedInstanceState);
        setContentView(R.layout.iris_enroll_enrolling);
        mStartMessage = (TextView) findViewById(R.id.start_message);
        mProgressBar = (ProgressBar) findViewById(R.id.iris_progress_bar);
        mProgressBar1 = (ProgressBar) findViewById(R.id.iris_progress_bar1);
        mIrisView = (IrisView)findViewById(R.id.iris_preview_texture);
        mIrisView.setCalback(this);
        mIrisView.setCropWindow(mDefaultPreviewRect.left, mDefaultPreviewRect.top, mDefaultPreviewRect.width(), mDefaultPreviewRect.height());
        mIrisView.setPreviewSize(mDefaultSensorSize.getWidth(), mDefaultSensorSize.getHeight());
        mPreview = new IrisPreview2(this);
        mRestoring = savedInstanceState != null;
        mToast = Toast.makeText(this, "", Toast.LENGTH_SHORT);
        mToast.setGravity(Gravity.TOP | Gravity.LEFT, 0, 0);
        Log.e(TAG, "sensor width " + mDefaultSensorSize.getWidth() + " sensor height "+ mDefaultSensorSize.getHeight());
        Log.e(TAG, "preview width " + mDefaultPreviewRect.width() + " preview height "+ mDefaultPreviewRect.height());
    }

    @Override
    protected void onStart() {
        super.onStart();
        mPreview.init();
    }

    @Override
    protected void onStop() {
        Log.w(TAG, "onStop");
        super.onStop();
        if (mSidecar != null) {
            mSidecar.setListener(null);
        }

        mPreview.deinit();
        if (!isChangingConfigurations()) {
            finish();
        }
    }

    private void startEnroll() {
        mPreview.start(mIrisView.getSurface());

        mSidecar = (IrisEnrollSidecar) getFragmentManager().findFragmentByTag(TAG_SIDECAR);
        if (mSidecar == null) {
            mSidecar = new IrisEnrollSidecar();
            getFragmentManager().beginTransaction().add(mSidecar, TAG_SIDECAR).commit();
        }
        mSidecar.setListener(this);
        updateProgress();
    }

    private void launchFinish(byte[] token) {
        Log.w(TAG, "launchFinish");
        Intent intent = getFinishIntent();
        intent.addFlags(Intent.FLAG_ACTIVITY_FORWARD_RESULT);
        intent.putExtra(ChooseLockSettingsHelper.EXTRA_KEY_CHALLENGE_TOKEN, token);
        startActivity(intent);
        finish();
    }

    protected Intent getFinishIntent() {
        return new Intent(this, IrisEnrollFinish.class);
    }

    @Override
    public void onEnrollmentHelp(CharSequence helpString) {
        showHelp(helpString);
    }

    @Override
    public void onEnrollmentError(CharSequence errString) {
        showError(errString);
        mPreview.stop();
        mToast.cancel();
    }

    @Override
    public void onEnrollmentProgressChange(int progress) {
        updateProgress();
        clearError();
    }

    @Override
    public void onEnrollmentSensorDone(CharSequence helpString) {
        Log.w(TAG, "onEnrollmentSensorDone closing preview camera");
        showError(helpString);
        mPreview.stop();
    }

    private void updateProgress() {
        int progress = getProgress(100, mSidecar.getEnrollmentProgress());
        int previousProgress = mProgressBar.getProgress();
        mProgressBar.setProgress(progress);
        mProgressBar1.setProgress(progress);
        Log.w(TAG, "updateProgress " + progress);
        if (progress > previousProgress)
            mToast.cancel();

        if (progress == 100) {
            mPreview.stop();
            mProgressBar.postDelayed(mDelayedFinishRunnable, FINISH_DELAY);
        }
    }

    private int getProgress(int steps, int progress) {
        int p = Math.max(0, progress);
        return PROGRESS_BAR_MAX * p / steps;
    }

    private void showError(CharSequence error) {
        Log.w(TAG, "showError " + error);
        mStartMessage.setText(error);
    }

    private void showHelp(CharSequence helpString) {
        //mToast.cancel();
        mToast.setText(helpString);
        mToast.show();
    }

    private void clearError() {
        Log.w(TAG, "clearError ");
    }

    // Give the user a chance to see progress completed before jumping to the next stage.
    private final Runnable mDelayedFinishRunnable = new Runnable() {
        @Override
        public void run() {
            launchFinish(mToken);
        }
    };

    private final Runnable mEnrollRunnable = new Runnable() {
        @Override
        public void run() {
            startEnroll();
        }
    };

}
