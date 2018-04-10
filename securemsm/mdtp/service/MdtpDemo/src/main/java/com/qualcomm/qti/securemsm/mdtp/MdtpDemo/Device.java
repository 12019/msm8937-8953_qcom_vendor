/*=============================================================================
Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

package com.qualcomm.qti.securemsm.mdtp.MdtpDemo;

import android.content.Context;

import com.qualcomm.qti.securemsm.mdtp.MdtpApi.MdtpConfig;
import com.qualcomm.qti.securemsm.mdtp.MdtpApi.MdtpFwlockConfig;

import android.util.Log;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.Random;

/**
 * This class provides the on-target device functionality.
 * It stores the device related configuration values and
 * gives access to the MDTP local deactivation and
 * MDTP local configuration update functions
 */
public class Device {

	/**
	 * Persistent configuration data
	 */
	private class Config implements Serializable {
		boolean mEnableLocalPinAuthentication = true;
		String mPin = "11111";
		byte mVerifyRatio = 10;
		boolean mEnableSimLock = false;
		String[] mFilesToProtect = new String[0];
		public void defaults() {
			mEnableLocalPinAuthentication = true;
			mPin = "11111111";
			mVerifyRatio = 10;
			mEnableSimLock = false;
			mFilesToProtect = new String[0];
		}
	}

	public static String TAG = "MdtpDevice";
	private static String FILENAME = "device_file";

	/** Singleton instance */
	private static Device instance = null;

	/** Class private data */
	private Config mConfig = null;
	private boolean mIsStarted = false;

	/**
	 * Get reference to singleton
	 */
	public static Device getInstance() {
		if(instance == null) {
			Log.d(TAG, "Instantiate Device");
			instance = new Device();
		}
		return instance;
	}

	/**
	 * Protect constructor
	 */
	private Device() {
		mConfig = new Config();
	}

	/**
	 * Load persistent data from internal file
	 */
	public void loadState(Context ctx) {
		try {
			/** Load configuration from internal file */
			FileInputStream fis = ctx.openFileInput(FILENAME);
			ObjectInputStream ois = new ObjectInputStream(fis);
			mConfig.mEnableLocalPinAuthentication = ois.readBoolean();
			mConfig.mEnableSimLock = ois.readBoolean();
			mConfig.mPin = (String)ois.readObject();
			mConfig.mVerifyRatio = ois.readByte();
			mConfig.mFilesToProtect = (String[])ois.readObject();
			ois.close();

		} catch(FileNotFoundException e) {
			mConfig.defaults();

		} catch(IOException e) {
			throw new RuntimeException(e);

		} catch(ClassNotFoundException e) {
			throw new RuntimeException(e);
		}
	}

	/**
	 * Start the MDTP binder service
	 */
	public void start(Context ctx) {
		if (!mIsStarted) {
			MdtpApiSingleton.getInstance().start(ctx);
			mIsStarted = true;
		}
	}

	/**
	 * Bind to the MDTP binder service
	 */
	public void bind(Context ctx) {
		MdtpApiSingleton.getInstance().bind(ctx);
	}

	/**
	 * Unbind the MDTP binder service
	 */
	public void unBind(Context ctx) {
		MdtpApiSingleton.getInstance().unBind(ctx);
	}

	/**
	 * Configuration setters & getters
	 */
	public boolean getEnableLocalPinAuthentication() {
		return mConfig.mEnableLocalPinAuthentication;
	}

	public void setEnableLocalPinAuthentication(boolean b) {
		mConfig.mEnableLocalPinAuthentication = b;
	}

	public String getPin() {
		return mConfig.mPin;
	}

	public void setPin(String s) {
		mConfig.mPin = s;
	}

	public byte getVerifyRatio() {
		return mConfig.mVerifyRatio;
	}

	public void setVerifyRatio(byte b) {
		mConfig.mVerifyRatio =b;
	}

	public boolean getEnableSimLock() {
		return mConfig.mEnableSimLock;
	}

	public void setEnableSimLock(boolean b) {
		mConfig.mEnableSimLock = b;
	}

	public String[] getFilesToProtect() {
		return mConfig.mFilesToProtect;
	}

	public void setFilesToProtect(String[] s) {
		mConfig.mFilesToProtect = s;
	}

	/**
	 * Store configuration in an internal file
	 */
	public void storeState(Context ctx) {
		try {
			FileOutputStream fos = ctx.openFileOutput(FILENAME, Context.MODE_PRIVATE);
			ObjectOutputStream oos = new ObjectOutputStream(fos);

			oos.writeBoolean(mConfig.mEnableLocalPinAuthentication);
			oos.writeBoolean(mConfig.mEnableSimLock);
			oos.writeObject(mConfig.mPin);
			oos.writeByte(mConfig.mVerifyRatio);
			oos.writeObject(mConfig.mFilesToProtect);

			oos.close();
		} catch (IOException e) {
			throw new RuntimeException(e);
		}
	}

	/**
	 * Generate a 8 digit random value
	 */
	public String randomPin() {
		mConfig.mPin = String.format("%08d", new Random().nextInt(99999999));
		return mConfig.mPin;
	}

	/**
	 * Reset configuration to default values
	 */
	public void reset() {
		mConfig.defaults();
	}

	/**
	 * Update device configuration
	 */
	public void updateDeviceConfig() {
		MdtpConfig cfg = new MdtpConfig();
		cfg.mEnableLocalPinAuthentication = mConfig.mEnableLocalPinAuthentication;
		cfg.mPin = mConfig.mPin;
		MdtpApiSingleton.getInstance().setConfig(cfg);

		MdtpFwlockConfig[] fwc = new MdtpFwlockConfig[1];
		fwc[0] = new MdtpFwlockConfig();
		fwc[0].mPartitionName = "system";
		fwc[0].mHashMode = MdtpFwlockConfig.FWLOCK_MODE_FILES;
		fwc[0].mForceVerifyBlock = new byte[0];
		fwc[0].mVerifyRatio = mConfig.mVerifyRatio;
		fwc[0].mFilesToProtect = mConfig.mFilesToProtect;
		MdtpApiSingleton.getInstance().fwLockConfig(fwc);
	}

	/**
	 * Locally deactivate an activated device using a PIN
	 */
	public void deactivateLocal(Context ctx) {
		MdtpApiSingleton.getInstance().deactivateLocal(mConfig.mPin);
		Server.getInstance().setAllowWizardUnblock(ctx, true);
	}
}
