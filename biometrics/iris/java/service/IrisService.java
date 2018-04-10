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

package com.android.server.iris;

import android.Manifest;
import android.app.ActivityManager;
import android.app.ActivityManager.RunningAppProcessInfo;
import android.app.ActivityManager.RunningTaskInfo;
import android.app.ActivityManagerNative;
import android.app.AppOpsManager;
import android.app.IUserSwitchObserver;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.UserInfo;
import android.hardware.iris.IIrisServiceLockoutResetCallback;
import android.os.Binder;
import android.os.DeadObjectException;
import android.os.Environment;
import android.os.Handler;
import android.os.IBinder;
import android.os.IRemoteCallback;
import android.os.PowerManager;
import android.os.RemoteException;
import android.os.SELinux;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.Slog;
import java.util.List;
import java.util.ArrayList;

import com.android.internal.logging.MetricsLogger;
import com.android.server.SystemService;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.hardware.iris.IrisManager;
import android.hardware.iris.Iris;
import android.hardware.iris.IrisIdentifier;
import android.hardware.iris.IIrisService;
import android.hardware.iris.IIrisDaemon;
import android.hardware.iris.IIrisDaemonCallback;
import android.hardware.iris.IIrisServiceReceiver;


import static android.app.ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND;

import static android.Manifest.permission.MANAGE_IRIS;
import static android.Manifest.permission.USE_IRIS;

import java.io.File;
import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.NoSuchElementException;

/**
 * A service to manage multiple clients that want to access the iris API.
 * The service is responsible for maintaining a list of clients and dispatching all
 * iris -related events.
 *
 * @hide
 */
public class IrisService extends SystemService implements IBinder.DeathRecipient {
    private static final String TAG = "IrisService";
    private static final boolean DEBUG = true;
    private static final String IRIS_DATA_DIR = "irisdata";
    private static final String IRISD = "android.hardware.iris.IIrisDaemon";
    private static final int MSG_USER_SWITCHING = 10;
    private static final int ENROLLMENT_TIMEOUT_MS = 20 * 1000;

    private ClientMonitor mAuthClient = null;
    private ClientMonitor mEnrollClient = null;
    private ClientMonitor mRemoveClient = null;
    private final ArrayList<IrisServiceLockoutResetMonitor> mLockoutMonitors =
            new ArrayList<>();
    private final AppOpsManager mAppOps;

    private static final long MS_PER_SEC = 1000;
    private static final long FAIL_LOCKOUT_TIMEOUT_MS = 30*1000;
    private static final int MAX_FAILED_ATTEMPTS = 5;
    private final String mKeyguardPackage;
    public static final int IRIS_ERROR_FATAL = 8;

    Handler mHandler = new Handler() {
        @Override
        public void handleMessage(android.os.Message msg) {
            switch (msg.what) {
                case MSG_USER_SWITCHING:
                    handleUserSwitching(msg.arg1);
                    break;

                default:
                    Slog.w(TAG, "Unknown message:" + msg.what);
            }
        }
    };

    private final IrisUtils mIrisUtils = IrisUtils.getInstance();
    private Context mContext;
    private long mHalDeviceId;
    private int mFailedAttempts;
    private IIrisDaemon mDaemon;

    private final Runnable mLockoutReset = new Runnable() {
        @Override
        public void run() {
            resetFailedAttempts();
        }
    };

    public IrisService(Context context) {
        super(context);
        mContext = context;
        mKeyguardPackage = ComponentName.unflattenFromString(context.getResources().getString(
                com.android.internal.R.string.config_keyguardComponent)).getPackageName();
        mAppOps = context.getSystemService(AppOpsManager.class);
    }

    @Override
    public void binderDied() {
        Slog.v(TAG, "iris deamon died");
        mDaemon = null;
        handleError(mHalDeviceId, IrisManager.IRIS_ERROR_SERVICE_NOT_AVAILABLE);
    }

    public IIrisDaemon getIrisDaemon() {
        if (mDaemon == null) {
            mDaemon = IIrisDaemon.Stub.asInterface(ServiceManager.getService(IRISD));
            if (mDaemon != null) {
                try {
                    mDaemon.asBinder().linkToDeath(this, 0);
                    mDaemon.init(mDaemonCallback);
                    mHalDeviceId = mDaemon.openHal();
                    if (mHalDeviceId != 0) {
                        updateActiveGroup(ActivityManager.getCurrentUser());
                        syncEnrollment();
                    } else {
                        Slog.w(TAG, "Failed to open iris HAL!");
                        mDaemon = null;
                    }
                } catch (RemoteException e) {
                    Slog.e(TAG, "Failed to open irisd HAL", e);
                    mDaemon = null; // try again later!
                }
            } else {
                Slog.w(TAG, "iris daemon not available");
            }
        }
        return mDaemon;
    }

