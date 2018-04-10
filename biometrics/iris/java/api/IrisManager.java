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

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.RequiresPermission;
import android.app.ActivityManagerNative;
import android.content.Context;
import android.os.Binder;
import android.os.CancellationSignal;
import android.os.CancellationSignal.OnCancelListener;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.os.UserHandle;
import android.security.keystore.AndroidKeyStoreProvider;
import android.util.Log;
import android.util.Slog;

import java.security.Signature;
import java.util.List;

import javax.crypto.Cipher;
import javax.crypto.Mac;

//todo
import static android.Manifest.permission.USE_IRIS;
import static android.Manifest.permission.MANAGE_IRIS;

/**
 * A class that coordinates access to the iris service.
 * @hide
 * <p>
 * Use {@link android.content.Context#getSystemService(java.lang.String)}
 * with argument {@link android.content.Context#IRIS_SERVICE} to get
 * an instance of this class.
 */

public class IrisManager {
    private static final String TAG = "IrisManager";
    private static final boolean DEBUG = true;
    private static final int MSG_ENROLL_RESULT = 100;
    private static final int MSG_AUTHENTICATION_SUCCEEDED = 101;
    private static final int MSG_AUTHENTICATION_FAILED = 102;
    private static final int MSG_ERROR = 103;
    private static final int MSG_REMOVED = 104;
    private static final int MSG_AUTHENTICATION_PROGRESS = 105;

    //
    // Error messages from fingerprint hardware during initilization, enrollment, authentication or
    // removal. Must agree with the list in fingerprint.h
    //

    /**
     * The iris service not available
     */
    public static final int IRIS_ERROR_SERVICE_NOT_AVAILABLE = 1;

    /**
     * Error state returned when the current request has been running too long. This is intended to
     * prevent programs from waiting for the iris service indefinitely. The timeout is
     * platform and sensor-specific, but is generally on the order of 30 seconds.
     */
    public static final int IRIS_ERROR_TIMEOUT = 2;

    /**
     * The operation was canceled
     */
    public static final int IRIS_ERROR_CANCELED = 3;

    /**
     * Error state returned for operations like enrollment; the operation cannot be completed
     * because there's not enough storage remaining to complete the operation.
     */
    public static final int IRIS_ERROR_NO_SPACE = 4;

    public static final int IRIS_ERROR_NO_USER = 5;

    public static final int IRIS_ERROR_NO_IRIS = 6;
    public static final int IRIS_ERROR_GENERIC = 7;

    public static final int IRIS_ERROR_FATAL = 8;
    public static final int IRIS_ERROR_IR_SENSOR = 9;
    public static final int IRIS_ERROR_BAD_QUALITY = 10;

   /**
     * The operation was canceled because the API is locked out due to too many attempts.
     */
    public static final int IRIS_ERROR_LOCKOUT = 100;

    /**
     * Hardware vendors may extend this list if there are conditions that do not fall under one of
     * the above categories. Vendors are responsible for providing error strings for these errors.
     * @hide
     */
    public static final int IRIS_ERROR_VENDOR_BASE = 1000;


    public static  final int IRIS_HELP_NONE = 0;
    public static  final int IRIS_HELP_BAD_IMAGE_QUALITY = 1;
    public static  final int IRIS_HELP_EYE_NOT_FOUND = 2;
    public static  final int IRIS_HELP_EYE_TOO_CLOSE = 3;
    public static  final int IRIS_HELP_EYE_TOO_FAR = 4;
    public static  final int IRIS_HELP_EYE_TOO_UP = 5;
    public static  final int IRIS_HELP_EYE_TOO_DOWN = 6;
    public static  final int IRIS_HELP_EYE_TOO_LEFT = 7;
    public static  final int IRIS_HELP_EYE_TOO_RIGHT = 8;
    public static  final int IRIS_HELP_MOTION_BLUR = 9;
    public static  final int IRIS_HELP_FOCUS_BLUR = 10;
    public static  final int IRIS_HELP_BAD_EYE_OPENNESS = 11;
    public static  final int IRIS_HELP_BAD_EYE_DISTANCE = 12;
    public static  final int IRIS_HELP_WITH_GLASS = 13;
    public static  final int IRIS_HELP_IR_SENSOR_DONE = 14;

