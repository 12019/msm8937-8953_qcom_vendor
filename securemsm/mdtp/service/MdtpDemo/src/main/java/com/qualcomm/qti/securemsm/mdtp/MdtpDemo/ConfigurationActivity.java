/*=============================================================================
Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

package com.qualcomm.qti.securemsm.mdtp.MdtpDemo;

import android.app.Activity;
import android.os.Bundle;
import android.text.Editable;
import android.util.Log;
import android.view.inputmethod.InputMethodManager;
import android.view.View;
import android.view.View.OnFocusChangeListener;
import android.view.WindowManager;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

/**
 * This class provides the UI for the local device configuration options
 */
public class ConfigurationActivity extends Activity
implements OnFocusChangeListener, OnCheckedChangeListener {

	public static String TAG = "MdtpConfigurationActivity";

	private CheckBox mCheckBoxUsePin;
	private EditText mEditTextPin;
	private EditText mEditTextRatio;
	private CheckBox mCheckBoxSimLock;
	private EditText[] mEditTextPath = new EditText[8];

	private boolean mHaveErrors = false;

	/**
	 * Hide the keyboard when focus moves from an editable field to any other field
	 */
	public void hideKeyboard() {
		InputMethodManager inputMethodManager =
				(InputMethodManager) this.getSystemService(Activity.INPUT_METHOD_SERVICE);

		/** Find the currently focused view, so we can grab the correct window token from it */
		View view = this.getCurrentFocus();

		/** If no view currently has focus, create a new one, just so we can grab a window token from it */
		if(view == null) {
			view = new View(this);
		}
		inputMethodManager.hideSoftInputFromWindow(view.getWindowToken(), 0);
	}

	/**
	 * Remove focus and hide keyboard when clicking on a checked field
	 */
	public void onCheckedChanged(CompoundButton buttonView, boolean isChecked)
	{
		mEditTextPin.clearFocus();
		mEditTextRatio.clearFocus();
		for (int i = 0; i < 8; i++)
			mEditTextPath[i].clearFocus();
		hideKeyboard();
	}

	/**
	 * Load values from the device's storage into corresponding GUI fields
	 */
	protected void loadValues() {
		mCheckBoxUsePin.setChecked(Device.getInstance().getEnableLocalPinAuthentication());
		mEditTextPin.setText(Device.getInstance().getPin());
		mEditTextRatio.setText(String.valueOf(Device.getInstance().getVerifyRatio()));
		mCheckBoxSimLock.setChecked(Device.getInstance().getEnableSimLock());
		String[] pathList = Device.getInstance().getFilesToProtect();
		if (pathList != null) {
			for (int i = 0; i < 8; i++) {
				if (pathList.length > i && pathList[i].length() > 0)
					mEditTextPath[i].setText(pathList[i]);
				else
					mEditTextPath[i].setText("");
			}
		}
	}

	/**
	 * Create the view
	 */
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		Log.d(TAG, "onCreate");

		setContentView(R.layout.activity_configuration);

		mCheckBoxUsePin = (CheckBox) findViewById(R.id.checkBox_use_local_pin);
		mCheckBoxUsePin.setOnCheckedChangeListener(this);

		mEditTextPin = (EditText) findViewById(R.id.editText_pin);
		mEditTextPin.setOnFocusChangeListener(this);

		mEditTextRatio = (EditText) findViewById(R.id.editText_ratio);
		mEditTextRatio.setOnFocusChangeListener(this);

		mCheckBoxSimLock = (CheckBox) findViewById(R.id.checkBox_sim_lock);
		mCheckBoxSimLock.setOnCheckedChangeListener(this);

		mEditTextPath[0] = (EditText) findViewById(R.id.editText_path1);
		mEditTextPath[1] = (EditText) findViewById(R.id.editText_path2);
		mEditTextPath[2] = (EditText) findViewById(R.id.editText_path3);
		mEditTextPath[3] = (EditText) findViewById(R.id.editText_path4);
		mEditTextPath[4] = (EditText) findViewById(R.id.editText_path5);
		mEditTextPath[5] = (EditText) findViewById(R.id.editText_path6);
		mEditTextPath[6] = (EditText) findViewById(R.id.editText_path7);
		mEditTextPath[7] = (EditText) findViewById(R.id.editText_path8);

		for (int i = 0; i < 8; i++)
			mEditTextPath[i].setOnFocusChangeListener(this);

		loadValues();
	}

	/**
	 * Validate fields when loosing focus from an editable field
	 */
	public void onFocusChange(View v, boolean hasFocus) {
		if (!hasFocus) {
			if (v.getId() == R.id.editText_pin) {
				if (mEditTextPin.getText().toString().length() < 8) {
					mEditTextPin.setError("PIN must be 8 digits");
				}
			} else if (v.getId() == R.id.editText_ratio) {
				if (Integer.valueOf(mEditTextRatio.getText().toString()) > 100)
					mEditTextRatio.setError("Ratio > 100%");
				if (Integer.valueOf(mEditTextRatio.getText().toString()) < 10)
					mEditTextRatio.setError("Ratio < 10%");
			} else if ((v.getId() == R.id.editText_path1) || (v.getId() == R.id.editText_path2) ||
					(v.getId() == R.id.editText_path3) || (v.getId() == R.id.editText_path4) ||
					(v.getId() == R.id.editText_path5) || (v.getId() == R.id.editText_path6) ||
					(v.getId() == R.id.editText_path7) || (v.getId() == R.id.editText_path8)){

				EditText et = (EditText)v;
				if ((et.getText() != null) && (et.getText().toString().length() > 0) &&
						(!et.getText().toString().trim().startsWith("/system/"))) {
					et.setError("Path must begin with '/system/'");
				}
			}
		}
	}

	/**
	 * Stop view
	 */
	@Override
	protected void onStop() {
		super.onStop();
		Log.d(TAG, "onStop");
	}

	/**
	 * Resume view
	 */
	@Override
	protected void onResume() {
		super.onResume();
		Log.d(TAG, "onResume");
		Device.getInstance().bind(this);
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
	 * Pause view
	 */
	@Override
	protected void onPause() {
		super.onPause();
		Log.d(TAG, "onPause");

		if (validateFields()) {
			/**
			 * Update device with values from activity
			 */
			Device.getInstance().setEnableLocalPinAuthentication(mCheckBoxUsePin.isChecked());
			Device.getInstance().setPin(mEditTextPin.getText().toString());
			Device.getInstance().setVerifyRatio(Byte.valueOf(mEditTextRatio.getText().toString()));
			Device.getInstance().setEnableSimLock(mCheckBoxSimLock.isChecked());
			String[] pathList = new String[8];

			for (int i = 0; i < 8; i++)
				pathList[i] = mEditTextPath[i].getText().toString();

			Device.getInstance().setFilesToProtect(pathList);

			/**
			 * Save every thing before leaving
			 */
			Device.getInstance().storeState(this);

			/**
			 * Update MDTP with new config values
			 */
			Device.getInstance().updateDeviceConfig();

			Toast.makeText(this,
					"Configuration updated", Toast.LENGTH_LONG).show();
		} else {
			Toast.makeText(this,
					"Fields have errors, configuration not updated", Toast.LENGTH_LONG).show();
		}
		Device.getInstance().unBind(this);
	}

	/**
	 * Reset to default button handler
	 */
	public void oclReset(View v) {
		Device.getInstance().reset();
		loadValues();
	}

	/**
	 * Generate random PIN button handler
	 */
	public void oclRandomPin(View v) {
		mEditTextPin.setText(Device.getInstance().randomPin());
	}

	/**
	 * Validate content of editable fields
	 */
	private boolean validateFields() {
		mHaveErrors = false;

		if (mEditTextPin.getText().toString().length() < 8) {
			mEditTextPin.setError("PIN must be 8 digits");
			mHaveErrors = true;
		}

		if (Integer.valueOf(mEditTextRatio.getText().toString()) > 100) {
			mEditTextRatio.setError("Ratio > 100%");
			mHaveErrors = true;
		}

		if (Integer.valueOf(mEditTextRatio.getText().toString()) < 10) {
			mEditTextRatio.setError("Ratio < 10%");
			mHaveErrors = true;
		}

		for (int i = 0; i < 8; i++) {
			if ((mEditTextPath[i].getText() != null) && (mEditTextPath[i].getText().toString().length() > 0) &&
					(!mEditTextPath[i].getText().toString().trim().startsWith("/system/"))) {
				mEditTextPath[i].setError("Path must begin with '/system/'");
				mHaveErrors = true;
			}
		}

		return !mHaveErrors;
	}
}