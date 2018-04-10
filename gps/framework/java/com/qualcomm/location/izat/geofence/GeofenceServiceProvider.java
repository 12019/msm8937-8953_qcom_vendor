/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
  Copyright (c) 2015 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

package com.qualcomm.location.izat.geofence;

import android.content.Intent;
import android.location.Location;
import android.os.IBinder;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.util.Log;

public class GeofenceServiceProvider {
    private static final String TAG = "GeofenceServiceProvider";
    private static final boolean VERBOSE_DBG = Log.isLoggable(TAG, Log.VERBOSE);
    private static final int GEOFENCE_RESULT_SUCCESS = 0;
    private static final int GEOFENCE_RESULT_ERROR_TOO_MANY_GEOFENCES = -100;
    private static final int GEOFENCE_RESULT_ERROR_ID_EXISTS = -101;
    private static final int GEOFENCE_RESULT_ERROR_ID_UNKNOWN = -102;
    private static final int GEOFENCE_RESULT_ERROR_INVALID_TRANSITION = -103;
    private static final int GEOFENCE_RESULT_ERROR_GENERIC = -149;

    private static final int GEOFENCE_REQUEST_TYPE_ADD = 1;
    private static final int GEOFENCE_REQUEST_TYPE_REMOVE = 2;
    private static final int GEOFENCE_REQUEST_TYPE_PAUSE = 3;
    private static final int GEOFENCE_REQUEST_TYPE_RESUME = 4;
    private static int sGeofenceId = 1;
    private static final Object sCallBacksLock = new Object();

    private RemoteCallbackList<IGeofenceCallback> mGeofenceCallbacks
        = new RemoteCallbackList<IGeofenceCallback>();

    public static GeofenceServiceProvider sInstance = null;
    public static GeofenceServiceProvider getInstance() {
        if (sInstance == null) {
            sInstance = new GeofenceServiceProvider();
        }
        return sInstance;
    }

    public GeofenceServiceProvider() {
        if (VERBOSE_DBG)
            Log.d(TAG, "GeofenceServiceProvider construction");
        native_geofence_init();
    }

    /* Remote binder */
    private final IGeofenceService.Stub mBinder = new IGeofenceService.Stub() {

        public void registerCallback(final IGeofenceCallback callback) {
            if (callback == null) {
                Log.e(TAG, "callback is null");
                return;
            }
            synchronized (sCallBacksLock) {
                mGeofenceCallbacks.register(callback);
            }
            try {
                callback.asBinder().linkToDeath(new IBinder.DeathRecipient() {
                    @Override
                    public void binderDied() {
                        synchronized (sCallBacksLock) {
                            mGeofenceCallbacks.unregister(callback);
                        }
                    }
                }, 0);
            } catch (RemoteException e) {
                throw new RuntimeException("Failed unregister geofence cb", e);
            }
        }

        public void unregisterCallback(IGeofenceCallback callback) {
            if (callback == null) {
                Log.e(TAG, "callback is null");
                return;
            }
            synchronized (sCallBacksLock) {
                mGeofenceCallbacks.unregister(callback);
            }
        }

        public int addGeofence(double latitude,
                               double longitude,
                               double radius, // in meters
                               int transitionTypes,
                               int responsiveness, // in milliseconds
                               int confidence,
                               int dwellTime, // in seconds
                               int dwellTimeMask) {

            final int geofenceId = sGeofenceId++;
            if (VERBOSE_DBG) {
                Log.d(TAG, "in IGeofenceService.Stub(): addGeofence()" +
                           "; geofenceId is " + geofenceId +
                           "; latitude is " + latitude +
                           "; longitude is " + longitude +
                           "; radius is " + radius +
                           "; transitionTypes is " + transitionTypes +
                           "; responsiveness is " + responsiveness +
                           "; confidence is " + confidence +
                           "; dwellTime is " + dwellTime +
                           "; dwellTimeMask is " + dwellTimeMask);
            }
            native_add_geofence(geofenceId, latitude, longitude, radius,
                                transitionTypes, responsiveness, confidence,
                                dwellTime, dwellTimeMask);
            return geofenceId;
        }

        public void removeGeofence(int geofenceId) {
            if (VERBOSE_DBG) {
                Log.d(TAG, "in IGeofenceService.Stub(): removeGeofence()" +
                           "; geofenceId is " + geofenceId);
            }
            native_remove_geofence(geofenceId);
        }

        public void updateGeofence(int geofenceId,
                                   int transitionTypes,
                                   int notifyResponsiveness) {
            if (VERBOSE_DBG) {
                Log.d(TAG, "in IGeofenceService.Stub(): updateGeofence()" +
                           "; geofenceId is " + geofenceId);
            }
            native_update_geofence(geofenceId,
                                   transitionTypes,
                                   notifyResponsiveness);
        }

        public void pauseGeofence(int geofenceId) {
            if (VERBOSE_DBG) {
                Log.d(TAG, "in IGeofenceService.Stub(): pauseGeofence()" +
                           "; geofenceId is " + geofenceId);
            }
            native_pause_geofence(geofenceId);
        }

        public void resumeGeofence(int geofenceId,
                                   int transitionTypes) {
            if (VERBOSE_DBG) {
                Log.d(TAG, "in IGeofenceService.Stub(): resumeGeofence()" +
                           "; geofenceId is " + geofenceId);
            }
            native_resume_geofence(geofenceId, transitionTypes);
        }
    };

