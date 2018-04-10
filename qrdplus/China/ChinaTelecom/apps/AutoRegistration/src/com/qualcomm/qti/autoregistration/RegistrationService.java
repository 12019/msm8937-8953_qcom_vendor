/**
 * Copyright (c) 2014, Qualcomm Technologies, Inc. All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */

package com.qualcomm.qti.autoregistration;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.BroadcastReceiver;
import android.content.IntentFilter;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.UserHandle;
import android.os.ServiceManager;
import android.os.SystemProperties;
import android.os.RemoteException;
import android.preference.PreferenceManager;
import android.telephony.ServiceState;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Log;
import android.widget.Toast;

import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneConstants;
import com.android.internal.telephony.PhoneFactory;
import com.android.internal.telephony.TelephonyIntents;

import org.codeaurora.internal.IExtTelephony;

import java.util.List;

public class RegistrationService extends Service {

    private static final String TAG = "RegistrationService";
    private static final String BOOT_COMPLETE_FLAG = "boot_complete";
    private static final String MANUAL_REGISTRATION_FLAG = "manual";
    private static final boolean DBG = true;

    private static final long DELAY_REQUEST_AFTER_POWER_ON = 60 * 1000;
    private static final long INTERVAL_RESCHEDUAL = 60 * 60 * 1000;
    private static final int MAX_REQUEST_TIMES = 10;

    private static final String PREF_ICCID_ON_SUCCESS = "sim_iccid";
    private static final String PREF_REQUEST_COUNT = "register_request_count";
    private static final String PREF_WIFI_MAC = "wifi_macid";

    public static final String ACTION_AUTO_REGISTERATION = "com.qualcomm.action.AUTO_REGISTRATION";
    //China Telecomm Issuer Identification Number
    public static final String CT_IIN = "898603,898611,898612";

    private SharedPreferences mSharedPreferences;
    private AlarmManager mAlarmManager;
    private BroadcastReceiver mWifiStateChangeListener = null;
    private IExtTelephony mExtTelephony = IExtTelephony.Stub.
            asInterface(ServiceManager.getService("extphone"));
    private SubscriptionManager mSubscriptionManager;
    private ConnectivityManager mConnectivityManager;
    private boolean mRegistering = false;
    private boolean mPowerOn = true;
    private boolean mDelayBarrier = true;

