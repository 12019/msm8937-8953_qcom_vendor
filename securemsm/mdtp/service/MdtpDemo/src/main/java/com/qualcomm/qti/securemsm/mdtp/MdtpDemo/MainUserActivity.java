/*=============================================================================
Copyright (c) 2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

package com.qualcomm.qti.securemsm.mdtp.MdtpDemo;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Bundle;
import android.view.View;
import android.util.Log;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

/**
 * This class provides the UI for the device user to activate or
 * deactivate the device
 */
public class MainUserActivity extends Activity {

	public static String TAG = "MdtpMainUserActivity";

	static final int  CHECK_BIND_CMD = 1;
	static final int  ACTIVATE_CMD = 2;
	static final int  DEACTIVATE_CMD = 3;
	static final int  DEACTIVATE_LCL_CMD = 4;

	/**
	 * Offload MDTP services to a non-UI thread
	 */
	private class SendActivateMsg extends AsyncTask<Integer, Void, Integer> {
		private Dialog mOverlayDialog;
		private Context mContext;
		public SendActivateMsg(Context context) {
			mContext = context;
			mOverlayDialog = new Dialog(context, android.R.style.Theme_Panel);
			mOverlayDialog.setCancelable(false);
		}

		protected Integer doInBackground(Integer... ints) {
			try {
				if (ints[0] == CHECK_BIND_CMD) {
					Server.getInstance().initialize(mContext);

					/** Check if application has successfully bound to MDTP service */
					int i;
					for (i = 0; (Server.getInstance().isBound() == false) && (i < 5); i++)
						Thread.sleep(100);

					/** not bound to service */
					if (i == 3)
						return -CHECK_BIND_CMD;
				} else if (ints[0] == ACTIVATE_CMD)
					Server.getInstance().activate(mContext, Device.getInstance().getEnableSimLock());
				else if (ints[0] == DEACTIVATE_CMD)
					Server.getInstance().deactivate(mContext, Device.getInstance().getEnableSimLock());
				else if (ints[0] == DEACTIVATE_LCL_CMD)
					Device.getInstance().deactivateLocal(mContext);
			} catch (Exception e) {
				return -1*ints[0];
			}
			return 0;
		}

		protected void onPreExecute() {
			mButtonDeactivate.setEnabled(false);
			mOverlayDialog.show();
			mSpinner.setVisibility(View.VISIBLE);
		}

		protected void onPostExecute(Integer sts) {
			mSpinner.setVisibility(View.GONE);
			mOverlayDialog.dismiss();
			String msg = null;
			if (sts == -CHECK_BIND_CMD) {
				msg = "Failed to bind to MDTP service";
			}
			if (sts == -ACTIVATE_CMD) {
				if (Device.getInstance().getEnableSimLock())
					msg = "Activate with SIM lock failed - check for SIM";
				else
					msg = "Activate failed";
			} else if (sts == -DEACTIVATE_CMD) {
				if (Device.getInstance().getEnableSimLock())
					msg = "Deactivate with SIM unlock failed - check for SIM";
				else
					msg = "Deactivate failed";
			} else if (sts == -DEACTIVATE_LCL_CMD) {
				msg = "Local deactivate failed";
			}
			if (msg != null)
				Toast.makeText(mContext, msg, Toast.LENGTH_LONG).show();
			updateState();
		}
	}

	private TextView mTextViewState;
	private Button mButtonConfig;
	private Button mButtonActivate;
	private Button mButtonDeactivate;
	private Button mButtonLocalDeactivate;
	private ProgressBar mSpinner;

	/**
	 * Show appropriate buttons when MDTP activated
	 */
	private void setActivated() {
		mTextViewState.setText(getString(R.string.state_activated));
		mButtonConfig.setEnabled(false);
		mButtonActivate.setEnabled(false);
		if (Server.getInstance().getAllowMdtpDeactivation()) {
			mButtonDeactivate.setEnabled(true);
			if (Device.getInstance().getEnableLocalPinAuthentication())
				mButtonLocalDeactivate.setEnabled(true);
			else
				mButtonLocalDeactivate.setEnabled(false);
		} else {
			mButtonDeactivate.setEnabled(false);
			mButtonLocalDeactivate.setEnabled(false);
		}
	}

	/**
	 * Show appropriate buttons when MDTP deactivated
	 */
	private void setDeactivated() {
		mTextViewState.setText(getString(R.string.state_deactivated));
		mButtonConfig.setEnabled(true);
		mButtonActivate.setEnabled(true);
		mButtonDeactivate.setEnabled(false);
		mButtonLocalDeactivate.setEnabled(false);
	}

	/**
	 * Update view according to the current MDTP state
	 */
	private void updateState() {
		if (Server.getInstance().isActivated())
			setActivated();
		else
			setDeactivated();
	}

	/**
	 * Create view
	 */
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		Log.d(TAG, "onCreate");

		setContentView(R.layout.activity_main_user);

		mTextViewState = (TextView) findViewById(R.id.textView_mdtp_state);
		mButtonConfig = (Button)findViewById(R.id.button_configuration);
		mButtonActivate = (Button)findViewById(R.id.button_service_activate);
		mButtonDeactivate = (Button)findViewById(R.id.button_service_deactivate);
		mButtonLocalDeactivate = (Button)findViewById(R.id.button_local_deactivate);

		mSpinner = (ProgressBar)findViewById(R.id.progressBar1);
		mSpinner.setVisibility(View.GONE);

		Server.getInstance().loadState(this);
		Device.getInstance().loadState(this);
	};

	/**
	 * Start view
	 */
	@Override
	protected void onStart() {
		super.onStart();
		Log.d(TAG, "onStart");
		Server.getInstance().start(this);
	}

	/**
	 * Stop view
	 */
	@Override
	protected void onStop() {
		super.onStop();
		Log.d(TAG, "onStop");
		Server.getInstance().storeState(this);
	}

	/**
	 * Resume view
	 */
	@Override
	protected void onResume() {
		super.onResume();
		Log.d(TAG, "onResume");
		Server.getInstance().bind(this);
		new SendActivateMsg(this).execute(CHECK_BIND_CMD);
	}

	/**
	 * Pause view
	 */
	@Override
	protected void onPause() {
		super.onPause();
		Log.d(TAG, "onPause");
		Server.getInstance().unBind(this);
	}

	/**
	 * Destory view
	 */
	@Override
	protected void onDestroy() {
		super.onDestroy();
		Log.d(TAG, "onDestroy");
	}

	/**
	 * Configuration button handler
	 */
	public void oclConfiguration(View v) {
		Intent intent = new Intent(this, ConfigurationActivity.class);
		startActivity(intent);
	}

	/**
	 * Remote service activation button handler
	 */
	public void oclServiceActivate(View v) {
		new SendActivateMsg(this).execute(ACTIVATE_CMD);
	}

	/**
	 * Remote service deactivation button handler
	 */
	public void oclServiceDeactivate(View v) {
		new SendActivateMsg(this).execute(DEACTIVATE_CMD);
	}

	/**
	 * Local deactivation button handler
	 */
	public void oclLocalDeactivate(View v) {
		new SendActivateMsg(this).execute(DEACTIVATE_LCL_CMD);
	}

	/**
	 * Invoke server emulator button handler
	 */
	public void oclServiceConsole(View v) {
		Intent intent = new Intent(this, ServerActivity.class);
		startActivity(intent);
	}

	/**
	 * Invoke setup wizard emulator button handler
	 */
	public void oclSetupWizard(View v) {
		Intent intent = new Intent(this, SetupWizardActivity.class);
		startActivity(intent);
	}
}
