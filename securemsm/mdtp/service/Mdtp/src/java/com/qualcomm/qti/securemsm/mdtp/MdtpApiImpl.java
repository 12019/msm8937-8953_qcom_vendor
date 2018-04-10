/*=============================================================================
Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
package com.qualcomm.qti.securemsm.mdtp;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

public class MdtpApiImpl implements MdtpApi {
	private static final String TAG = "MdtpApiImpl";
	private static final String SERVICE_NAME = "mdtp";
	private Intent mServiceIntent;
	private IMdtpService mIMdtpApi = null;  /** Binder class created by AIDL */
	private boolean mIsStarted = false;
	private boolean mIsBound = false;

	static final int STATE_ACTIVATED_MSK = 0x00000001; /** Bitmask offset of activated state in mdtp state_mask */
	static final int STATE_SIMLOCKED_MSK = 0x00000002; /** Bitmask offset of simlocked state in mdtp state_mask */
	static final int STATE_E911_MODE_MSK = 0x00000004; /** Bitmask offset of E911 state in mdtp state_mask */
	static final int STATE_TAMPERED_MSK =  0x00000008; /** Bitmask offset of tampered state in mdtp state_mask */

	/**
	 * This class represents the actual service connection. It casts the bound
	 * stub implementation of the service to the AIDL interface.
	 */
	private ServiceConnection mConnection = new ServiceConnection() {

		/** Called when the connection with the service is established */
		public void onServiceConnected(ComponentName className, IBinder service) {

			/** Gets an instance of the IRemoteInterface, which we can use to call on the service */
			Log.d(TAG, "MdtpService connected");
			if (mIMdtpApi == null) {
				mIMdtpApi = IMdtpService.Stub.asInterface(service);
			}
		}

		/** Called when the connection with the service disconnects unexpectedly */
		public void onServiceDisconnected(ComponentName className) {
			Log.e(TAG, "MdtpService disconnected");
			mIMdtpApi = null;
		}
	};

	/*
	 *  Constructor
	 */
	public MdtpApiImpl() {
		Log.d(TAG, "MdtpApiImpl CTOR");
		mServiceIntent = new Intent();
		mServiceIntent.setClassName("com.qualcomm.qti.securemsm.mdtp",
				"com.qualcomm.qti.securemsm.mdtp.MdtpService");
	}

	/**
	 * Starts the MDTP Service application
	 * @param[in]  ctx  context of the calling activity
	 * @Exception RuntimeException
	 */
	public void start(Context ctx) throws RuntimeException {
		Log.d(TAG, "Starting service, mIsStarted="+mIsStarted);
		if (ctx == null)
			throw new RuntimeException(new NullPointerException());

		if (mServiceIntent == null)
			throw new RuntimeException(new NullPointerException());

		if (mIsStarted)
			throw new RuntimeException("MdtpService already started");

		ctx.startService(mServiceIntent);
		mIsStarted = true;
	}

	/**
	 * Stops the MDTP Service application
	 * @param[in]  ctx  context of the calling activity
	 * @Exception RuntimeException
	 */
	public void stop(Context ctx) throws RuntimeException {
		Log.d(TAG, "Stopping service, mIsStarted="+mIsStarted);
		if (ctx == null)
			throw new RuntimeException(new NullPointerException());

		if (mServiceIntent == null)
			throw new RuntimeException(new NullPointerException());

		if (!mIsStarted)
			throw new RuntimeException("MdtpService not started");

		ctx.stopService(mServiceIntent);
		mIsBound = false;
		mIsStarted = false;
	}

	/**
	 * Binds the connection to the system service application
	 * @param[in]  ctx  context of the calling activity
	 * @Exception RuntimeException
	 */
	public void bind(Context ctx) throws RuntimeException {
		if (mServiceIntent == null || mConnection == null)
			throw new RuntimeException(new NullPointerException());

		if (ctx == null)
			throw new RuntimeException(new NullPointerException());

		if (!mIsStarted)
			throw new RuntimeException("MdtpService not started");

		if (mIsBound)
			throw new RuntimeException("MdtpService already bound");

		if (ctx.bindService(mServiceIntent, mConnection, Context.BIND_AUTO_CREATE)) {
			Log.d(TAG, "Binding successfull");
			mIsBound = true;
		}
		else {
			Log.d(TAG, "Binding FAILED !!!!");
			throw new RuntimeException("MdtpService bind failed");
		}
	}

	/**
	 * Unbinds the connection from the system service application
	 * @param[in]  ctx  context of the calling activity
	 * @Exception RuntimeException
	 */
	public void unBind(Context ctx) throws RuntimeException {
		if ((mConnection == null) || (mIMdtpApi == null))
			throw new RuntimeException(new NullPointerException());

		if (ctx == null)
			throw new RuntimeException(new NullPointerException());

		if (!mIsBound)
			throw new RuntimeException("MdtpService not bound");
		ctx.unbindService(mConnection);
		mIsBound = false;
	}

	/**
	 * Checks if the API has been successfully bound to the MDTP service.
	 * @return true / false
	 */
	public boolean isBound() {
		if (!mIsBound)
			return false;
		return(mIMdtpApi != null);
	}

	/**
	 * Returns the current state of the MDTP.
	 * @return MdtpState
	 * @Exception RuntimeException
	 */
	@Override
	public MdtpState getState() throws RuntimeException {
		if (!mIsBound)
			throw new RuntimeException("MdtpService not bound");
		try {
			int[] state_mask = new int[1];
			if (mIMdtpApi.getState(state_mask) != 0)
				throw new RuntimeException("getState failed");
			else {
				MdtpState state = new MdtpState();

				state.mActivated = ((state_mask[0] & STATE_ACTIVATED_MSK) != 0) ? STATE_ACTIVATED : STATE_DEACTIVATED;
				state.mSimLocked = ((state_mask[0] & STATE_SIMLOCKED_MSK) != 0) ? STATE_SIMLOCKED : STATE_SIMUNLOCKED;
				state.mEmergencyOnly = ((state_mask[0] & STATE_E911_MODE_MSK) != 0) ? STATE_E911_MODE : STATE_NOT_E911_MODE;
				state.mTampered = ((state_mask[0] & STATE_TAMPERED_MSK) != 0) ? STATE_TAMPERED : STATE_UNTAMPERED;

				return state;
			}
		} catch (RemoteException e) {
			throw new RuntimeException(e);
		}
	}

	/**
	 * Returns the version of the MDTP library.
	 * @return MdtpVersion
	 * @Exception RuntimeException
	 */
	@Override
	public MdtpVersion getVersion() throws RuntimeException {
		if (!mIsBound)
			throw new RuntimeException("MdtpService not bound");
		try {
			int[] libV = new int[1];
			int[] tzV = new int[1];
			if (mIMdtpApi.getVersion(libV, tzV) != 0)
				throw new RuntimeException("getVersion failed");
			else {
				MdtpVersion version = new MdtpVersion();
				version.mLibVersion = libV[0];
				version.mTzVersion = tzV[0];
				return version;
			}
		} catch (RemoteException e) {
			throw new RuntimeException(e);
		}
	}

	/**
	 * Provides a way to override default configuration for MDTP protection. Default configuration
	 * does not allow local pin authentication. This function can only be called prior to MDTP activation.
	 * @param[in]  config  general configuration of the MDTP protection.
	 * @Exception RuntimeException
	 */
	@Override
	public void setConfig(MdtpConfig config) throws RuntimeException {
		int local_auth = 0;
		if(config.mEnableLocalPinAuthentication){
			local_auth = 1;
		}
		if (!mIsBound)
			throw new RuntimeException("MdtpService not bound");
		try {
			byte[] bytes;
			if(config.mPin == null){
				bytes = new byte[1];
				bytes[0] = 0;
			}else if ((config.mPin.length() == 0) || (config.mPin == "")) {
				bytes = new byte[1];
				bytes[0] = 0;
			} else {
				bytes = config.mPin.getBytes();
			}
			if (mIMdtpApi.setConfig(local_auth, bytes) != 0)
				throw new RuntimeException("setConfig failed");
		} catch (RemoteException e) {
			throw new RuntimeException(e);
		}
	}

	/**
	 * Process a signed message sent to device by MDTP services. Signed messages are used for sensitive operations,
	 * such as MDTP activation, deactivation, etc. The structure of the content and the different messages supported
	 * by the MDTP is described in "MDTP Interface Specification" document. The function will process the message,
	 * validate its authenticity, and execute the command it carries.
	 * @param[in]    message signed message.
	 * @Exception RuntimeException
	 */
	@Override
	public void processSignedMsg(byte[] message) throws RuntimeException {
		if (!mIsBound)
			throw new RuntimeException("MdtpService not bound");
		try {
			if (mIMdtpApi.processSignedMsg(message) != 0)
				throw new RuntimeException("processSignedMsg failed");
		} catch (RemoteException e) {
			throw new RuntimeException(e);
		}
	}

	/**
	 * Deactivates the MDTP protection, using local pin authentication, instead of a signed deactivation message arriving
	 * from MDTP services.  Local pin authentication  must be explicitly allowed, and pin must be set via configuration
	 * prior to MDTP activation. @see mdtp_config.
	 * @param[in]  pin  PIN of MDTP_PIN_LEN digits.
	 * @Exception RuntimeException
	 */
	@Override
	public void deactivateLocal(String pin) throws RuntimeException {
		if (!mIsBound)
			throw new RuntimeException("MdtpService not bound");
		try {
			if (mIMdtpApi.deactivateLocal(pin.getBytes()) != 0)
				throw new RuntimeException("deactivateLocal failed");
		} catch (RemoteException e) {
			throw new RuntimeException(e);
		}
	}

	/**
	 * Get an identity token to uniquely identify a specific instance of MDTP activation/deactivation. Only when MDTP state
	 * is deactivated, and a token has not been previously generated, a new unique token shall be generated and persisted.
	 * Upon MDTP deactivation the identity token is cleared and a subsequent call to this API shall generate a new token.
	 * @return byte[]
	 * @Exception RuntimeException
	 */
	@Override
	public byte[] getIdToken() throws RuntimeException {
		if (!mIsBound)
			throw new RuntimeException("MdtpService not bound");
		try {
			byte[] token = new byte[MDTP_ID_TOKEN_LEN];
			if (mIMdtpApi.getIdToken(token) != 0)
				throw new RuntimeException("getIdToken failed");
			else
				return token;
		} catch (RemoteException e) {
			throw new RuntimeException(e);
		}
	}

	/**
	 * Updates the MDTP with a given RFC5280 CRL
	 * @param[in]  crl  Certificate Revocation List (CRL), according to RFC5280.
	 * @param[in]  cert  The X509 certificate the CRL is signed with. This is to identify who signed the CRL (the root CA or its
	 * subordinates CAs)
	 * @Exception RuntimeException
	 */
	@Override
	public void updateCrl(byte[] crl, byte[] certificate) throws RuntimeException {
		if (!mIsBound)
			throw new RuntimeException("MdtpService not bound");
		try {
			if (mIMdtpApi.updateCrl(crl, certificate) != 0)
				throw new RuntimeException("updateCrl failed");
		} catch (RemoteException e) {
			throw new RuntimeException(e);
		}
	}

	/**
	 * Get the MDTP recovery information.
	 * The recovery information is used to force MDTP into deactivated state, to be used in rare cases where MDTP persistent storage
	 * was corrupted by user action such as a low level device repair which re-flashed all storage.
	 * Notes:
	 * Recovery information <b>must not be kept on device</b>. Instead it should be sent to MDTP services.
	 *  This call allocates a buffer, and returns its pointer and size. The caller is responsible for freeing the buffer when done.
	 * @return byte[]  Buffer holding the recovery information.
	 * @Exception RuntimeException
	 */
	@Override
	public byte[] getRecoveryInfo() throws RuntimeException {
		if (!mIsBound)
			throw new RuntimeException("MdtpService not bound");
		try {
			/** Get info size */
			int[] size = new int[1];
			if (mIMdtpApi.getRecoveryInfoSize(size) != 0)
				throw new RuntimeException("getRecoveryInfo failed");

			/** Allocate array */
			byte[] info = new byte[size[0]];
			if (mIMdtpApi.getRecoveryInfo(info) != 0)
				throw new RuntimeException("getRecoveryInfo failed");
			else
				return info;
		} catch (RemoteException e) {
			throw new RuntimeException(e);
		}
	}

	/*****************************************************************************************************************/
	/* MDTP lock functionality API */
	/*****************************************************************************************************************/

	/**
	 * Provides a way to modify the configuration for Firmware Lock protection.
	 * The configuration received by this function, will be merged with the default configuration.
	 * The only partition that can be contained in a configuration is 'system', and the only mode is @c MDTP_FWLOCK_MODE_FILES.
	 * So, merging the received configuration in this function is mainly merging the file lists of the configurations (default and received).
	 * Also, the higher verify_ratio will be taken.
	 *
	 * This function can only be called prior to MDTP activation.
	 *
	 * Notes:
	 * Default configuration can be set by the OEM when building the MDTP Trusted Component.
	 * Using this function can only add files to the default configuration list, but not remove.
	 *
	 * @param[in]  fwlockConfig   Configuration of Firmware Lock protection.
	 * @Exception RuntimeException
	 */
	@Override
	public void fwLockConfig(MdtpFwlockConfig[] fwlockConfig) throws RuntimeException {
		if (!mIsBound)
			throw new RuntimeException("MdtpService not bound");
		IMdtpFwlockConfig[] iCfg = new IMdtpFwlockConfig[fwlockConfig.length];

		for (int i = 0; i < fwlockConfig.length; i++) {
			iCfg[i] = new IMdtpFwlockConfig();
			iCfg[i].mPartitionName = fwlockConfig[i].mPartitionName;
			iCfg[i].mHashMode = fwlockConfig[i].mHashMode;
			iCfg[i].mVerifyRatio = fwlockConfig[i].mVerifyRatio;
			iCfg[i].mForceVerifyBlock = fwlockConfig[i].mForceVerifyBlock;
			iCfg[i].mFilesToProtect = new String[fwlockConfig[i].mFilesToProtect.length];
			for (int j = 0; j < fwlockConfig[i].mFilesToProtect.length; j++)
				iCfg[i].mFilesToProtect[j] = fwlockConfig[i].mFilesToProtect[j];
		}

		try {
			if (mIMdtpApi.fwLockConfig(iCfg) != 0)
				throw new RuntimeException("fwLockConfig failed");
		} catch (RemoteException e) {
			throw new RuntimeException(e);
		}
	}
}