    private IIrisService mService;
    private Context mContext;
    private IBinder mToken = new Binder();
    private AuthenticationCallback mAuthenticationCallback;
    private EnrollmentCallback mEnrollmentCallback;
    private RemovalCallback mRemovalCallback;
    private CryptoObject mCryptoObject;
    private Iris mRemovalIris;
    private Handler mHandler;

    private class OnEnrollCancelListener implements OnCancelListener {
        @Override
        public void onCancel() {
            cancelEnrollment();
        }
    }

    private class OnAuthenticationCancelListener implements OnCancelListener {
        private CryptoObject mCrypto;

        public OnAuthenticationCancelListener(CryptoObject crypto) {
            mCrypto = crypto;
        }

        @Override
        public void onCancel() {
            cancelAuthentication(mCrypto);
        }
    }

    /**
     * A wrapper class for the crypto objects supported by IrisManager. Currently the
     * framework supports {@link Signature}, {@link Cipher} and {@link Mac} objects.
     */
    public static final class CryptoObject {

        public CryptoObject(@NonNull Signature signature) {
            mCrypto = signature;
        }

        public CryptoObject(@NonNull Cipher cipher) {
            mCrypto = cipher;
        }

        public CryptoObject(@NonNull Mac mac) {
            mCrypto = mac;
        }

        /**
         * Get {@link Signature} object.
         * @return {@link Signature} object or null if this doesn't contain one.
         */
        public Signature getSignature() {
            return mCrypto instanceof Signature ? (Signature) mCrypto : null;
        }

        /**
         * Get {@link Cipher} object.
         * @return {@link Cipher} object or null if this doesn't contain one.
         */
        public Cipher getCipher() {
            return mCrypto instanceof Cipher ? (Cipher) mCrypto : null;
        }

        /**
         * Get {@link Mac} object.
         * @return {@link Mac} object or null if this doesn't contain one.
         */
        public Mac getMac() {
            return mCrypto instanceof Mac ? (Mac) mCrypto : null;
        }

        /**
         * @hide
         * @return the opId associated with this object or 0 if none
         */
        public long getOpId() {
            return mCrypto != null ?
                    AndroidKeyStoreProvider.getKeyStoreOperationHandle(mCrypto) : 0;
        }

        private final Object mCrypto;
    };

    /**
     * Container for callback data from {@link IrisManager#authenticate(CryptoObject,
     *     CancellationSignal, int, AuthenticationCallback, Handler)}.
     */
    public static class AuthenticationResult {
        private Iris mIris;
        private CryptoObject mCryptoObject;

        /**
         * Authentication result
         *
         * @param crypto the crypto object
         * @param iris the recognized iris data, if allowed.
         * @hide
         */
        public AuthenticationResult(CryptoObject crypto, Iris iris) {
            mCryptoObject = crypto;
            mIris = iris;
        }

        /**
         * Obtain the crypto object associated with this transaction
         * @return crypto object provided to {@link IrisManager#authenticate(CryptoObject,
         *     CancellationSignal, int, AuthenticationCallback, Handler)}.
         */
        public CryptoObject getCryptoObject() { return mCryptoObject; }

        /**
         * Obtain the Iris associated with this operation. Applications are strongly
         * discouraged from associating specific iris with specific applications or operations.
         *
         * @hide
         */
        public Iris getIris() { return mIris; }
    };

    /**
     * Callback structure provided to {@link IrisManager#authenticate(CryptoObject,
     * CancellationSignal, int, AuthenticationCallback, Handler)}. Users of {@link
     * IrisManager#authenticate(CryptoObject, CancellationSignal,
     * int, AuthenticationCallback, Handler) } must provide an implementation of this for listening to
     * iris events.
     */
    public static abstract class AuthenticationCallback {
        /**
         * Called when an unrecoverable error has been encountered and the operation is complete.
         * No further callbacks will be made on this object.
         * @param errorCode An integer identifying the error message
         * @param errString A human-readable error string that can be shown in UI
         */
        public void onAuthenticationError(int errorCode, CharSequence errString) { }

