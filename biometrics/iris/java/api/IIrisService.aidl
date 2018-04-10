/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Not a Contribution.
 * Apache license notifications and license are retained
 * for attribution purposes only.
 */

/*
 * Copyright (C) 2014 The Android Open Source Project
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
package android.hardware.iris;

import android.os.Bundle;
import android.hardware.iris.IIrisServiceReceiver;
import android.hardware.iris.IIrisServiceLockoutResetCallback;
import android.hardware.iris.Iris;
import java.util.List;

/**
 * Communication channel from client to the iris service.
 * @hide
 */
interface IIrisService {
    // Authenticate the given sessionId with an iris
    void authenticate(IBinder token, long sessionId, int userId,
            IIrisServiceReceiver receiver, int flags, String opPackageName);

    // Cancel authentication for the given sessionId
    void cancelAuthentication(IBinder token, String opPackageName);

    // Start iris enrollment
    void enroll(IBinder token, in byte [] cryptoToken, int groupId, IIrisServiceReceiver receiver,
            int flags);

    // Cancel enrollment in progress
    void cancelEnrollment(IBinder token);

    // Any errors resulting from this call will be returned to the listener
    void remove(IBinder token, int irisId, int groupId, IIrisServiceReceiver receiver);

    // Rename the iris specified by irisId and groupId to the given name
    void rename(int irisId, int groupId, String name);

    // Get a list of enrolled iris in the given group.
    List<Iris> getEnrolledIris(int groupId, String opPackageName);

    // Determine if HAL is loaded and ready
    boolean isHardwareDetected(long deviceId, String opPackageName);

    // Get a pre-enrollment authentication token
    long preEnroll(IBinder token);

    // Finish an enrollment sequence and invalidate the authentication token
    int postEnroll(IBinder token);

    // Determine if a user has at least one enrolled iris
    boolean hasEnrolledIris(int groupId, String opPackageName);

    // Gets the authenticator ID for iris
    long getAuthenticatorId(String opPackageName);

    // Reset the timeout when user authenticates with strong auth (e.g. PIN, pattern or password)
    void resetTimeout(in byte [] cryptoToken);

    // Add a callback which gets notified when the iris lockout period expired.
    void addLockoutResetCallback(IIrisServiceLockoutResetCallback callback);
}
