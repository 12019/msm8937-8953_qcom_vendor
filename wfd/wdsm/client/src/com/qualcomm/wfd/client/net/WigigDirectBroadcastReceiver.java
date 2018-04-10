/*
 * Copyright (c) 2012-2013, 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Not a Contribution, Apache license notifications and license are retained
 * for attribution purposes only.
 */

/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.qualcomm.wfd.client.net;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.NetworkInfo;
import android.net.wifi.p2p.WifiP2pDevice;
import android.net.wifi.p2p.WifiP2pDeviceList;
import android.net.wifi.p2p.WifiP2pGroup;
import android.net.wifi.p2p.WifiP2pInfo;
import com.qualcomm.qti.wigig.p2p.WigigP2pManager;
import android.util.Log;

import com.qualcomm.wfd.client.WFDClientActivity;

import java.util.Collection;

/**
 * A BroadcastReceiver that notifies of important wifi p2p events.
 */
public class WigigDirectBroadcastReceiver extends BroadcastReceiver {

    private static final String TAG = "C.WigigDBReceiver";
    private WigigP2pManager manager;
    private WFDClientActivity activity;
    private WigigP2pNetManager wifiUtil;

    /**
     * @param manager WigigP2pManager system service
     * @param //channel WifiP2p p2p channel
     * @param //activity activity associated with the receiver
     */
    public WigigDirectBroadcastReceiver(WigigP2pManager manager/*, Channel channel*/) {
        super();
        this.manager = manager;
        wifiUtil = WigigP2pNetManager.sInstance;
    }

    /*
     * (non-Javadoc)
     * @see android.content.BroadcastReceiver#onReceive(android.content.Context,
     * android.content.Intent)
     */
    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        Log.v(TAG, "onReceive: " + intent);

        if (WigigP2pManager.WIGIG_P2P_STATE_CHANGED_ACTION.equals(action)) {
            Log.d(TAG, "WIGIG_P2P_STATE_CHANGED_ACTION");
            // UI update to indicate wifi p2p status.
            int state = intent.getIntExtra(WigigP2pManager.EXTRA_WIGIG_STATE, -1);
            wifiUtil.onStateChanged(state);
            Log.d(WFDClientActivity.TAG, "P2P state changed - " + state);
        } else if (WigigP2pManager.WIGIG_P2P_PEERS_CHANGED_ACTION.equals(action)) {
            // request available peers from the wifi p2p manager. This is an
            // asynchronous call and the calling activity is notified with a
            // callback on PeerListListener.onPeersAvailable()
            Log.d(TAG, "WIGIG_P2P_PEERS_CHANGED_ACTION");
            WifiP2pDeviceList devices = (WifiP2pDeviceList) intent.getParcelableExtra(WigigP2pManager.EXTRA_P2P_DEVICE_LIST);
            wifiUtil.onPeerListUpdate(devices.getDeviceList());
        } else if (WigigP2pManager.WIGIG_P2P_CONNECTION_CHANGED_ACTION.equals(action)) {

                Log.d(TAG, "WIGIG_P2P_CONNECTION_CHANGED_ACTION");
            if (manager == null) {
                return;
            }

            NetworkInfo networkInfo = (NetworkInfo) intent
                    .getParcelableExtra(WigigP2pManager.EXTRA_NETWORK_INFO);

            if (networkInfo.isConnected()) {
                WifiP2pInfo wifiInfo = intent.getParcelableExtra(WigigP2pManager.EXTRA_WIGIG_P2P_INFO);
                if (wifiInfo.groupFormed) {
                    WifiP2pGroup wifip2pgroup = intent.getParcelableExtra(WigigP2pManager.EXTRA_WIGIG_P2P_GROUP);
                    Collection<WifiP2pDevice> devices = wifip2pgroup.getClientList();
                    wifiUtil.onConnectionInfoAvailable(wifiInfo);
                    wifiUtil.onConnectedListUpdate(devices);
                    if(wifiInfo.isGroupOwner && wifip2pgroup.getClientList().isEmpty()) {
                        Log.e(TAG,"Group formed but client list is null");//Auto-GO mode
                    }
                }
            } else {
                // It's a disconnect
                Log.d(TAG, "It's a disconnect.");
                wifiUtil.onDisconnected();
            }
        } else if (WigigP2pManager.WIGIG_P2P_THIS_DEVICE_CHANGED_ACTION.equals(action)) {

            Log.d(TAG, "WIGIG_P2P_THIS_DEVICE_CHANGED_ACTION");
            wifiUtil.onLocalDeviceChanged((WifiP2pDevice) intent.getParcelableExtra(
                            WigigP2pManager.EXTRA_WIGIG_P2P_DEVICE));
            Log.d(TAG, "This Device Changed" + ((WifiP2pDevice) intent.getParcelableExtra(
                            WigigP2pManager.EXTRA_WIGIG_P2P_DEVICE)).toString());
        }
    }
}