    protected void handleEnumerate(long deviceId, int[] irisIds, int[] groupIds) {
        if (irisIds.length != groupIds.length) {
            Slog.w(TAG, "irisIds and groupIds differ in length: f[]="
                    + irisIds + ", g[]=" + groupIds);
            return;
        }
        if (DEBUG) Slog.w(TAG, "Enumerate: iris[]=" + irisIds + ", g[]=" + groupIds);
    }

    protected void handleRemoved(long deviceId, int irisId, int groupId) {
        final ClientMonitor client = mRemoveClient;
        if (irisId != 0) {
            removeTemplateForUser(mRemoveClient, irisId);
        }
        if (client != null && client.sendRemoved(irisId, groupId)) {
            removeClient(mRemoveClient);
        }
    }

    protected void handleError(long deviceId, int error) {
        boolean resetDaemon = false;
        if (error == IRIS_ERROR_FATAL) {
            Slog.v(TAG, "iris deamon fatal error, try to recover");
            resetDaemon = true;
        }

        if (mEnrollClient != null) {
            final IBinder token = mEnrollClient.token;
            if (mEnrollClient.sendError(error)) {
                stopEnrollment(token, false);
            }
        } else if (mAuthClient != null) {
            final IBinder token = mAuthClient.token;
            if (mAuthClient.sendError(error)) {
                stopAuthentication(token, false);
            }
        } else if (mRemoveClient != null) {
            if (mRemoveClient.sendError(error)) removeClient(mRemoveClient);
        }

        if (resetDaemon) {
            mDaemon = null;
        }
    }

    protected void handleAuthenticated(long deviceId, int irisId, int groupId) {
        if (mAuthClient != null) {
            final IBinder token = mAuthClient.token;
            mAuthClient.sendAuthenticated(irisId, groupId);
            stopAuthentication(token, false);
            removeClient(mAuthClient);
        }
    }

    protected void handleEnrollResult(long deviceId, int irisId, int groupId, int progress, int quality) {
        if (mEnrollClient != null) {
            if (mEnrollClient.sendEnrollResult(irisId, groupId, progress, quality)) {
                if (progress == 100 && irisId != 0) {
                    addTemplateForUser(mEnrollClient, irisId);
                    removeClient(mEnrollClient);
                }
            }
        }
    }

    protected void handleAuthenticating(long deviceId, int quality) {
        if (mAuthClient != null) {
            mAuthClient.sendAuthenticating(quality);
        }
    }

    void handleUserSwitching(int userId) {
        updateActiveGroup(userId);
    }

    private void removeClient(ClientMonitor client) {
        if (client == null) return;
        client.destroy();
        if (client == mAuthClient) {
            mAuthClient = null;
        } else if (client == mEnrollClient) {
            mEnrollClient = null;
        } else if (client == mRemoveClient) {
            mRemoveClient = null;
        }
    }

    private boolean inLockoutMode() {
        return mFailedAttempts >= MAX_FAILED_ATTEMPTS;
    }

    private void resetFailedAttempts() {
        if (DEBUG && inLockoutMode()) {
            Slog.v(TAG, "Reset iris lockout");
        }
        mFailedAttempts = 0;
        // If we're asked to reset failed attempts externally (i.e. from Keyguard), the runnable
        // may still be in the queue; remove it.
        mHandler.removeCallbacks(mLockoutReset);
        notifyLockoutResetMonitors();
    }

    private boolean handleFailedAttempt(ClientMonitor clientMonitor) {
        mFailedAttempts++;
        if (inLockoutMode()) {
            // Failing multiple times will continue to push out the lockout time.
            mHandler.removeCallbacks(mLockoutReset);
            mHandler.postDelayed(mLockoutReset, FAIL_LOCKOUT_TIMEOUT_MS);
            if (clientMonitor != null
                    && !clientMonitor.sendError(IrisManager.IRIS_ERROR_LOCKOUT)) {
                Slog.w(TAG, "Cannot send lockout message to client");
            }
            return true;
        }
        return false;
    }

    private void removeTemplateForUser(ClientMonitor clientMonitor, int irisId) {
        mIrisUtils.removeIrisForUser(mContext, irisId, clientMonitor.userId);
    }

    private void addTemplateForUser(ClientMonitor clientMonitor, int irisId) {
        mIrisUtils.addIrisForUser(mContext, irisId, clientMonitor.userId);
    }

