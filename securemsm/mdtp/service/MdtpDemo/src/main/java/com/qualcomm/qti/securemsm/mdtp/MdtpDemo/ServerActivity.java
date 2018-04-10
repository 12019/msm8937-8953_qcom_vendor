/*=============================================================================
Copyright (c) 2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

package com.qualcomm.qti.securemsm.mdtp.MdtpDemo;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.os.AsyncTask;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.RadioGroup;
import android.widget.Toast;

/**
 * This class provides the UI for the off-target server configuration options
 */
public class ServerActivity extends Activity {

	/**
	 * Offload MDTP services to a non-UI thread
	 */
	private class SendActivateMsg extends AsyncTask<Integer, Void, Integer> {
		static final int  ACTIVATE_CMD = 1;
		static final int  DEACTIVATE_CMD = 2;
		static final int  LOCK_CMD = 3;
		static final int  UNLOCK_CMD = 4;
		static final int  KILL_CMD = 5;
		static final int  RESTORE_CMD = 6;

		private Dialog mOverlayDialog;
		private Context mContext;
		private String mStrAction;
		private SendActivateMsg(Context context) {
			mContext = context;
			mOverlayDialog = new Dialog(context, android.R.style.Theme_Panel);
			mOverlayDialog.setCancelable(false);
		}

		protected Integer doInBackground(Integer... ints) {
			try {
				switch(ints[0]) {
				case ACTIVATE_CMD:
					mStrAction = "Activate";
					Server.getInstance().activate(mContext, false);
					break;
				case DEACTIVATE_CMD:
					mStrAction = "Deactivate";
					Server.getInstance().deactivate(mContext, false);
					break;
				case LOCK_CMD:
					mStrAction = "Lock SIM";
					Server.getInstance().lockSim();
					break;
				case UNLOCK_CMD:
					mStrAction = "Unlock SIM";
					Server.getInstance().unlockSim();
					break;
				case KILL_CMD:
					mStrAction = "Kill";
					Server.getInstance().kill();
					break;
				case RESTORE_CMD:
					mStrAction = "Restore";
					Server.getInstance().restore();
					break;
				}
			} catch (Exception e) {
				return -1 * ints[0];
			}
			return 0;
		}

		protected void onPreExecute() {
			mOverlayDialog.show();
			mSpinner.setVisibility(View.VISIBLE);
		}

		protected void onPostExecute(Integer sts) {
			mSpinner.setVisibility(View.GONE);
			mOverlayDialog.dismiss();
			switch(-1*sts) {
			case ACTIVATE_CMD:
				Toast.makeText(mContext,
						"Activate failed", Toast.LENGTH_LONG).show();
				break;
			case DEACTIVATE_CMD:
				Toast.makeText(mContext,
						"Deactivate failed", Toast.LENGTH_LONG).show();
				break;
			case LOCK_CMD:
				Toast.makeText(mContext,
						"Lock failed - either already locked, or no SIM", Toast.LENGTH_LONG).show();
				break;
			case UNLOCK_CMD:
				Toast.makeText(mContext,
						"Unlock failed - either already unlocked, or no SIM", Toast.LENGTH_LONG).show();
				break;
			case KILL_CMD:
				Toast.makeText(mContext,
						"Kill failed - already killed", Toast.LENGTH_LONG).show();
				break;
			case RESTORE_CMD:
				Toast.makeText(mContext,
						"Restore failed - already restored", Toast.LENGTH_LONG).show();
				break;
			default:
				Toast.makeText(mContext,
						mStrAction + " succeeded", Toast.LENGTH_LONG).show();
				updateState();
			}
		}
	}

	private Button mButtonActivate;
	private Button mButtonDeactivate;
	private Button mButtonLockSim;
	private Button mButtonUnlockSim;
	private Button mButtonKill;
	private Button mButtonRestore;
	private RadioGroup mAllowDeactivate;
	private RadioGroup mAllowWizUnblock;
	private ProgressBar mSpinner;

	/**
	 * Show appropriate buttons when MDTP activated
	 */
	private void setActivated() {
		mButtonActivate.setEnabled(false);
		mButtonDeactivate.setEnabled(true);
		mButtonLockSim.setEnabled(true);
		mButtonUnlockSim.setEnabled(true);
		mButtonKill.setEnabled(true);
		mButtonRestore.setEnabled(true);
	}