        /**
         * Called when a recoverable error has been encountered during authentication. The help
         * string is provided to give the user guidance for what went wrong, such as
         * "Sensor dirty, please clean it."
         * @param helpCode An integer identifying the error message
         * @param helpString A human-readable string that can be shown in UI
         */
        public void onAuthenticationHelp(int helpCode, CharSequence helpString) { }

        /**
         * Called when an iris is recognized.
         * @param result An object containing authentication-related data
         */
        public void onAuthenticationSucceeded(AuthenticationResult result) { }

        /**
         * Called when an iris  is valid but not recognized.
         */
        public void onAuthenticationFailed() { }

    };

    /**
     * Callback structure provided to {@link IrisManager#enroll(long, EnrollmentCallback,
     * CancellationSignal, int). Users of {@link #IrisManager()}
     * must provide an implementation of this to {@link IrisManager#enroll(long,
     * CancellationSignal, int, EnrollmentCallback) for listening to iris events.
     *
     * @hide
     */
    public static abstract class EnrollmentCallback {
        /**
         * Called when an unrecoverable error has been encountered and the operation is complete.
         * No further callbacks will be made on this object.
         * @param errMsgId An integer identifying the error message
         * @param errString A human-readable error string that can be shown in UI
         */
        public void onEnrollmentError(int errMsgId, CharSequence errString) { }

        /**
         * Called when a recoverable error has been encountered during enrollment. The help
         * string is provided to give the user guidance for what went wrong, such as
         * "iris image not clear" or what they need to do next, such as
         * "move your eyes in the centre."
         * @param helpMsgId An integer identifying the error message
         * @param helpString A human-readable string that can be shown in UI
         */
        public void onEnrollmentHelp(int helpMsgId, CharSequence helpString) { }

        /**
         * Called as each enrollment step progresses. Enrollment is considered complete when
         * remaining reaches 0. This function will not be called if enrollment fails. See
         * {@link EnrollmentCallback#onEnrollmentError(int, CharSequence)}
         * @param remaining The number of remaining steps
         */
        public void onEnrollmentProgress(int progress) { }
    };

    /**
     * Callback structure provided to {@link IrisManager#remove(int). Users of
     * {@link #IrisManager()} may optionally provide an implementation of this to
     * {@link IrisManager#remove(int, int, RemovalCallback)} for listening to
     * iris template removal events.
     *
     * @hide
     */
    public static abstract class RemovalCallback {
        /**
         * Called when the given fingerprint can't be removed.
         * @param iris The Iris  that the call attempted to remove
         * @param errMsgId An associated error message id
         * @param errString An error message indicating why the fingerprint id can't be removed
         */
        public void onRemovalError(Iris iris, int errMsgId, CharSequence errString) { }

        /**
         * Called when a given iris is successfully removed.
         * @param iris the iris template that was removed.
         */
        public void onRemovalSucceeded(Iris iris) { }
    };

    /**
     * @hide
     */
    public static abstract class LockoutResetCallback {

        /**
         * Called when lockout period expired and clients are allowed to listen for fingerprint
         * again.
         */
        public void onLockoutReset() { }
    };

    /**
     * Request authentication of a crypto object. This call starts up the iris service
     * and starts scanning for an iris. It terminates when
     * {@link AuthenticationCallback#onAuthenticationError(int, CharSequence)} or
     * {@link AuthenticationCallback#onAuthenticationSucceeded(AuthenticationResult)} is called, at
     * which point the object is no longer valid. The operation can be canceled by using the
     * provided cancel object.
     *
     * @param crypto object associated with the call or null if none required.
     * @param cancel an object that can be used to cancel authentication
     * @param flags optional flags; should be 0
     * @param callback an object to receive authentication events
     * @param handler an optional handler to handle callback events
     *
     * @throws IllegalArgumentException if the crypto operation is not supported or is not backed
     *         by <a href="{@docRoot}training/articles/keystore.html">Android Keystore
     *         facility</a>.
     * @throws IllegalStateException if the crypto primitive is not initialized.
     */
    @RequiresPermission(USE_IRIS)
    public void authenticate(@Nullable CryptoObject crypto, @Nullable CancellationSignal cancel,
            int flags, @NonNull AuthenticationCallback callback, @Nullable Handler handler) {
        authenticate(crypto, cancel, flags, callback, handler, UserHandle.myUserId());
    }