    @Override
    public void onCreate() {
        super.onCreate();
        if (UserHandle.myUserId() != 0) {
            return;
        }
        mSharedPreferences = PreferenceManager.getDefaultSharedPreferences(this);
        mAlarmManager = (AlarmManager) getSystemService(Context.ALARM_SERVICE);
        mSubscriptionManager = SubscriptionManager.from(this);
        mConnectivityManager = (ConnectivityManager)
                getSystemService(Context.CONNECTIVITY_SERVICE);
        String mac =  mSharedPreferences.getString(PREF_WIFI_MAC, null);
        if(mac == null) {
           registerReceiver();
        }
        mSharedPreferences.edit().putInt(PREF_REQUEST_COUNT, 0).commit();
        monitorNetworkConnectionStatus();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && intent.getBooleanExtra(MANUAL_REGISTRATION_FLAG, false)
                && UserHandle.myUserId() == 0) {
            onRegistrationRquestManually();
        }
        return super.onStartCommand(intent, flags, startId);
    }

    private void registerReceiver() {
        mWifiStateChangeListener = new BroadcastReceiver() {
            public void onReceive(Context context, Intent intent) {
                String action = intent.getAction();
                if (action.equals(WifiManager.WIFI_STATE_CHANGED_ACTION)) {
                     if (intent.getIntExtra(WifiManager.EXTRA_WIFI_STATE, 0) == WifiManager.WIFI_STATE_ENABLED) {
                         getwifiMacAddress();
                     }
                }
            }
        };
        IntentFilter wifiStateChangeFilter = new IntentFilter(
               WifiManager.WIFI_STATE_CHANGED_ACTION);
        registerReceiver(mWifiStateChangeListener, wifiStateChangeFilter);
    }

    private void getwifiMacAddress(){
        WifiManager wifiManager = (WifiManager) getSystemService(Context.WIFI_SERVICE);
        WifiInfo wifiInfo = wifiManager.getConnectionInfo();
        String macAddress = wifiInfo == null ? null : wifiInfo.getMacAddress();
        mSharedPreferences.edit().putString(PREF_WIFI_MAC, macAddress)
                          .commit();
        if (mWifiStateChangeListener != null) {
            unregisterReceiver(mWifiStateChangeListener);
        }
    }

    private void monitorNetworkConnectionStatus() {
        BroadcastReceiver receiver = new BroadcastReceiver() {
            public void onReceive(Context context, Intent intent) {
                String action = intent.getAction();
                if (intent.getAction().equals(ConnectivityManager.CONNECTIVITY_ACTION)) {
                    tryToRegister(false);
                } else if (intent.getAction().equals(
                        TelephonyIntents.ACTION_SERVICE_STATE_CHANGED)) {
                    ServiceState state = ServiceState.newFromBundle(intent.getExtras());
                    if (state.getState() == ServiceState.STATE_IN_SERVICE) {
                        tryToRegister(true);
                    }
                }
            }
        };
        IntentFilter intentFilter = new IntentFilter(ConnectivityManager.CONNECTIVITY_ACTION);
        intentFilter.addAction(TelephonyIntents.ACTION_SERVICE_STATE_CHANGED);
        registerReceiver(receiver, intentFilter);
    }

    private void tryToRegister(boolean serviceChanged) {
        if (mConnectivityManager != null) {
            NetworkInfo info = mConnectivityManager.getActiveNetworkInfo();
            if (info != null && info.isConnected()) {
                // Once the SIM changed, try to register
                String iccIdText = getIccIdText();
                if (!iccIdText.equals(
                    mSharedPreferences.getString(PREF_ICCID_ON_SUCCESS, null))) {
                    cancelNextSchedule();
                    // if under wifi, need wait for in-service so that SID and baseId can be fetched
                    boolean underWifi = info.getType() == ConnectivityManager.TYPE_WIFI;
                    Log.d(TAG, "Try to register underWifi = " + underWifi +
                            " serviceChanged = " + serviceChanged);
                    if (serviceChanged || !underWifi || (underWifi && hasService())) {
                        onRegistrationRquest();
                    }
                }
            }
        }
    }

    private boolean hasService() {
        List<SubscriptionInfo> subList = mSubscriptionManager.getActiveSubscriptionInfoList();
        if (subList != null) {
            for (SubscriptionInfo subInfo : subList) {
                ServiceState state = TelephonyManager.getDefault()
                        .getServiceStateForSubscriber(subInfo.getSubscriptionId());
                if (state.getState() == ServiceState.STATE_IN_SERVICE) {
                    return true;
                }
            }
        }
        return false;
    }

    private void onRegistrationRquestManually() {
        int slotId = getCTSlotId();
        if (slotId < 0) {
            // Giving phone id on primary stack
            // so that MEID can be fetched
            slotId = getPrimaryStackPhoneId();
            Log.d(TAG, "Getting phone ID: " + slotId + "primary stack");
        }

        if (!isDDSOnCTSlotWithoutWifi()) {
            Toast.makeText(this, R.string.register_failed, Toast.LENGTH_LONG).show();
            return;
        }

        new RegistrationTask(this, slotId, getPrimarySimPhoneId()) {
            @Override
            public void onResult(boolean registered, String resultDesc) {
                toast(resultDesc);
                if (registered) {
                    mSharedPreferences.edit().putString(PREF_ICCID_ON_SUCCESS, getIccIdText())
                            .commit();
                }
            }
        };
    }

    private int getPrimarySimPhoneId() {
        String carrierMode = SystemProperties.get("persist.radio.carrier_mode", "default");
        boolean cTClassA = carrierMode.equals("ct_class_a");
        if (!cTClassA) {
            int ddsSubId =  mSubscriptionManager.getDefaultDataSubscriptionId();
            int ddsSlotId = SubscriptionManager.getSlotId(ddsSubId);
            // sim on dds slot is treated as primary SIM
            return ddsSlotId;
        }
        // sim on  phone 0 is treated as primay SIM for class a
        return PhoneConstants.SUB1;
    }

    private void onRegistrationRquest() {
        // MT in APM or SIM state not ready will not register.
        if (mRegistering || !isAnySimCardReady() || hasRoamingSub()) {
            if (DBG) {
                Log.d(TAG, "Any SIM is not ready or in Roaming state, not to register");
            }
            return;
        }
        // remaing the delay for fetching correct parameters when power up
        // otherwise, aligning with SMS auto reg
        if (mPowerOn) {
            if (mDelayBarrier) {
                mDelayBarrier = false;
                new Handler().postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        mPowerOn = false;
                        onRegistrationRquest();
                    }
                }, DELAY_REQUEST_AFTER_POWER_ON);
            }
            return;
        }

        String iccIdText = getIccIdText();
        if (DBG) {
            Log.d(TAG, "Iccid: " + iccIdText);
        }

        if (isIccIdChanged(iccIdText)
                || iccIdText.equals(mSharedPreferences.getString(PREF_ICCID_ON_SUCCESS, null))) {
            Toast.makeText(this, R.string.already_registered, Toast.LENGTH_LONG).show();
            if (DBG) {
                Log.d(TAG, "Registered subs, Ignore");
            }
            return;
        }
        int slotId = getCTSlotId();
        if (slotId < 0) {
            // Giving phone id on primary stack
            // so that MEID can be fetched
            slotId = getPrimaryStackPhoneId();
            Log.d(TAG, "Getting phone ID: " + slotId + "primary stack");
        }

        if (!isDDSOnCTSlotWithoutWifi()) {
            return;
        }

        mRegistering = true;
        new RegistrationTask(this, slotId, getPrimarySimPhoneId()) {
            @Override
            public void onResult(boolean registered, String resultDesc) {
                mRegistering = false;
                if (!registered) {
                    scheduleNextIfNeed();
                } else {
                    mSharedPreferences.edit().putString(PREF_ICCID_ON_SUCCESS, getIccIdText())
                            .commit();
                    if (DBG) {
                        Log.d(TAG, "Register Done!");
                    }
                }
                toast(resultDesc);
            }
        };
    }

    private boolean hasRoamingSub() {
        for (int i = 0; i < TelephonyManager.getDefault().getPhoneCount(); i++ ) {
            int[] subId = SubscriptionManager.getSubId(i);
            if (subId != null && TelephonyManager.getDefault().isNetworkRoaming(subId[0])) {
                return true;
            }
        }
        return false;
    }

    private int getPrimaryStackPhoneId() {
        int phoneId = 0;
        try {
            if (mExtTelephony != null) {
                phoneId = mExtTelephony.getPrimaryStackPhoneId();
            }
        } catch (RemoteException ex) {
            Log.w(TAG, "Failed to get primary stack id");
        }
        return phoneId;
    }

    private int getCTSlotId() {
        int slotId = -1;
        TelephonyManager telephonyMgr = TelephonyManager.getDefault();
        int phoneCount = telephonyMgr.getPhoneCount();
        Log.d(TAG, "getCTSlotId");
        for ( int index = 0; index < phoneCount; index++) {
            Log.d(TAG, "phone index: " + index);
            Phone phone = PhoneFactory.getPhone(index);
            if (phone.getPhoneType() == PhoneConstants.PHONE_TYPE_CDMA) {
                Log.d(TAG, "Got CDMA phone");
                String iccId = phone.getIccSerialNumber();
                Log.d(TAG, "ICCID: " + iccId);
                if(iccId != null) {
                    for (String iin : CT_IIN.split(","))    {
                        if (iccId.startsWith(iin)) {
                             Log.d(TAG, "Got CT slot index: " + index);
                            return index;
                        }
                    }
                }
            }
        }
        Log.d(TAG, "No slot with CT card");
        return slotId;
    }

    private boolean isDDSOnCTSlotWithoutWifi() {
        // Without wifi but dds in SIM2 will not register.
        ConnectivityManager connMgr =
                (ConnectivityManager) getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo networkInfo = connMgr.getActiveNetworkInfo();
        if (networkInfo != null && networkInfo.isConnected()) {
            if (TelephonyManager.from(this).isMultiSimEnabled()) {
                if (networkInfo.getType() != ConnectivityManager.TYPE_WIFI) {
                    int ddsSlot = SubscriptionManager
                            .getPhoneId(SubscriptionManager.getDefaultDataSubscriptionId());
                    Phone phone = PhoneFactory.getPhone(ddsSlot);
                    boolean ctWithDds = false;
                    if (phone != null) {
                        String iccId = phone.getIccSerialNumber();
                        if (iccId != null) {
                            for (String iin : CT_IIN.split(",")) {
                                if (iccId.startsWith(iin)) {
                                    ctWithDds = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (DBG && !ctWithDds) {
                        Log.d(TAG, "DDS now on non CT slot without wifi, not to register");
                    }
                    return ctWithDds;
                }
            }
        }
        return true;
    }

    private boolean isAnySimCardReady() {
        int numPhones = TelephonyManager.getDefault().getPhoneCount();
        for (int index = 0; index < numPhones; index++) {
            if (TelephonyManager.getDefault().getSimState(index)
                    == TelephonyManager.SIM_STATE_READY) {
                return true;
            }
        }
        return false;
    }

    private boolean isIccIdChanged(String iccId) {
        boolean flag = true;
        if (iccId.contains(",")) {
            String[] strArray = TextUtils.split(iccId, ",");
            for (String str : strArray) {
                flag &= TextUtils.isEmpty(str.trim());
            }
        } else {
            flag = TextUtils.isEmpty(iccId.trim());
        }
        return flag;
    }

    private String getIccIdText() {
        String iccId = null;
        if (TelephonyManager.getDefault().isMultiSimEnabled()) {
            int phoneCount = TelephonyManager.getDefault().getPhoneCount();
            for (int index = 0; index < phoneCount; index++) {
                String id = TelephonyManager.from(this).getSimSerialNumber(
                        SubscriptionManager.getSubId(index)[0]);
                if (id == null) {
                    id = " ";
                }
                if (iccId == null) {
                    iccId = id;
                } else {
                    iccId += ("," + id);
                }
            }
        } else {
            String id = TelephonyManager.from(this).getSimSerialNumber();
            iccId = (null == id) ? " " : id;
        }
        return iccId;
    }

    protected void scheduleNextIfNeed() {
        int reuqestCount = mSharedPreferences.getInt(PREF_REQUEST_COUNT, 0) + 1;
        mSharedPreferences.edit().putInt(PREF_REQUEST_COUNT, reuqestCount).commit();
        if (reuqestCount < MAX_REQUEST_TIMES) {
            PendingIntent intent = PendingIntent.getBroadcast(this, 0, new Intent(
                    ACTION_AUTO_REGISTERATION), 0);
            mAlarmManager.set(AlarmManager.RTC_WAKEUP,
                    System.currentTimeMillis() + INTERVAL_RESCHEDUAL, intent);
        }
    }

    private void cancelNextSchedule() {
        PendingIntent pi = PendingIntent.getBroadcast(this, 0, new Intent(
                 ACTION_AUTO_REGISTERATION), PendingIntent.FLAG_NO_CREATE);
        if (pi != null) {
            mAlarmManager.cancel(pi);
        }
    }

    protected void toast(final String resultDesc) {
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                String resultInfo = null;
                if (TextUtils.isEmpty(resultDesc)) {
                    resultInfo = RegistrationService.this.getResources().getString(
                            R.string.register_failed);
                } else {
                    resultInfo = getLocalString(resultDesc);
                }
                Toast.makeText(RegistrationService.this, resultInfo, Toast.LENGTH_LONG).show();
            }
        });
    }

    private String getLocalString(String originalResult) {
        return (android.util.NativeTextHelper.getInternalLocalString(RegistrationService.this,
                originalResult,
                R.array.original_registry_results, R.array.local_registry_results));
    }

}