    void startEnrollment(IBinder token, byte[] cryptoToken, int groupId,
            IIrisServiceReceiver receiver, int flags, boolean restricted, int uid) {
        IIrisDaemon daemon = getIrisDaemon();
        if (daemon == null) {
            Slog.w(TAG, "enroll: no irisd!");
            return;
        }
        stopPendingOperations(true);
        mEnrollClient = new ClientMonitor(token, receiver, groupId, restricted);
        final int timeout = (int) (ENROLLMENT_TIMEOUT_MS / MS_PER_SEC);
        try {
            final int result = daemon.enroll(cryptoToken, groupId, timeout, uid);
            if (result != 0) {
                Slog.w(TAG, "startEnroll failed, result=" + result);
                handleError(mHalDeviceId, IrisManager.IRIS_ERROR_SERVICE_NOT_AVAILABLE);
            }
        } catch (RemoteException e) {
            Slog.e(TAG, "startEnroll failed", e);
        }
    }

    public long startPreEnroll(IBinder token) {
        IIrisDaemon daemon = getIrisDaemon();
        if (daemon == null) {
            Slog.w(TAG, "startPreEnroll: no irisd!");
            return 0;
        }
        try {
            return daemon.preEnroll();
        } catch (RemoteException e) {
            Slog.e(TAG, "startPreEnroll failed", e);
        }
        return 0;
    }

    public int startPostEnroll(IBinder token) {
        IIrisDaemon daemon = getIrisDaemon();
        if (daemon == null) {
            Slog.w(TAG, "startPostEnroll: no irisd!");
            return 0;
        }
        try {
            return daemon.postEnroll();
        } catch (RemoteException e) {
            Slog.e(TAG, "startPostEnroll failed", e);
        }
        return 0;
    }

    private void stopPendingOperations(boolean initiatedByClient) {
        if (mEnrollClient != null) {
            stopEnrollment(mEnrollClient.token, initiatedByClient);
        }
        if (mAuthClient != null) {
            stopAuthentication(mAuthClient.token, initiatedByClient);
        }
        // mRemoveClient is allowed to continue
    }

    /**
     * Stop enrollment in progress and inform client if they initiated it.
     *
     * @param token token for client
     * @param initiatedByClient if this call is the result of client action (e.g. calling cancel)
     */
    void stopEnrollment(IBinder token, boolean initiatedByClient) {
        IIrisDaemon daemon = getIrisDaemon();
        if (daemon == null) {
            Slog.w(TAG, "stopEnrollment: no iris daemon!");
            return;
        }
        final ClientMonitor client = mEnrollClient;
        if (client == null || client.token != token) return;
        if (initiatedByClient) {
            try {
                int result = daemon.cancelEnrollment();
                if (result != 0) {
                    Slog.w(TAG, "stopEnrollment failed, result = " + result);
                }
            } catch (RemoteException e) {
                Slog.e(TAG, "stopEnrollment failed", e);
            }
            client.sendError(IrisManager.IRIS_ERROR_CANCELED);
        }
        removeClient(mEnrollClient);
    }

    void startAuthentication(IBinder token, long opId, int groupId,
            IIrisServiceReceiver receiver, int flags, boolean restricted, int uid) {
        IIrisDaemon daemon = getIrisDaemon();
        if (daemon == null) {
            Slog.w(TAG, "startAuthentication: no iris daemon!");
            return;
        }
        stopPendingOperations(true);
        mAuthClient = new ClientMonitor(token, receiver, groupId, restricted);
        if (inLockoutMode()) {
            Slog.v(TAG, "In lockout mode; disallowing authentication");
            if (!mAuthClient.sendError(IrisManager.IRIS_ERROR_LOCKOUT)) {
                Slog.w(TAG, "Cannot send lock out message to client");
            }
            mAuthClient = null;
            return;
        }
        try {
            final int result = daemon.authenticate(opId, groupId, uid);
            if (result != 0) {
                Slog.w(TAG, "startAuthentication failed, result=" + result);
                handleError(mHalDeviceId, IrisManager.IRIS_ERROR_SERVICE_NOT_AVAILABLE);
            }
        } catch (RemoteException e) {
            Slog.e(TAG, "startAuthentication failed", e);
        }
    }