    /**
     * Use the provided handler thread for events.
     * @param handler
     */
    private void useHandler(Handler handler) {
        if (handler != null) {
            mHandler = new MyHandler(handler.getLooper());
        } else if (mHandler.getLooper() != mContext.getMainLooper()){
            mHandler = new MyHandler(mContext.getMainLooper());
        }
    }

    /**
     * Per-user version
     * @hide
     */
    @RequiresPermission(USE_IRIS)
    public void authenticate(@Nullable CryptoObject crypto, @Nullable CancellationSignal cancel,
            int flags, @NonNull AuthenticationCallback callback, Handler handler, int userId) {
        if (callback == null) {
            throw new IllegalArgumentException("Must supply an authentication callback");
        }

        if (cancel != null) {
            if (cancel.isCanceled()) {
                Log.w(TAG, "authentication already canceled");
                return;
            } else {
                cancel.setOnCancelListener(new OnAuthenticationCancelListener(crypto));
            }
        }

        if (mService != null) try {
            useHandler(handler);
            mAuthenticationCallback = callback;
            mEnrollmentCallback = null;
            mRemovalCallback = null;
            mCryptoObject = crypto;
            long sessionId = (crypto != null) ? crypto.getOpId() : 0;
            mService.authenticate(mToken, sessionId, userId, mServiceReceiver, flags,
                    mContext.getOpPackageName());
        } catch (RemoteException e) {
            Log.w(TAG, "Remote exception while authenticating: ", e);
            if (callback != null) {
                // Though this may not be a hardware issue, it will cause apps to give up or try
                // again later.
                callback.onAuthenticationError(IRIS_ERROR_SERVICE_NOT_AVAILABLE,
                        getErrorString(IRIS_ERROR_SERVICE_NOT_AVAILABLE));
            }
        }
    }

    /**
     * Request iris enrollment. This call starts up iris service
     * and starts scanning for iris. Progress will be indicated by callbacks to the
     * {@link EnrollmentCallback} object. It terminates when
     * {@link EnrollmentCallback#onEnrollmentError(int, CharSequence)} or
     * {@link EnrollmentCallback#onEnrollmentProgress(int) is called with progress == 100, at
     * which point the object is no longer valid. The operation can be canceled by using the
     * provided cancel object.
     * @param token a unique token provided by a recent creation or verification of device
     * credentials (e.g. pin, pattern or password).
     * @param cancel an object that can be used to cancel enrollment
     * @param flags optional flags
     * @param callback an object to receive enrollment events
     * @hide
     */
    @RequiresPermission(MANAGE_IRIS)
    public void enroll(byte [] token, CancellationSignal cancel, int flags,
            EnrollmentCallback callback) {
        if (callback == null) {
            throw new IllegalArgumentException("Must supply an enrollment callback");
        }

        if (cancel != null) {
            if (cancel.isCanceled()) {
                Log.w(TAG, "enrollment already canceled");
                return;
            } else {
                cancel.setOnCancelListener(new OnEnrollCancelListener());
            }
        }

        if (mService != null) try {
            mEnrollmentCallback = callback;
            mAuthenticationCallback = null;
            mRemovalCallback = null;
            mService.enroll(mToken, token, getCurrentUserId(), mServiceReceiver, flags);
        } catch (RemoteException e) {
            Log.w(TAG, "Remote exception in enroll: ", e);
            if (callback != null) {
                // Though this may not be a hardware issue, it will cause apps to give up or try
                // again later.
                callback.onEnrollmentError(IRIS_ERROR_SERVICE_NOT_AVAILABLE,
                        getErrorString(IRIS_ERROR_SERVICE_NOT_AVAILABLE));
            }
        }
    }