    private void reportGeofenceTransition(int geofenceId,
                                          int transition,
                                          Location location) {
        if (VERBOSE_DBG)
            Log.d(TAG, "reportGeofenceTransition id : " + geofenceId +
                       "; transition : " + transition);
        synchronized (sCallBacksLock) {
            int index = mGeofenceCallbacks.beginBroadcast();
            for (int i = 0; i < index; i++) {
                try {
                    mGeofenceCallbacks.getBroadcastItem(i).onTransitionEvent(geofenceId,
                                                                             transition,
                                                                             location);
                } catch (RemoteException e) {
                    // do nothing
                }
            }
            mGeofenceCallbacks.finishBroadcast();
        }
    }

    private void reportGeofenceRequestStatus(int requestType,
                                             int geofenceId,
                                             int status) {
        if (VERBOSE_DBG)
            Log.d(TAG, "reportGeofenceRequestStatus requestType: " +
                       requestType +"; id : " + geofenceId +
                       "; status : " + status);
        synchronized (sCallBacksLock) {
            int index = mGeofenceCallbacks.beginBroadcast();
            for (int i = 0; i < index; i++) {
                try {
                    mGeofenceCallbacks.getBroadcastItem(i).onRequestResultReturned(geofenceId,
                                                                                   requestType,
                                                                                   status);
                } catch (RemoteException e) {
                    // do nothing
                }
            }
            mGeofenceCallbacks.finishBroadcast();
        }
    }

    private void reportGeofenceStatus(int status,
                                      Location location) {
        if (VERBOSE_DBG)
            Log.d(TAG, "reportGeofenceStatus - status : " + status);
        synchronized (sCallBacksLock) {
            int index = mGeofenceCallbacks.beginBroadcast();
            for (int i = 0; i < index; i++) {
                try {
                    mGeofenceCallbacks.getBroadcastItem(i).onEngineReportStatus(status, location);
                } catch (RemoteException e) {
                    // do nothing
                }
            }
            mGeofenceCallbacks.finishBroadcast();
        }
    }

    public IGeofenceService getGeofenceBinder() {
        return mBinder;
    }

    static {
        native_geofence_class_init();
    }

    private static native void native_geofence_class_init();
    private native void native_geofence_init();
    private native void native_add_geofence(int geofenceId,
                                            double latitude,
                                            double longitude,
                                            double radius,
                                            int transitionTypes,
                                            int notificationResponsivenes,
                                            int confidence,
                                            int dwellTime,
                                            int dwellTimeMask);
    private native void native_remove_geofence(int geofenceId);
    private native void native_update_geofence(int geofenceId,
                                               int transitionTypes,
                                               int notifyResponsiveness);
    private native void native_pause_geofence(int geofenceId);
    private native void native_resume_geofence(int geofenceId,
                                               int transitionTypes);
}