    /**
     * Stop authentication in progress and inform client if they initiated it.
     *
     * @param token token for client
     * @param initiatedByClient if this call is the result of client action (e.g. calling cancel)
     */
    void stopAuthentication(IBinder token, boolean initiatedByClient) {
        IIrisDaemon daemon = getIrisDaemon();
        if (daemon == null) {
            Slog.w(TAG, "stopAuthentication: no iris daemon!");
            return;
        }
        final ClientMonitor client = mAuthClient;
        if (client == null || client.token != token) return;
        if (initiatedByClient) {
            try {
                int result = daemon.cancelAuthentication();
                if (result != 0) {
                    Slog.w(TAG, "stopAuthentication failed, result=" + result);
                }
            } catch (RemoteException e) {
                Slog.e(TAG, "stopAuthentication failed", e);
            }
            client.sendError(IrisManager.IRIS_ERROR_CANCELED);
        }
        removeClient(mAuthClient);
    }

    void startRemove(IBinder token, int irisId, int userId,
            IIrisServiceReceiver receiver, boolean restricted) {
        IIrisDaemon daemon = getIrisDaemon();
        if (daemon == null) {
            Slog.w(TAG, "startRemove: no iris daemon!");
            return;
        }

        stopPendingOperations(true);
        mRemoveClient = new ClientMonitor(token, receiver, userId, restricted);
        // The iris template ids will be removed when we get confirmation from the HAL
        try {
            final int result = daemon.remove(irisId, userId);
            if (result != 0) {
                Slog.w(TAG, "startRemove with id = " + irisId + " failed, result=" + result);
                handleError(mHalDeviceId, IrisManager.IRIS_ERROR_SERVICE_NOT_AVAILABLE);
            }
        } catch (RemoteException e) {
            Slog.e(TAG, "startRemove failed", e);
        }
    }

    public List<Iris> getEnrolledIris(int userId) {
        return mIrisUtils.getIrisForUser(mContext, userId, false);
    }

    public boolean hasEnrolledIris(int userId) {
        return mIrisUtils.getIrisForUser(mContext, userId, false).size() > 0;
    }

    boolean hasPermission(String permission) {
        return getContext().checkCallingOrSelfPermission(permission)
                == PackageManager.PERMISSION_GRANTED;
    }

    void checkPermission(String permission) {
        getContext().enforceCallingOrSelfPermission(permission,
                "Must have " + permission + " permission.");
    }

    int getEffectiveUserId(int userId) {
        UserManager um = UserManager.get(mContext);
        if (um != null) {
            final long callingIdentity = Binder.clearCallingIdentity();
            userId = um.getCredentialOwnerProfile(userId);
            Binder.restoreCallingIdentity(callingIdentity);
        } else {
            Slog.e(TAG, "Unable to acquire UserManager");
        }
        return userId;
    }

    boolean isCurrentUserOrProfile(int userId) {
        UserManager um = UserManager.get(mContext);

        // Allow current user or profiles of the current user...
        List<UserInfo> profiles = um.getEnabledProfiles(userId);
        final int n = profiles.size();
        for (int i = 0; i < n; i++) {
            if (profiles.get(i).id == userId) {
                return true;
            }
        }
        return false;
    }

    private boolean isForegroundActivity(int uid, int pid) {
        try {
            List<RunningAppProcessInfo> procs =
                    ActivityManagerNative.getDefault().getRunningAppProcesses();
            int N = procs.size();
            for (int i = 0; i < N; i++) {
                RunningAppProcessInfo proc = procs.get(i);
                if (proc.pid == pid && proc.uid == uid
                        && proc.importance == IMPORTANCE_FOREGROUND) {
                    return true;
                }
            }
        } catch (RemoteException e) {
            Slog.w(TAG, "am.getRunningAppProcesses() failed");
        }
        return false;
    }

    /**
     * @param opPackageName name of package for caller
     * @param foregroundOnly only allow this call while app is in the foreground
     * @return true if caller can use iris API
     */
    private boolean canUseIris(String opPackageName, boolean foregroundOnly) {
        checkPermission(USE_IRIS);//todo
        final int uid = Binder.getCallingUid();
        final int pid = Binder.getCallingPid();
        if (opPackageName.equals(mKeyguardPackage)) {
            return true; // Keyguard is always allowed
        }
        if (!isCurrentUserOrProfile(UserHandle.getCallingUserId())) {
            Slog.w(TAG,"Rejecting " + opPackageName + " ; not a current user or profile");
            return false;
        }
        if (mAppOps.noteOp(AppOpsManager.OP_USE_FINGERPRINT, uid, opPackageName) //TODO
                != AppOpsManager.MODE_ALLOWED) {
            Slog.v(TAG, "Rejecting " + opPackageName + " ; permission denied");
            return false;
        }
        if (foregroundOnly && !isForegroundActivity(uid, pid)) {
            Slog.v(TAG, "Rejecting " + opPackageName + " ; not in foreground");
            return false;
        }
        return true;
    }