	/**
	 * Show appropriate buttons when MDTP deactivated
	 */
	private void setDeactivated() {
		mButtonActivate.setEnabled(true);
		mButtonDeactivate.setEnabled(false);
		mButtonLockSim.setEnabled(false);
		mButtonUnlockSim.setEnabled(false);
		mButtonKill.setEnabled(false);
		mButtonRestore.setEnabled(false);
	}

	/**
	 * Update view according to the current MDTP state
	 */
	private void updateState() {
		if (Server.getInstance().isActivated())
			setActivated();
		else
			setDeactivated();
		if (Server.getInstance().getAllowMdtpDeactivation())
			mAllowDeactivate.check(R.id.radio_allowed);
		else
			mAllowDeactivate.check(R.id.radio_notallowed);
		if (Server.getInstance().getAllowWizardUnblock())
			mAllowWizUnblock.check(R.id.radio_wizallowed);
		else
			mAllowWizUnblock.check(R.id.radio_wiznotallowed);
	}

	/**
	 * Create view
	 */
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_server);

		mButtonActivate = (Button)findViewById(R.id.button_activate);
		mButtonDeactivate = (Button)findViewById(R.id.button_deactivate);
		mButtonLockSim = (Button)findViewById(R.id.button_locksim);
		mButtonUnlockSim = (Button)findViewById(R.id.button_unlocksim);
		mButtonKill = (Button)findViewById(R.id.button_killfunc);
		mButtonRestore = (Button)findViewById(R.id.button_restorefunc);
		mAllowDeactivate = (RadioGroup)findViewById(R.id.radio_deactivate);
		mAllowWizUnblock = (RadioGroup)findViewById(R.id.radio_wizunblock);
		mSpinner=(ProgressBar)findViewById(R.id.progressBar2);
		mSpinner.setVisibility(View.GONE);

		try {
			Server.getInstance().initialize(this);
		} catch (Exception e) {
			Toast.makeText(this, "Connection to Safe Switch daemon failed", Toast.LENGTH_LONG).show();
			finish();
		}
	}

	/**
	 * Start view
	 */
	@Override
	protected void onStart() {
		super.onStart();
	}

	/**
	 * Stop view
	 */
	@Override
	protected void onStop() {
		super.onStop();
		Server.getInstance().storeState(this);
	}

	/**
	 * Resume view
	 */
	@Override
	protected void onResume() {
		super.onResume();
		Server.getInstance().bind(this);
		updateState();
	}

	/**
	 * Pause view
	 */
	@Override
	protected void onPause() {
		super.onPause();
		Server.getInstance().unBind(this);
	}

	/**
	 * Activate MDTP button handler
	 */
	public void oclActivate(View v) {
		new SendActivateMsg(this).execute(SendActivateMsg.ACTIVATE_CMD);
	}

	/**
	 * Deactivate MDTP button handler
	 */
	public void oclDeactivate(View v) {
		new SendActivateMsg(this).execute(SendActivateMsg.DEACTIVATE_CMD);
	}

	/**
	 * Lock SIM button handler
	 */
	public void oclLockSim(View v) {
		new SendActivateMsg(this).execute(SendActivateMsg.LOCK_CMD);
	}

	/**
	 * Unlock SIM button handler
	 */
	public void oclUnlockSim(View v) {
		new SendActivateMsg(this).execute(SendActivateMsg.UNLOCK_CMD);
	}

	/**
	 * KIll button handler
	 */
	public void oclKillFunc(View v) {
		new SendActivateMsg(this).execute(SendActivateMsg.KILL_CMD);
	}

	/**
	 * Restore button handler
	 */
	public void oclRestoreFunc(View v) {
		new SendActivateMsg(this).execute(SendActivateMsg.RESTORE_CMD);
	}

	/**
	 * Deactivate radio button handler
	 */
	public void oclAllowDeactivation(View v) {
		int selectedId = mAllowDeactivate.getCheckedRadioButtonId();
		if (selectedId == R.id.radio_allowed)
			Server.getInstance().setAllowMdtpDeactivation(this, true);
		else
			Server.getInstance().setAllowMdtpDeactivation(this, false);
		updateState();
	}

	/**
	 * Setup wizard radio button handler
	 */
	public void oclAllowWizUnblock(View v) {
		int selectedId = mAllowWizUnblock.getCheckedRadioButtonId();
		if (selectedId == R.id.radio_wizallowed)
			Server.getInstance().setAllowWizardUnblock(this, true);
		else
			Server.getInstance().setAllowWizardUnblock(this, false);
	}
}