    /**
     * Requests a pre-enrollment auth token to tie enrollment to the confirmation of
     * existing device credentials (e.g. pin/pattern/password).
     * @hide
     */
    @RequiresPermission(MANAGE_IRIS)
    public long preEnroll() {
        long result = 0;
        if (mService != null) try {
            result = mService.preEnroll(mToken);
        } catch (RemoteException e) {
            Log.w(TAG, "Remote exception in enroll: ", e);
        }
        return result;
    }

    /**
     * Finishes enrollment and cancels the current auth token.
     * @hide
     */
    @RequiresPermission(MANAGE_IRIS)
    public int postEnroll() {
        int result = 0;
        if (mService != null) try {
            result = mService.postEnroll(mToken);
        } catch (RemoteException e) {
            Log.w(TAG, "Remote exception in post enroll: ", e);
        }
        return result;
    }

    /**
     * Remove given iris template from iris hardware and/or protected storage.
     * @param iris the Iris item to remove
     * @param callback an optional callback to verify that iris templates have been
     * successfully removed. May be null of no callback is required.
     *
     * @hide
     */
    @RequiresPermission(MANAGE_IRIS)
    public void remove(Iris iris, RemovalCallback callback) {
        if (mService != null) try {
            mRemovalCallback = callback;
            mEnrollmentCallback = null;
            mAuthenticationCallback = null;
            mRemovalIris = iris;
            mService.remove(mToken, iris.getIrisId(), getCurrentUserId(), mServiceReceiver);
        } catch (RemoteException e) {
            Log.w(TAG, "Remote exception in remove: ", e);
            if (callback != null) {
                callback.onRemovalError(iris, IRIS_ERROR_SERVICE_NOT_AVAILABLE,
                        getErrorString(IRIS_ERROR_SERVICE_NOT_AVAILABLE));
            }
        }
    }

    /**
     * Renames the given iris template
     * @param irisId the iris id
     * @param newName the new name
     *
     * @hide
     */
    @RequiresPermission(MANAGE_IRIS)
    public void rename(int irisId, String newName) {
        // Renames the given irisId
        if (mService != null) {
            try {
                mService.rename(irisId, getCurrentUserId(), newName);
            } catch (RemoteException e) {
                Log.v(TAG, "Remote exception in rename(): ", e);
            }
        } else {
            Log.w(TAG, "rename(): Service not connected!");
        }
    }

    /**
     * Obtain the list of enrolled iris templates.
     * @return list of current iris items
     *
     * @hide
     */
    @RequiresPermission(USE_IRIS)
    public List<Iris> getEnrolledIris(int userId) {
        if (mService != null) try {
            return mService.getEnrolledIris(userId, mContext.getOpPackageName());
        } catch (RemoteException e) {
            Log.v(TAG, "Remote exception in getEnrolledIris: ", e);
        }
        return null;
    }

    /**
     * Obtain the list of enrolled fingerprints templates.
     * @return list of current fingerprint items
     *
     * @hide
     */
    @RequiresPermission(USE_IRIS)
    public List<Iris> getEnrolledIris() {
        return getEnrolledIris(UserHandle.myUserId());
    }

    /**
     * Determine if there is at least one iris enrolled.
     *
     * @return true if at least one iris is enrolled, false otherwise
     */
    @RequiresPermission(USE_IRIS)
    public boolean hasEnrolledIris() {
        if (mService != null) try {
            return mService.hasEnrolledIris(UserHandle.myUserId(),
                    mContext.getOpPackageName());
        } catch (RemoteException e) {
            Log.v(TAG, "Remote exception in getEnrolledIris: ", e);
        }
        return false;
    }

    /**
     * Determine if iris service hardware is present and functional.
     *
     * @return true if hardware is present and functional, false otherwise.
     */
    @RequiresPermission(USE_IRIS)
    public boolean isHardwareDetected() {
        if (mService != null) {
            try {
                long deviceId = 0;
                return mService.isHardwareDetected(deviceId, mContext.getOpPackageName());
            } catch (RemoteException e) {
                Log.v(TAG, "Remote exception in isFingerprintHardwareDetected(): ", e);
            }
        } else {
            Log.w(TAG, "isFingerprintHardwareDetected(): Service not connected!");
        }
        return false;
    }