    private void syncEnrollment() {
        Slog.v(TAG, "syncEnrollment");
        if (mDaemon == null) {
            Slog.w(TAG, "syncEnrollment: no iris daemon!");
            return;
        }

        try {
            List<IrisIdentifier> ids = mDaemon.enumerateEnrollment();

            for (IrisIdentifier id : ids) {
                int groupId = id.getGroupId();
                int irisId = id.getIrisId();
                List<Iris> savedIds = mIrisUtils.getIrisForUser(mContext, groupId, true);
                boolean found = isValidIris(irisId, savedIds);
                if (!found) {
                    Slog.v(TAG, "recovering iris " + irisId);
                    mIrisUtils.addIrisForUser(mContext, irisId, groupId);
                } else {
                    Slog.v(TAG, "validating iris " + irisId);
                    mIrisUtils.validateIrisForUser(mContext, irisId, groupId);
                }
            }
        } catch (RemoteException e) {
            Slog.w(TAG, "syncEnrollment failed");
        }
    }


    private boolean isValidIris(int irisId, List<Iris> irislist) {
            boolean found = false;
            for (Iris temp : irislist) {
                if (temp.getIrisId() == irisId) {
                    found = true;
                    break;
                }
            }
            return found;
    }


    private void addLockoutResetMonitor(IrisServiceLockoutResetMonitor monitor) {
        if (!mLockoutMonitors.contains(monitor)) {
            mLockoutMonitors.add(monitor);
        }
    }

    private void removeLockoutResetCallback(
            IrisServiceLockoutResetMonitor monitor) {
        mLockoutMonitors.remove(monitor);
    }

    private void notifyLockoutResetMonitors() {
        for (int i = 0; i < mLockoutMonitors.size(); i++) {
            mLockoutMonitors.get(i).sendLockoutReset();
        }
    }

    private class ClientMonitor implements IBinder.DeathRecipient {
        IBinder token;
        IIrisServiceReceiver receiver;
        int userId;
        boolean restricted; // True if client does not have MANAGE_IRIS permission

        public ClientMonitor(IBinder token, IIrisServiceReceiver receiver, int userId,
                boolean restricted) {
            this.token = token;
            this.receiver = receiver;
            this.userId = userId;
            this.restricted = restricted;
            try {
                token.linkToDeath(this, 0);
            } catch (RemoteException e) {
                Slog.w(TAG, "caught remote exception in linkToDeath: ", e);
            }
        }

        public void destroy() {
            if (token != null) {
                try {
                    token.unlinkToDeath(this, 0);
                } catch (NoSuchElementException e) {
                    // TODO: remove when duplicate call bug is found
                    Slog.e(TAG, "destroy(): " + this + ":", new Exception("here"));
                }
                token = null;
            }
            receiver = null;
        }

        @Override
        public void binderDied() {
            token = null;
            removeClient(this);
            receiver = null;
        }

        @Override
        protected void finalize() throws Throwable {
            try {
                if (token != null) {
                    if (DEBUG) Slog.w(TAG, "removing leaked reference: " + token);
                    removeClient(this);
                }
            } finally {
                super.finalize();
            }
        }

        /*
         * @return true if we're done.
         */
        private boolean sendRemoved(int irisId, int groupId) {
            if (receiver == null) return true; // client not listening
            try {
                receiver.onRemoved(mHalDeviceId, irisId, groupId);
                return irisId == 0;
            } catch (RemoteException e) {
                Slog.w(TAG, "Failed to notify Removed:", e);
            }
            return false;
        }

        /*
         * @return true if we're done.
         */
        private boolean sendEnrollResult(int irisId, int groupId, int progress, int quality) {
            if (receiver == null) return true; // client not listening

            //MetricsLogger.action(mContext, MetricsLogger.ACTION_IRIS_ENROLL);
            try {
                receiver.onEnrollResult(mHalDeviceId, irisId, groupId, progress, quality);
                return progress == 100;
            } catch (RemoteException e) {
                Slog.w(TAG, "Failed to notify EnrollResult:", e);
                return true;
            }
        }

