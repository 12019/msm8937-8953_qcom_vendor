
/*
* Copyright (c) 2012-2016 Qualcomm Technologies, Inc.
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

package com.qualcomm.wfd.client;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.wifi.p2p.WifiP2pConfig;
import android.preference.PreferenceManager;
import android.content.SharedPreferences;

import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.net.wifi.p2p.WifiP2pInfo;
import android.net.wifi.p2p.WifiP2pDevice;
import android.net.wifi.p2p.WifiP2pManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.RemoteException;
import java.lang.InterruptedException;
import java.lang.ref.WeakReference;
import java.util.Collection;
import java.util.Map;

import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.TableLayout;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;

import com.qualcomm.wfd.WfdDevice;
import com.qualcomm.wfd.WfdEnums;
import com.qualcomm.wfd.WfdStatus;
import com.qualcomm.wfd.WfdEnums.WFDDeviceType;
import com.qualcomm.wfd.client.DeviceListFragment.DeviceActionListener;
import com.qualcomm.wfd.client.ServiceUtil.ServiceFailedToBindException;
import com.qualcomm.wfd.client.WfdOperationUtil.WfdOperation;
import com.qualcomm.wfd.client.WfdOperationUtil.WfdOperationTask;
import com.qualcomm.wfd.client.net.CombinedNetManager;
import com.qualcomm.wfd.client.net.NetDeviceIF;
import com.qualcomm.wfd.client.net.NetManagerIF;
import com.qualcomm.wfd.client.net.ResultListener;
import com.qualcomm.wfd.service.IWfdActionListener;
import static com.qualcomm.wfd.client.WfdOperationUtil.*;
import com.qualcomm.wfd.WfdEnums.AVPlaybackMode;

public class WFDClientActivity extends Activity implements DeviceActionListener, SharedPreferences.OnSharedPreferenceChangeListener {
	public static final String TAG = "C.WFDClientActivity";

    private static WeakReference<WFDClientActivity> sInstanceRef = new WeakReference<WFDClientActivity>(null);

	private TextView mynameTextView;
	private TextView mymacaddressTextView;
	private TextView mystatusTextView;
    private TextView mynameTextViewWigig;
    private TextView mymacaddressTextViewWigig;
    private TextView mystatusTextViewWigig;

	private TableLayout controlbuttonsTableLayout;
	private ToggleButton playpauseButton;
	private ToggleButton standbyresumeButton;
	private Button teardownButton;
	private ToggleButton enableDisableUibcButton;
    private ToggleButton selectTransportButton;
    private ToggleButton flushButton;
    private ToggleButton toggleDSButton;

	private MenuItem sourceItem;
	private MenuItem sinkItem;
	private Boolean modeSource = true;
	private int localDeviceType = WFDDeviceType.SOURCE.getCode();
	private boolean startingSession = false;
        private boolean mUIBCM14 = false;
        private boolean mModeDS = false;
	private boolean inSession = false;
	ProgressDialog startSessionProgressDialog = null;
    ProgressDialog ipDialog = null;
	private WifiP2pDevice device;
	private boolean isWifiP2pEnabled = false;
	private boolean retryChannel = false;
	private final IntentFilter intentFilter = new IntentFilter();
	private BroadcastReceiver receiver = null;
	boolean isDiscoveryRequested = false;
	CombinedNetManager wifiUtil;
	private ClientEventHandler eventHandler;
	private IWfdActionListener mActionListener;
	private SharedPreferences mSharedPrefs = null;
        private WfdDevice localWfdDev = null, peerWfdDev = null;
	private final int START_SURFACE = 1;

	/**
	 * @param isWifiP2pEnabled
	 *            the isWifiP2pEnabled to set
	 */
	public void setIsWifiP2pEnabled(boolean isWifiP2pEnabled) {
		this.isWifiP2pEnabled = isWifiP2pEnabled;
		if (!this.isWifiP2pEnabled) {
		    Log.d(TAG, "Find selected from action bar while p2p off");
			Toast.makeText(WFDClientActivity.this,
					R.string.p2p_off_warning, Toast.LENGTH_SHORT)
					.show();
		}
	}

    public static WFDClientActivity getInstance() {
        return sInstanceRef.get();
    }

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
        sInstanceRef = new WeakReference<>(this);
		setContentView(R.layout.main);

        eventHandler = new ClientEventHandler();
        wifiUtil = new CombinedNetManager(eventHandler, WfdEnums.NetType.WIFI_P2P, WfdEnums.NetType.WIGIG_P2P);
		wifiUtil.send_wfd_set(true, WFDDeviceType.getValue(localDeviceType));

		try {
                    ServiceUtil.bindService(getApplicationContext(), eventHandler);
                } catch (ServiceFailedToBindException e) {
                    Log.e(TAG, "ServiceFailedToBindException received");
                }

        mynameTextView = (TextView) this.findViewById(R.id.tv_my_name);
		mymacaddressTextView = (TextView) this.findViewById(R.id.tv_my_mac_address);
		mystatusTextView = (TextView) this.findViewById(R.id.tv_my_status);
        mynameTextViewWigig = (TextView) this.findViewById(R.id.tv_my_name_wigig);
        mymacaddressTextViewWigig = (TextView) this.findViewById(R.id.tv_my_mac_address_wigig);
        mystatusTextViewWigig = (TextView) this.findViewById(R.id.tv_my_status_wigig);

		controlbuttonsTableLayout = (TableLayout) this.findViewById(R.id.tl_control_buttons);
		playpauseButton = (ToggleButton) this.findViewById(R.id.btn_play_pause);
		standbyresumeButton = (ToggleButton) this.findViewById(R.id.btn_standby_resume);
		teardownButton = (Button) this.findViewById(R.id.btn_teardown);
		enableDisableUibcButton = (ToggleButton) this.findViewById(R.id.btn_start_stop_uibc);
        selectTransportButton = (ToggleButton) this.findViewById(R.id.btn_transport_udp_tcp);
        flushButton = (ToggleButton) this.findViewById(R.id.btn_flush);
        toggleDSButton = (ToggleButton) this.findViewById(R.id.btn_enable_disable_ds);
        mSharedPrefs = PreferenceManager.getDefaultSharedPreferences(this);
        PreferenceManager.setDefaultValues(this, R.xml.preferences, false);
        mSharedPrefs.registerOnSharedPreferenceChangeListener(this);
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
        if (mSharedPrefs != null)
        { mSharedPrefs.unregisterOnSharedPreferenceChangeListener(this);
          mSharedPrefs = null;
        }
		try {
		    unbindWfdService();
	        } catch (IllegalArgumentException e) {
	            Log.e(TAG,"Illegal Argument Exception ",e);
		}
        wifiUtil.destroy();
        wifiUtil = null;
		Log.d(TAG, "onDestroy() called");
	}

       /** To prevent dynamic configuration changes from destroying activity */
       @Override
	public void onConfigurationChanged (Configuration newConfig) {
	Log.e(TAG, "onConfigurationChanged called due to"+ newConfig.toString());
	super.onConfigurationChanged(newConfig);
       }

	/** register the BroadcastReceiver with the intent values to be matched */
	@Override
	public void onResume() {
        super.onResume();
        Log.d(TAG, "onResume() called");
        wifiUtil.onMainActivityResume();
    }

	@Override
	public void onPause() {
		super.onPause();
        wifiUtil.onMainActivityPause();
        Log.d(TAG, "onPause() called");
	}

    @Override
	public void onSharedPreferenceChanged(SharedPreferences sharedPreferences,String key){

        Log.d(TAG, "onSharedPreferenceChanged Listner");

        if (mSharedPrefs == null) {
            Log.e(TAG, "Null objects!");
            return;
        }

        if (key.equals("udp_decoder_latency_value")){

            Log.d(TAG, "decoder latency preference changed");
            int decoderLatency = Integer.parseInt(mSharedPrefs.getString("udp_decoder_latency_value", "5"));

            if (peerWfdDev != null
                    && decoderLatency != peerWfdDev.decoderLatency && inSession) {
                Log.d(TAG, "setting decoder latency");
                startWfdOperationTask(this, WfdOperation.SET_DECODER_LATENCY);
                peerWfdDev.decoderLatency = decoderLatency;
            }
        }
    }

	@Override
	public boolean onPrepareOptionsMenu(Menu menu) {
	    super.onPrepareOptionsMenu(menu);
	    sourceItem = menu.findItem(R.id.menu_select_source);
	    sinkItem = menu.findItem(R.id.menu_select_sink);
        mModeDS = false;
	    return true;
	}

    public void playPauseWfdOperation(View view) {
        Log.d(TAG, "playPauseControlOperation() called");
        if (!playpauseButton.isChecked()) {
            Log.d(TAG, "playPauseWfdOperation: Play button clicked");
            playpauseButton.toggle();
            startWfdOperationTask(this, WfdOperation.PLAY);
        } else {
            Log.d(TAG, "playPauseWfdOperation: Pause button clicked");
            playpauseButton.toggle();
            startWfdOperationTask(this, WfdOperation.PAUSE);
        }
    }

    public void standbyResumeWfdOperation(View view) {
        Log.d(TAG, "standbyResumeWfdOperation() called");
        if (!standbyresumeButton.isChecked()) {
            Log.d(TAG, "standbyResumeWfdOperation: Standby button clicked");
            standbyresumeButton.toggle();
            startWfdOperationTask(this, WfdOperation.STANDBY);
        } else {
            Log.d(TAG, "standbyResumeWfdOperation: Resume button clicked");
            standbyresumeButton.toggle();
            startWfdOperationTask(this, WfdOperation.RESUME);
        }
    }

    public void startStopUibcWfdOperation(View view) {
        Log.d(TAG, "startStopUibcControlOperation() called");
        enableDisableUibcButton.toggle();
        if (!mUIBCM14) {
            if (!enableDisableUibcButton.isChecked()) {
                Log.d(TAG, "Start UIBC button clicked");
                startWfdOperationTask(this, WfdOperation.START_UIBC);
            } else {
                Log.d(TAG, "Stop UIBC button clicked");
                startWfdOperationTask(this, WfdOperation.STOP_UIBC);
            }
        } else {
            int ret = -1;
            try {
                ret = ServiceUtil.getInstance().setUIBC();
            } catch (RemoteException e) {
                e.printStackTrace();
            }
            Toast.makeText(getApplicationContext(), "UIBC set: " + ret,
                    Toast.LENGTH_SHORT).show();
            if (0 == ret) {
                enableDisableUibcButton.setText("Start UIBC");
                mUIBCM14 = false;
            } else {
                enableDisableUibcButton.setText("Send M14");
                mUIBCM14 = true;
            }
        }
    }

    public void toggleDSMode(View view) {
        Log.d(TAG, "toggleDSMode() called");
        if(!ServiceUtil.getmServiceAlreadyBound()) {
            Log.e(TAG, "toggleDSMode() called when not in WFD Session");
            Toast.makeText(getApplicationContext(), "Not in WFD Session!",
                Toast.LENGTH_SHORT).show();
            return;
        }

        try {
            WfdStatus wfdStatus = ServiceUtil.getInstance().getStatus();
            if (wfdStatus.state != WfdEnums.SessionState.PLAY.ordinal()) {
                Log.d(TAG, "Toggle DS Not allowed in non-PLAY state");
                toggleDSButton.toggle();
                Toast.makeText(getApplicationContext(),
                        "Not honouring DirStr toggle in non-PLAY state",
                        Toast.LENGTH_SHORT).show();
                return;
            }
        } catch (RemoteException e) {
            e.printStackTrace();
        }

        /*Toggle DS Mode*/
        int ret = -1;
        try {
            ret = ServiceUtil.getInstance().toggleDSMode(!mModeDS);
        } catch(RemoteException e) {
            e.printStackTrace();
        }

        /*Update UI Button*/
        if(ret == 1) {
            mModeDS = !mModeDS;
            if(toggleDSButton.isChecked()) {
                Log.d(TAG, "Enabling DS");
                toggleDSButton.setText("Disable DS");
            } else {
                Log.d(TAG, "Disabling DS");
                toggleDSButton.setText("Enable DS");
            }
        }
    }

    public void toggleRTPTransport(View view) {
        Log.d(TAG, "toggleRTPTransport() called");
        if (ServiceUtil.getmServiceAlreadyBound()) {
            try {
                WfdStatus wfdStatus = ServiceUtil.getInstance().getStatus();
                if (wfdStatus.state != WfdEnums.SessionState.PLAY.ordinal()) {
                    Log.d(TAG, "Not allowed in non-PLAY state");
                    selectTransportButton.toggle();
                    Toast.makeText(getApplicationContext(),
                            "Not honouring TCP/UDP switch in non-PLAY state",
                            Toast.LENGTH_SHORT).show();
                    return;
                }
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
        if (selectTransportButton.isChecked()) {
            Log.d(TAG, "Start TCP Transport");
            selectTransportButton.toggle();
            startWfdOperationTask(this, WfdOperation.SELECT_TCP);
        } else {
            Log.d(TAG, "Start UDP Transport");
            selectTransportButton.toggle();
            startWfdOperationTask(this, WfdOperation.SELECT_UDP);
        }
    }

    public void flushOperation(View view) {
        Log.d(TAG, "flushOperation() called");
        startWfdOperationTask(this, WfdOperation.FLUSH);
    }

    public void teardownWfdOperation(View view) {
        if(view == null) {
            if(localDeviceType != WFDDeviceType.SOURCE.getCode()) {
                /*
                 * This call has been triggered after receiving network disconnect
                 * event. In case of sink, we should trigger a teardown from the
                 * application level.
                 */
                if(ServiceUtil.getmServiceAlreadyBound()) {
                    Log.d(TAG,"Teardown on network disconnect");
                    try {
                        ServiceUtil.getInstance().teardown();
                    } catch (RemoteException e) {
                        e.printStackTrace();
                    }
                }
            }
            return;
        }
        Log.d(TAG, "Teardown button clicked");
        WfdOperationTask task = new WfdOperationTask(this, "Tearing Down WFD Session", WfdOperation.TEARDOWN);
        task.execute();
    }

	private class PreTeardownTaskSessionCheck extends AsyncTask<Void, Void, Boolean> {
        @Override
        protected Boolean doInBackground(Void... params) {
            try {
                //verify session state
                WfdStatus wfdStatus = ServiceUtil.getInstance().getStatus();
                Log.d(TAG, "wfdStatus.state= " + wfdStatus.state);
                if (wfdStatus.state == WfdEnums.SessionState.INVALID.ordinal() ||
                        wfdStatus.state == WfdEnums.SessionState.INITIALIZED.ordinal()) {
                    Log.d(TAG, "wfdStatus.state= " + wfdStatus.state);
                    return false;
                } else {
                    return true;
                }
            } catch (RemoteException e) {
                Log.e(TAG, "PreTeardownTaskSessionCheck- Remote exception", e);
            }
            return true;
        }

        @Override
        protected void onPostExecute(Boolean callTeardown) {
            Log.d(TAG, "PreTeardownTaskSessionCheck- onPostExecute");
            Log.d(TAG, "PreTeardownTaskSessionCheck- callTeardown: " + callTeardown);
            if (callTeardown) {
                Log.d(TAG, "PreTeardownTaskSessionCheck- now calling teardown");
            } else {
                Log.d(TAG, "PreTeardownTaskSessionCheck- not in a WFD session, not going to call teardownOperation");
            }
        }
    }


	public class StartSessionTask extends AsyncTask<NetDeviceIF, Void, Integer> {

        AlertDialog.Builder builder;
        Boolean showBuilder = false;

		@Override
		protected Integer doInBackground(NetDeviceIF... devices) {
			Log.d(TAG, "StartSessionTask: doInBackground called");
			int ret = 0;
			NetDeviceIF peerDevice = devices[0];
			if (!peerDevice.supportWfd()) {
				Log.e(TAG, "StartSessionTask: doInBackground- Device is missing wfdInfo");
				Message messageInit = eventHandler.obtainMessage(INVALID_WFD_DEVICE);
				eventHandler.sendMessage(messageInit);
				return ret;
			}

			try {
				Log.d(TAG, "StartSessionTask- doInBackground- Setting surface to a surface");
				if (!peerDevice.getNetTypeManager().getLocalDevice().hasDeviceInfo()) {
                    builder = new AlertDialog.Builder(WFDClientActivity.this);
                    builder.setTitle("Start session cannot proceed")
                         .setMessage("This device is null")
                         .setPositiveButton("Ok", new DialogInterface.OnClickListener() {
                         public void onClick(DialogInterface dialog, int id) {
                             return;
                         }
                    });
				    this.showBuilder = true;
				    return ret;
				}
                int initReturn = ServiceUtil.getInstance().init(
                        WFDClientActivity.this.mActionListener, localWfdDev);
	            if (!(initReturn == 0 || initReturn == WfdEnums.ErrorType.ALREADY_INITIALIZED.getCode())) {
                    Log.e(TAG, "StartSessionTask- doInBackground: init failed with error- " + initReturn);
                }
                if (peerWfdDev.ipAddress == null && modeSource == false) {
				    Log.e(TAG, "StartSessionTask- doInBackground: ipaddress null pop up");
                    builder = new AlertDialog.Builder(WFDClientActivity.this);
		            builder.setTitle("Start session cannot proceed")
    		            .setMessage("Unable to obtain peer ip address")
    		            .setPositiveButton("Ok", new DialogInterface.OnClickListener() {
    		                public void onClick(DialogInterface dialog, int id) {
    		                    return;
    		                }
    		            });
                    showBuilder = true;
		            return ret;
				}
                ret = ServiceUtil.getInstance().startWfdSession(peerWfdDev);
			} catch (RemoteException e) {
				Log.e(TAG, "Remote exception", e);
			}
			return ret;
		}

		@Override
		protected void onPreExecute() {

			if(!ServiceUtil.getmServiceAlreadyBound()){
				/* This is not a normal scenario as we always bind service in onOptionFind()
				* Its almost impossible for user to start session without pressing find.
				* Just addressing the corner case here
				*/
				Log.e(TAG, "Service is not already bound, do it now");
				try {
					ServiceUtil.bindService(getApplicationContext(), eventHandler);
				} catch (ServiceFailedToBindException e) {
					Log.e(TAG, "ServiceFailedToBindException received");
				}
			}

			Log.d(TAG, "StartSessionTask- onPreExecute() called");
			startSessionProgressDialog = ProgressDialog.show(WFDClientActivity.this,
					"Starting WFD Session", "Press back to cancel", true, true);

			startSessionProgressDialog.setCancelable(true);
			startSessionProgressDialog.setOnCancelListener(new  DialogInterface.OnCancelListener() {
				@Override
				public void onCancel(DialogInterface dialog) {
					Log.d(TAG, "StartSessionTask- onCancel called before");
					cancel(true);
					try {
                                            if(ServiceUtil.getmServiceAlreadyBound()) {
                                               ServiceUtil.getInstance().teardown();
                                            }
					} catch (RemoteException e) {
						Log.e(TAG, "RemoteException in teardown");
					}
					Log.d(TAG, "StartSessionTask- onCancel called after");
				}
			});

                	if (!modeSource) {
                   		showHideWfdSurface();
                   		Log.d(TAG, "StartSessionTask: onPreExecute- device is sink, showHideWfdSurface");
                	}

	        	Log.d(TAG, "StartSessionTask: onPreExecute- end");
		};

		@Override
		protected void onCancelled() {
            		Log.d(TAG, "StartSessionTask: onCancelled called because" +
				" startSessionTask returned");
		}

		@Override
		protected void onPostExecute(Integer sessionId) {
            Log.d(TAG, "StartSessionTask: onPostExecute() called");

            Boolean startSessionSuccess = true;
            if (showBuilder) {
                Log.d(TAG, "StartSessionTask: onPostExecute- showBuilder = true, showing unable to obtain peer ip address alert");
                startSessionSuccess = false;
                AlertDialog alert = builder.create();
                alert.show();
                Message messageFailSession = eventHandler.obtainMessage(CLEAR_UI);
                eventHandler.sendMessage(messageFailSession);
            }

			if (sessionId < 0) {
                Log.d(TAG, "StartSessionTask: onPostExecute- sessionId < 0, Failed to start session with error: " + sessionId);
				Toast.makeText(WFDClientActivity.this,
						"Failed to start session with error: " + sessionId,
						1500).show();
				return;
			} else if (sessionId == WfdEnums.ErrorType.OPERATION_TIMED_OUT.getCode()) {
                Log.d(TAG, "StartSessionTask: onPostExecute- Start session is taking longer than expected");
				AlertDialog.Builder builder = new AlertDialog.Builder(WFDClientActivity.this);
				builder.setTitle("Start session is taking longer than expected")
					   .setMessage("Would you like to continue waiting?")
				       .setPositiveButton("Yes", new DialogInterface.OnClickListener() {
				           public void onClick(DialogInterface dialog, int id) {
                               Log.d(TAG, "StartSessionTask: onPostExecute- User clicked yes (would like to continue waiting)");
				        	   return;
				           }
				       })
				       .setNegativeButton("No", new DialogInterface.OnClickListener() {
				           public void onClick(DialogInterface dialog, int id) {
                                Log.d(TAG, "StartSessionTask: onPostExecute- User clicked no (would not like to continue waiting)");
                                teardownWfdOperation(null);
				           }
				       });
				AlertDialog alert = builder.create();
				alert.show();
            } else if (startSessionSuccess) {
                Log.d(TAG, "StartSessionTask: completed successfully.");
                wifiUtil.stopPeerDiscovery(ResultListener.NullListener);
                inSession = true;
			} else {
                Log.d(TAG, "StartSessionTask: completed unsuccessfully.");
			}
		}
	}

	public void connectClickHandler(View v) {
		Log.d(TAG, "connectClickHandler received click");
		DeviceListFragment fragmentList = (DeviceListFragment) getFragmentManager()
				.findFragmentById(R.id.frag_list);
		fragmentList.handleConnectClick(v);
	}

	public void connectOptionsClickHandler(View v) {
		Log.d(TAG, "connectOptionsClickHandler received click");
		DeviceListFragment fragmentList = (DeviceListFragment) getFragmentManager()
				.findFragmentById(R.id.frag_list);
		fragmentList.handleConnectOptionsClick(v);
	}

	public void disconnectClickHandler(View v) {
		Log.d(TAG, "disconnectClickHandler received click");
		disconnect();
	}

	public void startClientClickHandler(View v) {
		Log.d(TAG, "startClientClickHandler received click");
		DeviceListFragment fragmentList = (DeviceListFragment) getFragmentManager()
				.findFragmentById(R.id.frag_list);
		fragmentList.handleStartClientClick(v);
	}

	public void filterClickHandler(View V) {
		Log.d(TAG, "filterClickHandler received click");
		DeviceListFragment fragmentList = (DeviceListFragment) getFragmentManager()
			.findFragmentById(R.id.frag_list);
		fragmentList.handleFilterClick();
	}

	/**
	 * Remove all peers and clear all fields. This is called on
	 * BroadcastReceiver receiving a state change event.
	 */
	public void resetData() {
		Log.d(TAG, "resetData() called");
		DeviceListFragment fragmentList = (DeviceListFragment) getFragmentManager()
				.findFragmentById(R.id.frag_list);
		if (fragmentList != null) {
			fragmentList.clearPeers();
		}
	}

	@Override
	public void connect(WifiP2pConfig config, final NetDeviceIF device) {
		Log.d(TAG, "connect(,) called");
        device.setConfig(config);
        device.connect(new ResultListener() {
            @Override
            public void onSuccess(Object result) {
                final DeviceListFragment fragment = (DeviceListFragment)
                        getFragmentManager().findFragmentById(R.id.frag_list);
                fragment.setConnectedDevice(device);
            }

            @Override
            public void onFailure(Object err) {
                Toast.makeText(WFDClientActivity.this,
                        "Connect failed. Retry.", Toast.LENGTH_SHORT)
                        .show();
            }
        });
	}

	public void disconnect() {
	    Log.d(TAG, "disconnect() called");
        wifiUtil.disconnect(ResultListener.NullListener);
    }

	@Override
	public void cancelDisconnect() {
		/*
		 * A cancel abort request by user. Disconnect i.e. removeGroup if
		 * already connected. Else, request WifiP2pManager to abort the ongoing
		 * request
		 */
        wifiUtil.cancelConnect(new ResultListener() {
            @Override
            public void onSuccess(Object result) {
                Toast.makeText(WFDClientActivity.this,
                        "Aborting connection",
                        Toast.LENGTH_SHORT).show();
            }

            @Override
            public void onFailure(Object err) {
                Toast.makeText(
                        WFDClientActivity.this,
                        "Connect abort request failed. Reason Code: "
                                + err,
                        Toast.LENGTH_SHORT).show();
            }
        });
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.main_menu, menu);
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		// Handle item selection
		switch (item.getItemId()) {
		/*case R.id.menu_advanced_settings:
			startActivity(new Intent(this, AdvancedPreferences.class));
			break;*/
		case R.id.menu_preferences:
			Log.d(TAG, "prefences clicked");
			startActivity(new Intent(this, Preferences.class));
			break;
		default:
			Log.d(TAG, "No menu item selected");
		}
		return super.onOptionsItemSelected(item);
	}

	public void onOptionSelectSourceOrSink(MenuItem i) {
		Log.d(TAG, "Source or Sink selected from action bar");
		if (i.getTitle().toString().contains("ource")) {
			Log.d(TAG, "Source selected from action bar");
			localDeviceType = WFDDeviceType.SOURCE.getCode();
			if (ServiceUtil.getmServiceAlreadyBound()) {
				try {
					ServiceUtil.getInstance().setDeviceType(localDeviceType);
				} catch (RemoteException e) {
                                        Log.e(TAG, "Remote exception when setting device type", e);
				}
			} else {
				try {
					ServiceUtil.bindService(getApplicationContext(), eventHandler);
				} catch (ServiceFailedToBindException e) {}
			}
                     	wifiUtil.send_wfd_set(true, WFDDeviceType.SOURCE);
			i.setTitle("Source (selected)");
                	modeSource = true;
                	Toast.makeText(WFDClientActivity.this,
                       		       "This device is now Source",
                        		Toast.LENGTH_SHORT).show();

            		sinkItem.setTitle("Sink");
		} else {
			Log.d(TAG, "Sink selected from action bar");
			localDeviceType = WFDDeviceType.PRIMARY_SINK.getCode();
			if (ServiceUtil.getmServiceAlreadyBound()) {
				try {
					ServiceUtil.getInstance().setDeviceType(localDeviceType);
				} catch (RemoteException e) {
                    Log.e(TAG, "Remote exception when setting device type", e);
				}
			} else {
				try {
					ServiceUtil.bindService(getApplicationContext(), eventHandler);
				} catch (ServiceFailedToBindException e) {}
			}
                        wifiUtil.send_wfd_set(true, WFDDeviceType.PRIMARY_SINK);
                	i.setTitle("Sink (selected)");
                	modeSource = false;
                	Toast.makeText(WFDClientActivity.this,
                       	"This device is now Sink",
                       	Toast.LENGTH_SHORT).show();
			sourceItem.setTitle("Source");
		}
	}

    public void onOptionFind(MenuItem i) {
        try {
            ServiceUtil.bindService(getApplicationContext(), eventHandler);
        } catch (ServiceFailedToBindException e) {
            Log.e(TAG, "ServiceFailedToBindException received");
        }
        if (!isWifiP2pEnabled) {
            Log.d(TAG, "Find selected from action bar while p2p off");
            Toast.makeText(WFDClientActivity.this,
                    R.string.p2p_off_warning, Toast.LENGTH_SHORT)
                    .show();
        } else {
            Log.d(TAG, "Find selected from action bar");
            isDiscoveryRequested = true;
            final DeviceListFragment fragment = (DeviceListFragment) getFragmentManager()
                    .findFragmentById(R.id.frag_list);
            fragment.onInitiateDiscovery();
            wifiUtil.discoveryPeers(new ResultListener() {
                @Override
                public void onSuccess(Object result) {
                    Toast.makeText(WFDClientActivity.this,
                            "Discovery Initiated",
                            Toast.LENGTH_SHORT).show();
                }

                @Override
                public void onFailure(Object err) {
                    Toast.makeText(WFDClientActivity.this,
                            "Discovery Failed : " + err,
                            Toast.LENGTH_SHORT).show();
                }
            });
        }
    }

    public void onOptionCreateGroup(MenuItem i) {
        if (!isWifiP2pEnabled) {
            Log.d(TAG, "Create Group selected from menu while p2p off");
            Toast.makeText(WFDClientActivity.this, R.string.p2p_off_warning,
                    Toast.LENGTH_SHORT).show();
        } else {
                Log.d(TAG, "Create Group selected from menu");
                wifiUtil.createGroup(new ResultListener() {
                    @Override
                    public void onSuccess(Object result) {
                        Toast.makeText(WFDClientActivity.this, "Group created",
                                Toast.LENGTH_SHORT).show();
                    }

                    @Override
                    public void onFailure(Object err) {
                        Toast.makeText(WFDClientActivity.this,
                                "Group create failed : " + err,
                                Toast.LENGTH_SHORT).show();
                    }
                });
        }
    }

	public void onOptionRemoveGroup(MenuItem i) {
		if (!isWifiP2pEnabled) {
			Log.d(TAG, "Remove Group selected from menu while p2p off");
			Toast.makeText(WFDClientActivity.this, R.string.p2p_off_warning,
					Toast.LENGTH_SHORT).show();
        } else {
                Log.d(TAG, "Remove Group selected from menu");
                wifiUtil.removeGroup(new ResultListener() {
                    @Override
                    public void onSuccess(Object result) {
                        Toast.makeText(WFDClientActivity.this, "Group removed",
                                Toast.LENGTH_SHORT).show();
                    }

                    @Override
                    public void onFailure(Object err) {
                        Toast.makeText(WFDClientActivity.this,
                                "Group remove failed : " + err,
                                Toast.LENGTH_SHORT).show();
                    }
                });
        }
    }

	public void onOptionPreferences(MenuItem i) {
		Log.d(TAG, "Preferences selected from menu");
		startActivity(new Intent(this, Preferences.class));
	}

	public void onOptionVersion(MenuItem i) {
		Log.d(TAG, "Version selected from menu");
		try {
		    String version = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
			AlertDialog alertDialog = new AlertDialog.Builder(this).create();
			alertDialog.setTitle("Version #");
			alertDialog.setMessage(version);
			alertDialog.show();
		} catch (NameNotFoundException e) {
		    Log.e("tag", e.getMessage());
		}
	}

	public void onOptionShowHideSurface(MenuItem i) {
		showHideWfdSurface();
	}

	public void onOptionShowHideWfdOperationButtons(MenuItem i) {
	    setVisibleControlButtons();
	}

	void unbindWfdService() {
              Log.d(TAG, "unbindWfdService() called");
              if(ServiceUtil.getmServiceAlreadyBound()) {
                 ServiceUtil.unbindService(getApplicationContext());
              }
        }

    @Override
    public void startSession(NetDeviceIF inDevice) {
        Log.d(TAG, "startSession() called for " + (inDevice != null? inDevice.getName() : "unknown"));

        NetDeviceIF localDevInfo = inDevice.getNetTypeManager().getLocalDevice();
        if (localDevInfo == null) {
            Log.e(TAG, "Something amiss!! localDevInfo is null");
            return;
        }

        if( localDeviceType == WFDDeviceType.SOURCE.getCode()) {
            wifiUtil.setMiracastMode(NetManagerIF.MIRACAST_SOURCE);
        }
        if(modeSource && ServiceUtil.getmServiceAlreadyBound()) {
             String avMode = mSharedPrefs.getString("av_mode_type", "Audio_Video");
             AVPlaybackMode av = AVPlaybackMode.AUDIO_VIDEO;
             if(avMode.equals("Audio_Only")) {
                  av = AVPlaybackMode.AUDIO_ONLY;
             } else if(avMode.equals("Video_Only")) {
                  av = AVPlaybackMode.VIDEO_ONLY;
             }
             Log.d(TAG, "AV Mode = " + av);
             try {
                     ServiceUtil.getInstance().setAvPlaybackMode(av.ordinal());
             } catch (RemoteException e) {
                     Log.e(TAG, "Remote exception", e);
             }
        }

        if (localDevInfo.hasDeviceInfo()) {
            if (localDevInfo.isGroupOwner()) {
                localWfdDev = localDevInfo.convertToWfdDevice();
                if (!modeSource) {
                    final getIPTask Task = new getIPTask();
                    Task.execute(inDevice);
                    ipDialog = ProgressDialog.show(this,
                            "Awaiting IP allocation of peer from DHCP",
                            "Press back to cancel", true, true,
                            new DialogInterface.OnCancelListener() {
                                @Override
                                public void onCancel(DialogInterface dialog) {
                                        Log.e(TAG,
                                                "IP Dialog cancelled by listener");
                                    Task.cancel(false);
                                }
                            });
                } else {
                    // Source doesn't require sink IP
                    peerWfdDev = inDevice.convertToWfdDevice();
                    peerWfdDev.decoderLatency =
                    Integer.parseInt(mSharedPrefs.getString("udp_decoder_latency_value", "5"));
                    StartSessionTask task = new StartSessionTask();
                    Log.d(TAG, "Decoder LAtency " + peerWfdDev.decoderLatency);
                    Log.d(TAG, "Start session with " + inDevice.getAddress());
                    task.execute(inDevice);
                }
            } else {
                Log.e(TAG, "Local Device is not a group owner");
                //localWfdDev = convertToWfdDevice(device, null, false);
                localWfdDev = localDevInfo.convertToWfdDevice();
                // Get the local device IP
                /*
                 * localWfdDev.ipAddress = wifiUtil.getLocalIp();
                 */
                // peer is GO, so set its IP as the GO IP from localDdevInfo
                peerWfdDev = inDevice.convertToWfdDevice();
                peerWfdDev.decoderLatency =
                Integer.parseInt(mSharedPrefs.getString("udp_decoder_latency_value", "5"));
                StartSessionTask task = new StartSessionTask();
                Log.d(TAG, "Start session with " + inDevice.getAddress());
                task.execute(inDevice);
            }
        } else if (localDevInfo == null) {
            Log.e(TAG, "Something amiss!! Local Device Info is null.");
        }
    }

    class getIPTask extends AsyncTask<NetDeviceIF, Integer, Boolean> {
        private int timeout = 0;
        String show = new String("Failed to get IP from lease file in attempt ");
        private NetDeviceIF peerDevice = null;
        @Override
        protected Boolean doInBackground(NetDeviceIF... devices) {
            peerDevice = devices[0];
            while (timeout++ < 60) { // try 60 seconds for IP to get populated else
                               // give up
                peerWfdDev = peerDevice.convertToWfdDevice();
                if (peerWfdDev != null && peerWfdDev.ipAddress != null) {
                    return true;
                }
                if (isCancelled()) {
                    Log.e(TAG, "getIP Task was cancelled");
                    return false;
                }
                publishProgress(timeout);
                Log.e(TAG, show + timeout);
                try {
                    Thread.sleep(1000);
                } catch (InterruptedException e) {
                    Log.e(TAG, "Background thread of async task interrupted");
                }
            }
            return false;
        }

        @Override
        protected void onProgressUpdate(Integer... progress) {
            ipDialog.setMessage(show + progress[0]);
        }

        @Override
        protected void onPostExecute(Boolean success) {
            if (ipDialog != null) {
                ipDialog.dismiss();
            }
            if (success) {
                Log.e(TAG, "Successful in obtaining Peer Device IP");
                StartSessionTask task = new StartSessionTask();
                Log.d(TAG, "Start session with " + peerDevice.getAddress());
                task.execute(peerDevice);
            } else {
                Log.e(TAG, "Unsuccessful in obtaining Peer Device IP");
            }
        }
    }
	@Override
	public void stopSession(int sessionId) {
		Log.d(TAG, "stopSession called");
		try {
			ServiceUtil.getInstance().stopWfdSession();
		} catch (RemoteException e) {
			Log.e(TAG, "Remote exception", e);
		}
		setGoneControlButtons();
	}

	@Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK && inSession) {
            Log.d(TAG, "Back button pressed while starting session");
            teardownWfdOperation(null);
            return true;
        } else if (keyCode == KeyEvent.KEYCODE_BACK) {
            Log.d(TAG, "Back button pressed");
            AlertDialog.Builder builder = new AlertDialog.Builder(WFDClientActivity.this);
			builder.setMessage("Would you like to exit the application?")
			       .setPositiveButton("Yes", new DialogInterface.OnClickListener() {
			           public void onClick(DialogInterface dialog, int id) {
			                WFDClientActivity.this.finish();
			           }
			       })
			       .setNegativeButton("No", new DialogInterface.OnClickListener() {
			           public void onClick(DialogInterface dialog, int id) {
			                dialog.cancel();
			           }
			       });
			AlertDialog alert = builder.create();
			alert.show();
        }
        return super.onKeyDown(keyCode, event);
    }

	/**
	 * Update UI for this device.
	 *
     * @param device WifiP2pNetDevice object
	 */
	public void updateThisDevice(NetDeviceIF device) {
		Log.d(TAG, "updateThisDevice called");

        if (wifiUtil instanceof CombinedNetManager) {
            Map<WfdEnums.NetType, NetDeviceIF> localDevices = wifiUtil.getLocalDevices();
            for (WfdEnums.NetType type: localDevices.keySet()) {
                NetDeviceIF dev = localDevices.get(type);
                if (!dev.hasDeviceInfo()) {
                    continue;
                }
                if (type == WfdEnums.NetType.WIFI_P2P) {
                    mynameTextView.setText(dev.getName());
                    mymacaddressTextView.setText(dev.getAddress());
                    mystatusTextView.setText(dev.getStatusString());
                } else if (type == WfdEnums.NetType.WIGIG_P2P) {
                    mynameTextViewWigig.setText(dev.getName());
                    mymacaddressTextViewWigig.setText(dev.getAddress());
                    mystatusTextViewWigig.setText(dev.getStatusString());
                } else {
                    Log.e(TAG, "unknown net type: " + type);
                }
            }
        }
	}

	@Override
	public void peersReceived() {
		Log.d(TAG, "peersReceived() called");
        if (wifiUtil.allSubNetPeersReceived()) {
            Log.d(TAG, "all nets have peers now");
            isDiscoveryRequested = false;
        }
	}

	@Override
	public int getLocalDeviceType() {
		Log.d(TAG, "getLocalDeviceType() called");
		return localDeviceType;
	}

    private void setUIBCButton() {
        Bundle cfgItem = new Bundle();
        int ret = 0;
        try {
            ret = ServiceUtil.getInstance().getConfigItems(cfgItem);
        } catch (RemoteException e) {
            e.printStackTrace();
        }
        int[] configArr = new int[WfdEnums.ConfigKeys.TOTAL_CFG_KEYS.ordinal()];
        configArr = cfgItem.getIntArray(WfdEnums.CONFIG_BUNDLE_KEY);
        if (configArr!=null && 1 == configArr[WfdEnums.ConfigKeys.UIBC_M14.ordinal()]) {
            mUIBCM14 = true;
            enableDisableUibcButton.setText("Send M14");
        } else {
            mUIBCM14 = false;
        }
        Log.d(TAG, "setUIBCButton() M14 support is " + mUIBCM14);
    }

	public void setVisibleControlButtons() {
		Log.d(TAG, "setVisibleControlButtons() called");
		controlbuttonsTableLayout.setVisibility(View.VISIBLE);
        setUIBCButton();
	}

	public void setGoneControlButtons() {
		Log.d(TAG, "setGoneControlButtons() called");
		controlbuttonsTableLayout.setVisibility(View.GONE);
	}

	public void teardownWFDService() {
		Log.d(TAG, "teardownWFDService() called");
		PreTeardownTaskSessionCheck task = new PreTeardownTaskSessionCheck();
		task.execute();
	}

	public void deinitWFDService() {
		try {
			Log.e(TAG, "WiFi Direct deinit do in background started");
			ServiceUtil.getInstance().deinit();
            Log.e(TAG, "WiFi Direct deinit do in background finished");
		} catch (RemoteException e) {
            Log.e(TAG, "Remote exception", e);
		}
	}


	/**
	 * Class for internal event handling in WFDClientActivity. Must run on UI thread.
	 */
	class ClientEventHandler extends Handler {

	    @Override
	    public void handleMessage(Message msg) {
	        Log.d(TAG, "Event handler received: " + msg.what);

	        if (WfdOperationUtil.wfdOperationProgressDialog != null) {
	            Log.d(TAG, "clientProgressDialog != null");
	            if (WfdOperationUtil.wfdOperationProgressDialog.isShowing()) {
	                Log.d(TAG, "clientProgressDialog isShowing");
	            } else {
	                Log.d(TAG, "clientProgressDialog not isShowing");
	            }
	        } else {
	            Log.d(TAG, "clientProgressDialog == null");
	        }

            final DeviceListFragment fragmentList = (DeviceListFragment) getFragmentManager()
                    .findFragmentById(R.id.frag_list);

	        switch (msg.what) {

	            case PLAY_CALLBACK: {
	                playpauseButton.setChecked(false);
	                standbyresumeButton.setChecked(true);
	                if (WfdOperationUtil.wfdOperationProgressDialog != null && WfdOperationUtil.wfdOperationProgressDialog.isShowing()) {
	                    WfdOperationUtil.wfdOperationProgressDialog.dismiss();
	                    Log.d(TAG, "clientProgressDialog dismissed");
	                }
	            }
	            break;
	            case PAUSE_CALLBACK: {
                    playpauseButton.setChecked(true);
                    standbyresumeButton.setChecked(true);
                    if (WfdOperationUtil.wfdOperationProgressDialog != null && WfdOperationUtil.wfdOperationProgressDialog.isShowing()) {
                        WfdOperationUtil.wfdOperationProgressDialog.dismiss();
                        Log.d(TAG, "clientProgressDialog dismissed");
                    }
                }
                break;
	            case STANDBY_CALLBACK: {
                    standbyresumeButton.setChecked(false);
                    if (WfdOperationUtil.wfdOperationProgressDialog != null && WfdOperationUtil.wfdOperationProgressDialog.isShowing()) {
                        WfdOperationUtil.wfdOperationProgressDialog.dismiss();
                        Log.d(TAG, "clientProgressDialog dismissed");
                    }
                }
                break;
	            case SET_WFD_FINISHED: {
	                if (msg.arg1 == -1) {
	                    AlertDialog.Builder builder = new AlertDialog.Builder(
	                            WFDClientActivity.this);
	                    builder.setTitle("WFD failed to turn on")
	                    .setMessage("Would you like to exit the application?")
	                    .setCancelable(false)
	                    .setPositiveButton("Yes", new DialogInterface.OnClickListener() {
	                        public void onClick(DialogInterface dialog, int id) {
	                            WFDClientActivity.this.finish();
	                        }
	                    })
	                    .setNegativeButton("No", new DialogInterface.OnClickListener() {
	                        public void onClick(DialogInterface dialog, int id) {
	                            dialog.cancel();
	                        }
	                    });
	                    AlertDialog alert = builder.create();
	                    alert.show();
	                } else if (msg.arg1 == 0) {
	                    //Toast.makeText(WFDClientActivity.this, "WFD turned on", Toast.LENGTH_SHORT).show();
	                }
	            }
	            break;
	            case INVALID_WFD_DEVICE: {
	                AlertDialog.Builder builder = new AlertDialog.Builder(WFDClientActivity.this);
	                builder.setTitle("Selected device does not support WFD")
	                .setMessage("Please select another device to start a WFD session")
	                .setCancelable(false)
	                .setPositiveButton("Ok", new DialogInterface.OnClickListener() {
	                    public void onClick(DialogInterface dialog, int id) {
	                        dialog.cancel();
	                    }
	                });
	                AlertDialog alert = builder.create();
	                alert.show();
	            }
	            break;
	            case SERVICE_BOUND: {
	                WFDClientActivity.this.mActionListener = WfdOperationUtil.createmActionListener(eventHandler);
	                try {
	                    //set local device type
	                    int setDeviceTypeRet = ServiceUtil.getInstance().setDeviceType(localDeviceType);
	                    if (setDeviceTypeRet == 0) {
	                        Log.d(TAG, "mWfdService.setDeviceType called.");
	                    } else {
	                        Log.d(TAG, "mWfdService.setDeviceType failed with error code: "
	                                + setDeviceTypeRet);
	                    }

	                    //verify session state
	                    WfdStatus wfdStatus = ServiceUtil.getInstance().getStatus();
	                    Log.d(TAG, "wfdStatus.state= " + wfdStatus.state);
	                    if (wfdStatus.state == WfdEnums.SessionState.INVALID.ordinal() ||
	                            wfdStatus.state == WfdEnums.SessionState.INITIALIZED.ordinal()) {
	                        Log.d(TAG, "wfdStatus.state is INVALID or INITIALIZED");
	                        setGoneControlButtons();
	                    } else {
	                        setVisibleControlButtons();
	                    }


	                } catch (RemoteException e) {
	                    Log.e(TAG, "WfdService init() failed", e);
	                    return;
	                }
	                Toast.makeText(getApplicationContext(), "WFD service connected",
	                        Toast.LENGTH_SHORT).show();
	            }
	            break;
                case CLEAR_UI: {
                    if (startSessionProgressDialog != null) {
                        startSessionProgressDialog.dismiss();
                    }
                    if(!modeSource) {
                       Log.e(TAG, "Calling finish on SurfaceActivity");
                       try {
                             finishActivity (START_SURFACE);
                       } catch (ActivityNotFoundException e) {
                           Log.e(TAG, "Surface Activity not yet started/already finished");
                       }
                    }
                }
                //fall through to TEARDOWN_CALLBACK case handling for cleanup
	            case TEARDOWN_CALLBACK: {
	                if (WfdOperationUtil.wfdOperationProgressDialog != null && WfdOperationUtil.wfdOperationProgressDialog.isShowing()) {
	                    Log.d(TAG, "EventHandler: teardownCallback- dismiss clientProgressDialog");
	                    WfdOperationUtil.wfdOperationProgressDialog.dismiss();
	                }

                        if (startSessionProgressDialog != null) {
                            startSessionProgressDialog.dismiss();
                        }

	                try {
                            if(ServiceUtil.getmServiceAlreadyBound()) {
	                        ServiceUtil.getInstance().deinit();
                            }
	                } catch (RemoteException e) {
                        Log.e(TAG, "EventHandler: teardownCallback- Remote exception when calling deinit()", e);
	                }
                        cancelDisconnect();
//	                DeviceListFragment fragmentList = (DeviceListFragment) getFragmentManager()
//	                .findFragmentById(R.id.frag_list);
	                if (fragmentList != null) {
	                    Log.d(TAG, "EventHandler: teardownCallback- teardown fragment");
	                    fragmentList.doTeardown();
	                }
	                Log.d(TAG, "EventHandler: teardownCallback- setGoneControlButtons");
	                setGoneControlButtons();
	                enableDisableUibcButton.setChecked(false);
                        selectTransportButton.setChecked(false);
                        toggleDSButton.setChecked(false);

	                Log.d(TAG, "EventHandler: teardownCallback-  control buttons gone");
	                wifiUtil.send_wfd_set(true, WFDDeviceType.getValue(localDeviceType));
                        wifiUtil.setMiracastMode(NetManagerIF.MIRACAST_DISABLED);
	                Toast.makeText(WFDClientActivity.this, "WiFi Direct Teardown",
	                        Toast.LENGTH_SHORT).show();
                        unbindWfdService();
	                Log.d(TAG, "EventHandler: teardownCallback- completed");
	                inSession = false;
                        mModeDS = false;
	            }
	            break;
	            case START_SESSION_ON_UI: {
	                if (startSessionProgressDialog != null
	                        && startSessionProgressDialog.isShowing()) {
	                    startSessionProgressDialog.dismiss();
	                    Log.d(TAG, "EventHandler: startSessionOnUI- progress dialog closed");
	                }
	                if (modeSource) {
	                    Log.d(TAG, "EventHandler: startSessionOnUI- device is source, setVisibleControlButtons");
	                    setVisibleControlButtons();
	                }
	                wifiUtil.send_wfd_set(false, WFDDeviceType.getValue(localDeviceType));
	                Log.d(TAG, "EventHandler: startSessionOnUI- completed");
	            }
	            break;
	            case UIBC_ACTION_COMPLETED: {
                    enableDisableUibcButton.toggle();
                       if (enableDisableUibcButton.isChecked()) {
                           if (WfdOperationUtil.wfdOperationProgressDialog != null && WfdOperationUtil.wfdOperationProgressDialog.isShowing()) {
                               WfdOperationUtil.wfdOperationProgressDialog.dismiss();
                               Log.d(TAG,"clientProgressDialog dismissed on start UIBC successful");
                           }
                       } else {
                           if (WfdOperationUtil.wfdOperationProgressDialog != null && WfdOperationUtil.wfdOperationProgressDialog.isShowing()) {
                               WfdOperationUtil.wfdOperationProgressDialog.dismiss();
                               Log.d(TAG,"clientProgressDialog dismissed on stop UIBC successful");
                           }
                    }
                    Log.d(TAG, "EventHandler: uibcActionCompleted- completed");
                }
                break;
                case RTP_TRANSPORT_NEGOTIATED: {
                    Log.d(TAG,"EventHandler : RTP Port Negotiation Successful");
                    if (WfdOperationUtil.wfdOperationProgressDialog != null && WfdOperationUtil.wfdOperationProgressDialog.isShowing()) {
                        Log.d(TAG, "EventHandler: TCP/ UDP Success - dismiss clientProgressDialog");
                        WfdOperationUtil.wfdOperationProgressDialog.dismiss();
                    }
                    selectTransportButton.toggle();
                }
                break;
                case RTP_TRANSPORT_NEGOTIATED_FAIL: {
                    Log.d(TAG,"EventHandler : RTP Port Negotiation Failed");
                    if (WfdOperationUtil.wfdOperationProgressDialog != null && WfdOperationUtil.wfdOperationProgressDialog.isShowing()) {
                        Log.d(TAG, "EventHandler: TCP/ UDP Failure - dismiss clientProgressDialog");
                        WfdOperationUtil.wfdOperationProgressDialog.dismiss();
                    }
                }
                break;
                case NetManagerIF.WIFI_UTIL_RETRY_CHANNEL: {
                    Toast.makeText(WFDClientActivity.this, "Channel lost. Trying again",
                            Toast.LENGTH_LONG).show();
                    resetData();
                }
                break;
                case NetManagerIF.WIFI_UTIL_CHANNAL_LOST_PERMANENTLY: {
                    Toast.makeText(
                            WFDClientActivity.this,
                            "Severe! Channel is probably lost permanently. Try Disable/Re-Enable P2P.",
                            Toast.LENGTH_LONG).show();
                }
                break;
                case NetManagerIF.WIFI_UTIL_P2P_STATE_CHANGED: {
                    setIsWifiP2pEnabled(wifiUtil.isEnabled());
                }
                break;
                case NetManagerIF.WIFI_UTIL_DISCONNECTED: {
                    resetData();
                }
                break;
                case NetManagerIF.WIFI_UTIL_CONNECTION_INFO: {
                    WfdEnums.NetType netType = WfdEnums.NetType.values()[msg.arg1];
                    fragmentList.onConnectionInfoAvailable(netType, (WifiP2pInfo)msg.obj);
                }
                break;
                case NetManagerIF.WIFI_UTIL_PEERS_CHANGED: {
                    Collection<NetDeviceIF> peers = (Collection<NetDeviceIF>)msg.obj;
                    fragmentList.onPeersChanged(peers);
                }
                break;
                case NetManagerIF.WIFI_UTIL_CONNECTED_PEERS_CHANGED: {
                    // nothing to do, for multisink only
                }
                break;
                case NetManagerIF.WIFI_UTIL_LOCAL_DEVICE_CHANGED: {
                    updateThisDevice((NetDeviceIF)msg.obj);
                }
                break;
                default:
                    Log.e(TAG, "Unknown event received: " + msg.what);
            }
        }
    }

    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
              Log.d(TAG, "onActivityResult() called with "+ resultCode +"becasue of activity"
                 + "started with " + requestCode);
            if(requestCode == START_SURFACE && resultCode == RESULT_CANCELED) {
               Log.e(TAG, "Surface Activity has been destroyed , clear up UI");
               Message messageClearUI = eventHandler.obtainMessage(CLEAR_UI);
               eventHandler.sendMessage(messageClearUI);
            }
    }
    public void showHideWfdSurface() {
        startActivityForResult(new Intent(this, SurfaceActivity.class), START_SURFACE);
    }
}