    /**
     * Retrieves the authenticator token for binding keys to the lifecycle
     * of the current set of iris. Used only by internal clients.
     *
     * @hide
     */
    public long getAuthenticatorId() {
        if (mService != null) {
            try {
                return mService.getAuthenticatorId(mContext.getOpPackageName());
            } catch (RemoteException e) {
                Log.v(TAG, "Remote exception in getAuthenticatorId(): ", e);
            }
        } else {
            Log.w(TAG, "getAuthenticatorId(): Service not connected!");
        }
        return 0;
    }

    /**
     * Reset the lockout timer when asked to do so by keyguard.
     *
     * @param token an opaque token returned by password confirmation.
     *
     * @hide
     */
    public void resetTimeout(byte[] token) {
        if (mService != null) {
            try {
                mService.resetTimeout(token);
            } catch (RemoteException e) {
                Log.v(TAG, "Remote exception in resetTimeout(): ", e);
            }
        } else {
            Log.w(TAG, "resetTimeout(): Service not connected!");
        }
    }

    /**
     * @hide
     */
    public void addLockoutResetCallback(final LockoutResetCallback callback) {
        if (mService != null) {
            try {
                mService.addLockoutResetCallback(
                        new IIrisServiceLockoutResetCallback.Stub() {

                    @Override
                    public void onLockoutReset(long deviceId) throws RemoteException {
                        mHandler.post(new Runnable() {
                            @Override
                            public void run() {
                                callback.onLockoutReset();
                            }
                        });
                    }
                });
            } catch (RemoteException e) {
                Log.v(TAG, "Remote exception in addLockoutResetCallback(): ", e);
            }
        } else {
            Log.w(TAG, "addLockoutResetCallback(): Service not connected!");
        }
    }

    private class MyHandler extends Handler {
        private MyHandler(Context context) {
            super(context.getMainLooper());
        }

        private MyHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(android.os.Message msg) {
            switch(msg.what) {
                case MSG_ENROLL_RESULT:
                    sendEnrollResult((Iris) msg.obj, msg.arg1 /* progress */, msg.arg2);
                    break;
                case MSG_AUTHENTICATION_SUCCEEDED:
                    sendAuthenticatedSucceeded((Iris) msg.obj);
                    break;
                case MSG_AUTHENTICATION_FAILED:
                    sendAuthenticatedFailed();
                    break;
                case MSG_ERROR:
                    sendErrorResult((Long) msg.obj /* deviceId */, msg.arg1 /* errMsgId */);
                    break;
                case MSG_REMOVED:
                    sendRemovedResult((Long) msg.obj /* deviceId */, msg.arg1 /* irisId */,
                            msg.arg2 /* groupId */);
                    break;
                case MSG_AUTHENTICATION_PROGRESS:
                    sendAuthenticateProgress(msg.arg1);
                    break;

            }
        }

        private void sendRemovedResult(long deviceId, int irisId, int groupId) {
            if (mRemovalCallback != null) {
                int reqIrisId = mRemovalIris.getIrisId();
                int reqGroupId = mRemovalIris.getGroupId();
                if (irisId != reqIrisId) {
                    Log.w(TAG, "Iris id didn't match: " + irisId + " != " + reqIrisId);
                }
                if (groupId != reqGroupId) {
                    Log.w(TAG, "Group id didn't match: " + groupId + " != " + reqGroupId);
                }
                mRemovalCallback.onRemovalSucceeded(mRemovalIris);
            }
        }

        private void sendErrorResult(long deviceId, int errMsgId) {
            if (mEnrollmentCallback != null) {
                mEnrollmentCallback.onEnrollmentError(errMsgId, getErrorString(errMsgId));
            } else if (mAuthenticationCallback != null) {
                mAuthenticationCallback.onAuthenticationError(errMsgId, getErrorString(errMsgId));
            } else if (mRemovalCallback != null) {
                mRemovalCallback.onRemovalError(mRemovalIris, errMsgId,
                        getErrorString(errMsgId));
            }
        }