        /*
         * @return true if we're done.
         */
        private boolean sendAuthenticated(int irisId, int groupId) {
            boolean result = false;
            boolean authenticated = irisId != 0;
            if (receiver != null) {
                try {
                    //MetricsLogger.action(mContext, MetricsLogger.ACTION_IRIS_AUTH, authenticated);
                    if (!authenticated) {
                        receiver.onAuthenticationFailed(mHalDeviceId);
                    } else {
                     	Iris iris = !restricted ? new Iris("" /* TODO */, groupId, irisId, 1) : null;
                        receiver.onAuthenticationSucceeded(mHalDeviceId, iris);
                    }
                } catch (RemoteException e) {
                    Slog.w(TAG, "Failed to notify Authenticated:", e);
                    result = true; // client failed
                }
            } else {
                result = true; // client not listening
        }
        if (irisId == 0) {
                if (receiver != null) {
                    IrisUtils.vibrateIrisError(getContext());
                }
                result |= handleFailedAttempt(this);
        } else {
                if (receiver != null) {
                    IrisUtils.vibrateIrisSuccess(getContext());
                }
                result |= true; // we have a valid iris
                mHandler.post(mLockoutReset);
            }
            return result;
        }

        /*
         * @return true if we're done.
         */
        private boolean sendError(int error) {
            if (receiver != null) {
                try {
                    receiver.onError(mHalDeviceId, error);
                } catch (RemoteException e) {
                    Slog.w(TAG, "Failed to invoke sendError:", e);
                }
            }
            return true; // errors always terminate progress
        }


        /*
         * @return true if we're done.
         */
        private boolean sendAuthenticating(int quality) {
            if (receiver == null) return true; // client not listening
        
            //MetricsLogger.action(mContext, MetricsLogger.ACTION_IRIS_ENROLL);
            try {
                receiver.onAuthenticationProgress(mHalDeviceId, quality);
            } catch (RemoteException e) {
                Slog.v(TAG, "Failed to notify authenticating:", e);
            }

            return true;
        }

    }

    private class IrisServiceLockoutResetMonitor {

        private final IIrisServiceLockoutResetCallback mCallback;

        public IrisServiceLockoutResetMonitor(
                IIrisServiceLockoutResetCallback callback) {
            mCallback = callback;
        }

        public void sendLockoutReset() {
            if (mCallback != null) {
                try {
                    mCallback.onLockoutReset(mHalDeviceId);
                } catch (DeadObjectException e) {
                    Slog.w(TAG, "Death object while invoking onLockoutReset: ", e);
                    mHandler.post(mRemoveCallbackRunnable);
                } catch (RemoteException e) {
                    Slog.w(TAG, "Failed to invoke onLockoutReset: ", e);
                }
            }
        }

        private final Runnable mRemoveCallbackRunnable = new Runnable() {
            @Override
            public void run() {
                removeLockoutResetCallback(IrisServiceLockoutResetMonitor.this);
            }
        };
    }

