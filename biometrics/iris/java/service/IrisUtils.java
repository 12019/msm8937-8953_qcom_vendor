/*
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Not a Contribution.
 * Apache license notifications and license are retained
 * for attribution purposes only.
 */

/**
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
 * limitations under the License.
 */

package com.android.server.iris;

import android.content.Context;
import android.hardware.iris.Iris;
import android.os.Vibrator;
import android.text.TextUtils;
import android.util.SparseArray;

import com.android.internal.annotations.GuardedBy;

import java.util.List;

/**
 * Utility class for dealing with iris  and iris settings.
 */
public class IrisUtils {

    private static final long[] IRIS_ERROR_VIBRATE_PATTERN = new long[] {0, 30, 100, 30};
    private static final long[] IRIS_SUCCESS_VIBRATE_PATTERN = new long[] {0, 30};

    private static final Object sInstanceLock = new Object();
    private static IrisUtils sInstance;

    @GuardedBy("this")
    private final SparseArray<IrisUserState> mUsers = new SparseArray<>();

    public static IrisUtils getInstance() {
        synchronized (sInstanceLock) {
            if (sInstance == null) {
                sInstance = new IrisUtils();
            }
        }
        return sInstance;
    }

    private IrisUtils() {
    }

    public List<Iris> getIrisForUser(Context ctx, int userId, boolean includeInvalid) {
        return getStateForUser(ctx, userId).getIris(includeInvalid);
    }

    public void addIrisForUser(Context ctx, int irisId, int userId) {
        getStateForUser(ctx, userId).addIris(irisId, userId);
    }

    public void validateIrisForUser(Context ctx, int irisId, int userId) {
        getStateForUser(ctx, userId).validateIris(irisId, userId);
    }

    public void removeIrisForUser(Context ctx, int irisId, int userId) {
        getStateForUser(ctx, userId).removeIris(irisId);
    }

    public void renameIrisForUser(Context ctx, int irisId, int userId, CharSequence name) {
        if (TextUtils.isEmpty(name)) {
            // Don't do the rename if it's empty
            return;
        }
        getStateForUser(ctx, userId).renameIris(irisId, name);
    }

    public static void vibrateIrisError(Context context) {
        Vibrator vibrator = context.getSystemService(Vibrator.class);
        if (vibrator != null) {
            vibrator.vibrate(IRIS_ERROR_VIBRATE_PATTERN, -1);
        }
    }

    public static void vibrateIrisSuccess(Context context) {
        Vibrator vibrator = context.getSystemService(Vibrator.class);
        if (vibrator != null) {
            vibrator.vibrate(IRIS_SUCCESS_VIBRATE_PATTERN, -1);
        }
    }

    private IrisUserState getStateForUser(Context ctx, int userId) {
        synchronized (this) {
            IrisUserState state = mUsers.get(userId);
            if (state == null) {
                state = new IrisUserState(ctx, userId);
                mUsers.put(userId, state);
            }
            return state;
        }
    }
}