        private void sendEnrollResult(Iris iris, int progress, int quality) {
            if (mEnrollmentCallback != null) {
                Slog.v(TAG, "IrisManager enroll progress " + progress +" quality " + quality);
                if (quality > IRIS_HELP_NONE && quality <= IRIS_HELP_IR_SENSOR_DONE)
                    mEnrollmentCallback.onEnrollmentHelp(quality, getHelpString(quality));

                 mEnrollmentCallback.onEnrollmentProgress(progress);
            }
        }

        private void sendAuthenticatedSucceeded(Iris iris) {
            if (mAuthenticationCallback != null) {
                final AuthenticationResult result = new AuthenticationResult(mCryptoObject, iris);
                mAuthenticationCallback.onAuthenticationSucceeded(result);
            }
        }

        private void sendAuthenticatedFailed() {
            if (mAuthenticationCallback != null) {
               mAuthenticationCallback.onAuthenticationFailed();
            }
        }

        private void sendAuthenticateProgress(int quality) {
            if (mAuthenticationCallback != null) {
                 if (quality > IRIS_HELP_NONE && quality <= IRIS_HELP_IR_SENSOR_DONE)
                    mAuthenticationCallback.onAuthenticationHelp(quality, getHelpString(quality));
            }
        }

    };

    /**
     * @hide
     */
    public IrisManager(Context context, IIrisService service) {
        mContext = context;
        mService = service;
        if (mService == null) {
            Slog.v(TAG, "IrisManagerService was null");
        }
        mHandler = new MyHandler(context);
    }

    private int getCurrentUserId() {
        try {
            return ActivityManagerNative.getDefault().getCurrentUser().id;
        } catch (RemoteException e) {
            Log.w(TAG, "Failed to get current user id\n");
            return UserHandle.USER_NULL;
        }
    }

    private void cancelEnrollment() {
        if (mService != null) try {
            mService.cancelEnrollment(mToken);
        } catch (RemoteException e) {
            if (DEBUG) Log.w(TAG, "Remote exception while canceling enrollment");
        }
    }

    private void cancelAuthentication(CryptoObject cryptoObject) {
        if (mService != null) try {
            mService.cancelAuthentication(mToken, mContext.getOpPackageName());
        } catch (RemoteException e) {
            if (DEBUG) Log.w(TAG, "Remote exception while canceling enrollment");
        }
    }

    private String getErrorString(int errMsg) {
        switch (errMsg) {
            case IRIS_ERROR_SERVICE_NOT_AVAILABLE :
                return mContext.getString(
                    com.android.internal.R.string.iris_error_service_not_available);
            case IRIS_ERROR_TIMEOUT:
                return mContext.getString(com.android.internal.R.string.iris_error_timeout);
            case IRIS_ERROR_CANCELED:
                return mContext.getString(com.android.internal.R.string.iris_error_canceled);
            case IRIS_ERROR_NO_SPACE:
                return mContext.getString(
                    com.android.internal.R.string.iris_error_no_space);
            case IRIS_ERROR_NO_USER:
                return mContext.getString(
                    com.android.internal.R.string.iris_error_no_user);
            case IRIS_ERROR_NO_IRIS:
                return mContext.getString(
                    com.android.internal.R.string.iris_error_no_iris);
            case IRIS_ERROR_GENERIC:
                return mContext.getString(
                    com.android.internal.R.string.iris_error_generic);
            case IRIS_ERROR_FATAL:
                return mContext.getString(
                    com.android.internal.R.string.iris_error_fatal);
            case IRIS_ERROR_IR_SENSOR:
                return mContext.getString(
                    com.android.internal.R.string.iris_error_ir_sensor);
            case IRIS_ERROR_BAD_QUALITY:
                return mContext.getString(
                    com.android.internal.R.string.iris_error_bad_quality);
            case IRIS_ERROR_LOCKOUT:
                return mContext.getString(com.android.internal.R.string.iris_error_lockout);
            default:
                if (errMsg >= IRIS_ERROR_VENDOR_BASE) {
                    int msgNumber = errMsg - IRIS_ERROR_VENDOR_BASE;
                    String[] msgArray = mContext.getResources().getStringArray(
                            com.android.internal.R.array.iris_error_vendor);
                    if (msgNumber < msgArray.length) {
                        return msgArray[msgNumber];
                    }
                }
                return null;
        }
    }