    private IIrisDaemonCallback mDaemonCallback = new IIrisDaemonCallback.Stub() {

        @Override
        public void onEnrollResult(final long deviceId, final int irisId, final int groupId,
                final int progress, final int quality) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    handleEnrollResult(deviceId, irisId, groupId, progress, quality);
                }
            });
        }

        @Override
        public void onAuthenticated(final long deviceId, final int irisId, final int groupId) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    handleAuthenticated(deviceId, irisId, groupId);
                }
            });
        }

        @Override
        public void onError(final long deviceId, final int error) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    handleError(deviceId, error);
                }
            });
        }

        @Override
        public void onRemoved(final long deviceId, final int irisId, final int groupId) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    handleRemoved(deviceId, irisId, groupId);
                }
            });
        }

        @Override
        public void onEnumerate(final long deviceId, final int[] irisIds, final int[] groupIds) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    handleEnumerate(deviceId, irisIds, groupIds);
                }
            });
        }

        @Override
        public void  onAuthenticating(final long deviceId, final int quality) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    handleAuthenticating(deviceId, quality);
                }
            });
        }

    };

    private final class IrisServiceWrapper extends IIrisService.Stub {
        @Override // Binder call
        public long preEnroll(IBinder token) {
            checkPermission(MANAGE_IRIS);
            return startPreEnroll(token);
        }

        @Override // Binder call
        public int postEnroll(IBinder token) {
            checkPermission(MANAGE_IRIS);
            return startPostEnroll(token);
        }

        @Override // Binder call
        public void enroll(final IBinder token, final byte[] cryptoToken, final int groupId,
                final IIrisServiceReceiver receiver, final int flags) {
            //todo
            checkPermission(MANAGE_IRIS);
            final int limit =  mContext.getResources().getInteger(
                    com.android.internal.R.integer.config_irisMaxTemplatesPerUser);
            final int callingUid = Binder.getCallingUid();
            final int userId = UserHandle.getUserId(callingUid);
            final int enrolled = IrisService.this.getEnrolledIris(userId).size();
            if (enrolled >= limit) {
                Slog.w(TAG, "Too many iris templates registered");
                try {
                    receiver.onError(mHalDeviceId, IrisManager.IRIS_ERROR_NO_SPACE);
                } catch (RemoteException e) {
                    Slog.w(TAG, "Failed to invoke sendError:", e);
                }
                return;
            }
            final byte [] cryptoClone = Arrays.copyOf(cryptoToken, cryptoToken.length);

            // Group ID is arbitrarily set to parent profile user ID. It just represents
            // the default iris templates for the user.
            final int effectiveGroupId = getEffectiveUserId(groupId);

            final boolean restricted = isRestricted();
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    startEnrollment(token, cryptoClone, effectiveGroupId, receiver, flags, restricted, callingUid);
                }
            });
        }

        private boolean isRestricted() {
            // Only give privileged apps (like Settings) access to iris info
            final boolean restricted = !hasPermission(MANAGE_IRIS);
            return restricted;
        }

        @Override // Binder call
        public void cancelEnrollment(final IBinder token) {
            checkPermission(MANAGE_IRIS);
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    stopEnrollment(token, true);
                }
            });
        }

        @Override // Binder call
        public void authenticate(final IBinder token, final long opId, final int groupId,
                final IIrisServiceReceiver receiver, final int flags,
                final String opPackageName) {
            if (!canUseIris(opPackageName, true /* foregroundOnly */)) {
                return;
            }

            // Group ID is arbitrarily set to parent profile user ID. It just represents
            // the default iris for the user.
            final int effectiveGroupId = getEffectiveUserId(groupId);
            final int callingUid = Binder.getCallingUid();

            final boolean restricted = isRestricted();
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    MetricsLogger.histogram(mContext, "iris_token", opId != 0L ? 1 : 0);
                    startAuthentication(token, opId, effectiveGroupId, receiver, flags, restricted, callingUid);
                }
            });
        }

        @Override // Binder call
        public void cancelAuthentication(final IBinder token, String opPackageName) {
            if (!canUseIris(opPackageName, false /* foregroundOnly */)) {
                return;
            }
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    stopAuthentication(token, true);
                }
            });
        }

        @Override // Binder call
        public void remove(final IBinder token, final int irisId, final int groupId,
                final IIrisServiceReceiver receiver) {
            checkPermission(MANAGE_IRIS); // TODO: Maybe have another permission
            final boolean restricted = isRestricted();

            // Group ID is arbitrarily set to parent profile user ID. It just represents
            // the default iris for the user.
            final int effectiveGroupId = getEffectiveUserId(groupId);
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    startRemove(token, irisId, effectiveGroupId, receiver, restricted);
                }
            });

        }

        @Override // Binder call
        public boolean isHardwareDetected(long deviceId, String opPackageName) {
            if (!canUseIris(opPackageName, false /* foregroundOnly */)) {
                return false;
            }
            return mHalDeviceId != 0;
        }

        @Override // Binder call
        public void rename(final int irisId, final int groupId, final String name) {
            checkPermission(MANAGE_IRIS);

            // Group ID is arbitrarily set to parent profile user ID. It just represents
            // the default iris for the user.
            final int effectiveGroupId = getEffectiveUserId(groupId);
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    mIrisUtils.renameIrisForUser(mContext, irisId,
                            effectiveGroupId, name);
                }
            });
        }

        @Override // Binder call
        public List<Iris> getEnrolledIris(int userId, String opPackageName) {
            if (!canUseIris(opPackageName, false /* foregroundOnly */)) {
                return Collections.emptyList();
            }
            int effectiveUserId = getEffectiveUserId(userId);

            return IrisService.this.getEnrolledIris(effectiveUserId);
        }

        @Override // Binder call
        public boolean hasEnrolledIris(int userId, String opPackageName) {
            if (!canUseIris(opPackageName, false /* foregroundOnly */)) {
                return false;
            }

            int effectiveUserId  = getEffectiveUserId(userId);
            return IrisService.this.hasEnrolledIris(effectiveUserId);
        }

        @Override // Binder call
        public long getAuthenticatorId(String opPackageName) {
            // In this method, we're not checking whether the caller is permitted to use iris
            // API because current authenticator ID is leaked (in a more contrived way) via Android
            // Keystore (android.security.keystore package): the user of that API can create a key
            // which requires iris authentication for its use, and then query the key's
            // characteristics (hidden API) which returns, among other things, iris
            // authenticator ID which was active at key creation time.
            //
            // Reason: The part of Android Keystore which runs inside an app's process invokes this
            // method in certain cases. Those cases are not always where the developer demonstrates
            // explicit intent to use iris functionality. Thus, to avoiding throwing an
            // unexpected SecurityException this method does not check whether its caller is
            // permitted to use iris API.
            //
            // The permission check should be restored once Android Keystore no longer invokes this
            // method from inside app processes.

            return IrisService.this.getAuthenticatorId();
        }

        @Override // Binder call
        protected void dump(FileDescriptor fd, PrintWriter pw, String[] args) {
            if (mContext.checkCallingOrSelfPermission(Manifest.permission.DUMP)
                    != PackageManager.PERMISSION_GRANTED) {
                pw.println("Permission Denial: can't dump Fingerprint from from pid="
                        + Binder.getCallingPid()
                        + ", uid=" + Binder.getCallingUid());
                return;
            }

            final long ident = Binder.clearCallingIdentity();
            try {
                dumpInternal(pw);
            } finally {
                Binder.restoreCallingIdentity(ident);
            }
        }
        @Override // Binder call
        public void resetTimeout(byte [] token) {
            //checkPermission(RESET_FINGERPRINT_LOCKOUT);
            // TODO: confirm security token when we move timeout management into the HAL layer.
            mHandler.post(mLockoutReset);
        }

        @Override
        public void addLockoutResetCallback(final IIrisServiceLockoutResetCallback callback)
                throws RemoteException {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    addLockoutResetMonitor(
                            new IrisServiceLockoutResetMonitor(callback));
                }
            });
        }
    }

    private void dumpInternal(PrintWriter pw) {
        JSONObject dump = new JSONObject();
        try {
            dump.put("service", "Fingerprint Manager");

            JSONArray sets = new JSONArray();
            for (UserInfo user : UserManager.get(getContext()).getUsers()) {
                final int userId = user.getUserHandle().getIdentifier();
                JSONObject set = new JSONObject();
                set.put("id", userId);
                set.put("count", 1);
                sets.put(set);
            }

            dump.put("prints", sets);
        } catch (JSONException e) {
            Slog.e(TAG, "dump formatting failure", e);
        }
        pw.println(dump);
    }

    @Override
    public void onStart() {
        publishBinderService(Context.IRIS_SERVICE, new IrisServiceWrapper());
        IIrisDaemon daemon = getIrisDaemon();
        if (DEBUG) Slog.v(TAG, "Iris HAL id: " + mHalDeviceId);
        listenForUserSwitches();
    }

    private void updateActiveGroup(int userId) {
        IIrisDaemon daemon = getIrisDaemon();
        if (daemon != null) {
            try {
                userId = getEffectiveUserId(userId);
                final File systemDir = Environment.getUserSystemDirectory(userId);
                final File irisDir = new File(systemDir, IRIS_DATA_DIR);
                if (!irisDir.exists()) {
                    if (!irisDir.mkdir()) {
                        Slog.v(TAG, "Cannot make directory: " + irisDir.getAbsolutePath());
                        return;
                    }
                    // Calling mkdir() from this process will create a directory with our
                    // permissions (inherited from the containing dir). This command fixes
                    // the label.
                    if (!SELinux.restorecon(irisDir)) {
                        Slog.w(TAG, "Restorecons failed. Directory will have wrong label.");
                        return;
                    }
                }
                daemon.setActiveGroup(userId, irisDir.getAbsolutePath().getBytes());
            } catch (RemoteException e) {
                Slog.e(TAG, "Failed to setActiveGroup():", e);
            }
        }
    }

    private void listenForUserSwitches() {
        try {
            ActivityManagerNative.getDefault().registerUserSwitchObserver(
                new IUserSwitchObserver.Stub() {
                    @Override
                    public void onUserSwitching(int newUserId, IRemoteCallback reply) {
                        mHandler.obtainMessage(MSG_USER_SWITCHING, newUserId, 0 /* unused */)
                                .sendToTarget();
                    }
                    @Override
                    public void onUserSwitchComplete(int newUserId) throws RemoteException {
                        // Ignore.
                    }
                    @Override
                    public void onForegroundProfileSwitch(int newProfileId) {
                        // Ignore.
                    }
                });
        } catch (RemoteException e) {
            Slog.w(TAG, "Failed to listen for user switching event" ,e);
        }
    }

    public long getAuthenticatorId() {
        IIrisDaemon daemon = getIrisDaemon();
        if (daemon != null) {
            try {
                return daemon.getAuthenticatorId();
            } catch (RemoteException e) {
                Slog.e(TAG, "getAuthenticatorId failed", e);
            }
        }
        return 0;
    }

}