    private String getHelpString(int errMsg) {
         switch (errMsg) {
            case IRIS_HELP_NONE:
                return "";
            case IRIS_HELP_BAD_IMAGE_QUALITY:
                return mContext.getString(com.android.internal.R.string.iris_help_bad_image_quality);
            case IRIS_HELP_EYE_NOT_FOUND:
                return mContext.getString(
                     com.android.internal.R.string.iris_help_eye_not_found);
            case IRIS_HELP_EYE_TOO_CLOSE:
                return mContext.getString(
                     com.android.internal.R.string.iris_help_eye_too_close);
            case IRIS_HELP_EYE_TOO_FAR:
                return mContext.getString(
                     com.android.internal.R.string.iris_help_eye_too_far);
            case IRIS_HELP_EYE_TOO_UP:
                return mContext.getString(
                     com.android.internal.R.string.iris_help_eye_too_up);
            case IRIS_HELP_EYE_TOO_DOWN:
                return mContext.getString(
                     com.android.internal.R.string.iris_help_eye_too_down);
            case IRIS_HELP_EYE_TOO_LEFT:
                    return mContext.getString(
                     com.android.internal.R.string.iris_help_eye_too_left);
            case IRIS_HELP_EYE_TOO_RIGHT:
                return mContext.getString(
                     com.android.internal.R.string.iris_help_eye_too_right);
            case IRIS_HELP_MOTION_BLUR:
                return mContext.getString(
                     com.android.internal.R.string.iris_help_motion_blur);
            case IRIS_HELP_FOCUS_BLUR:
                return mContext.getString(
                     com.android.internal.R.string.iris_help_focus_blur);
            case IRIS_HELP_BAD_EYE_OPENNESS:
                return mContext.getString(
                     com.android.internal.R.string.iris_help_bad_eye_openness);
            case IRIS_HELP_BAD_EYE_DISTANCE:
                return mContext.getString(
                     com.android.internal.R.string.iris_help_bad_eye_distance);
            case IRIS_HELP_WITH_GLASS:
                return mContext.getString(
                     com.android.internal.R.string.iris_help_with_glass);
            case IRIS_HELP_IR_SENSOR_DONE:
                return mContext.getString(
                     com.android.internal.R.string.iris_help_ir_sensor_done);
             default:
                 return null;
         }
     }

    private IIrisServiceReceiver mServiceReceiver = new IIrisServiceReceiver.Stub() {

        @Override // binder call
        public void onEnrollResult(long deviceId, int irisId, int groupId, int progress, int quality) {
            mHandler.obtainMessage(MSG_ENROLL_RESULT, progress, quality,
                    new Iris(null, groupId, irisId, 1)).sendToTarget();
        }

        @Override // binder call
        public void onAuthenticationSucceeded(long deviceId, Iris iris) {
            mHandler.obtainMessage(MSG_AUTHENTICATION_SUCCEEDED, iris).sendToTarget();
        }

        @Override // binder call
        public void onAuthenticationFailed(long deviceId) {
            mHandler.obtainMessage(MSG_AUTHENTICATION_FAILED).sendToTarget();
        }

        @Override // binder call
        public void onError(long deviceId, int error) {
            mHandler.obtainMessage(MSG_ERROR, error, 0, deviceId).sendToTarget();
        }

        @Override // binder call
        public void onRemoved(long deviceId, int irisId, int groupId) {
            mHandler.obtainMessage(MSG_REMOVED, irisId, groupId, deviceId).sendToTarget();
        }

        @Override // binder call
        public void onAuthenticationProgress(long deviceId, int quality) {
            mHandler.obtainMessage(MSG_AUTHENTICATION_PROGRESS, quality, 0).sendToTarget();
        }

    };

}

