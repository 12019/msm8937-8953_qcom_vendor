/*=============================================================================
Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

package com.qualcomm.qti.securemsm.mdtp.MdtpDemo;

import android.content.Context;

import com.qualcomm.qti.securemsm.mdtp.MdtpApi;
import com.qualcomm.qti.securemsm.mdtp.ServerSigner.ASN1.MdtpMsgBuilder;
import com.qualcomm.qti.securemsm.mdtp.ServerSigner.CMS.CMSBuilder;
import com.qualcomm.qti.securemsm.mdtp.ServerSigner.PKI.PKILoader;

import android.util.Log;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * This class emulates an external server on the target device.
 * The main purpose of this class is to demonstrate how to create
 * the signed messages sent to the MDTP service.
 * In a real life implementation the functionality of this class
 * will be implemented off-target, and will access the MDPT functions
 * through an on-target proxy process.
 */
public class Server {

	public static String TAG = "MdtpServer";
	private static String FILENAME = "server_file";

	/** Singleton instance */
	private static Server instance = null;

	/** Class private data */
	private boolean mInitialized = false;

	private byte[] mIdToken = new byte[MdtpApi.MDTP_ID_TOKEN_LEN];
	private boolean mIsActivated = false;
	private boolean mAllowMdtpDeactivation = true;
	private boolean mAllowWizardUnblock = false;

	private boolean mIsStarted = false;

	/**
	 * Get reference to singleton
	 */
	public static Server getInstance() {
		if(instance == null) {
			Log.d(TAG, "Instantiate Server");
			instance = new Server();
		}
		return instance;
	}

	/**
	 * Protect constructor
	 */
	private Server() {}

	/**
	 * Load state from internal file
	 */
	public void loadState(Context ctx) {
		try {
			/** Load activation state from internal file */
			FileInputStream fis = ctx.openFileInput(FILENAME);
			DataInputStream dis = new DataInputStream(fis);
			mAllowWizardUnblock = dis.readBoolean();
			mAllowMdtpDeactivation = dis.readBoolean();
			dis.read(mIdToken);
			fis.close();
		} catch (FileNotFoundException e) {
			mIsActivated = false;
		} catch (IOException e) {
			throw new RuntimeException(e);
		}
	}

	/**
	 * Save state in internal file
	 */
	public void storeState(Context ctx) {
		try {
			FileOutputStream fos = ctx.openFileOutput(FILENAME, Context.MODE_PRIVATE);
			DataOutputStream dos = new DataOutputStream(fos);
			dos.writeBoolean(mAllowWizardUnblock);
			dos.writeBoolean(mAllowMdtpDeactivation);
			dos.write(mIdToken);
			fos.close();
		} catch(IOException e) {
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
	 * Verify if the MDTP service is bound
	 */
	public boolean isBound() {
		if (MdtpApiSingleton.getInstance().isBound()) {
			return true;
		} else {
			return false;
		}
	}

	/**
	 * Initialize operational context. Should be the first method invoked
	 * @param c
	 */
	public void initialize(Context ctx) {
		if (!mInitialized) {

			/** Load certificate chain and ISV private key from PEM coded resource files */
			PKILoader.load(ctx);
			mInitialized = true;
		}
	}

	/**
	 * Setters & Getters
	 */

	public boolean isActivated() {
		mIsActivated = (MdtpApiSingleton.getInstance().getState().mActivated == MdtpApi.STATE_ACTIVATED) ? true : false;
		return mIsActivated;
	}

	public void setAllowMdtpDeactivation(Context ctx, boolean b) {
		mAllowMdtpDeactivation = b;
		storeState(ctx);
	}

	public boolean getAllowMdtpDeactivation() {
		return mAllowMdtpDeactivation;
	}

	public void setAllowWizardUnblock(Context ctx, boolean b) {
		mAllowWizardUnblock = b;
		storeState(ctx);
	}

	public boolean getAllowWizardUnblock() {
		return mAllowWizardUnblock;
	}

	/**
	 * Generate and issue activation message
	 */
	public  void activate(Context ctx, boolean enableSimLock) throws  RuntimeException {
		if (!mIsActivated) {
			/** Refresh Id Token */
			mIdToken = MdtpApiSingleton.getInstance().getIdToken();

			/** Activation message - SET_STATE(activated=1, fwlock=1, simlock=0/1) */
			byte[] activationMsg =
					CMSBuilder.create(MdtpMsgBuilder.buildSetStateMsg(true, true, enableSimLock, mIdToken));
			MdtpApiSingleton.getInstance().processSignedMsg(activationMsg);
			mIsActivated = true;
			mAllowWizardUnblock = true;
			storeState(ctx);
		}
	}

	/**
	 * Generate and issue deactivation message
	 */
	public  void deactivate(Context ctx, boolean enableSimLock) throws  RuntimeException {
		if (mIsActivated) {
			/** Deactivation message - SET_STATE(activated=0, fwlock=1, simlock=0/1) */
			byte[] deactivationMsg =
					CMSBuilder.create(MdtpMsgBuilder.buildSetStateMsg(false, true, enableSimLock, mIdToken));
			MdtpApiSingleton.getInstance().processSignedMsg(deactivationMsg);
			mIsActivated = false;
		}
	}

	/**
	 * Generate and issue lock SIM message
	 */
	public  void lockSim() throws  RuntimeException {
		if (mIsActivated) {
			/** SIM lock message - SET_LOCK(setlock=1, fwlock=0, simlock=1) */
			byte[] simLockMsg =
					CMSBuilder.create(MdtpMsgBuilder.buildSetLockMsg(true, false, true, mIdToken));
			MdtpApiSingleton.getInstance().processSignedMsg(simLockMsg);
		}
	}

	/**
	 * Generate and issue unlock SIM message
	 */
	public  void unlockSim() throws  RuntimeException {
		if (mIsActivated) {
			/** SIM unlock message - SET_LOCK(setlock=0, fwlock=0, simlock=1) */
			byte[] simUnlockMsg =
					CMSBuilder.create(MdtpMsgBuilder.buildSetLockMsg(false, false, true, mIdToken));
			MdtpApiSingleton.getInstance().processSignedMsg(simUnlockMsg);
		}
	}

	/**
	 * Generate and issue kill message
	 */
	public  void kill() throws  RuntimeException {
		/** Kill message - SET_KILL(setkill=1, sim=1) */
		byte[] killMsg =
				CMSBuilder.create(MdtpMsgBuilder.buildSetKillMsg(true, true, mIdToken));
		MdtpApiSingleton.getInstance().processSignedMsg(killMsg);
	}

	/**
	 * Generate and issue restore message
	 */
	public  void restore() throws  RuntimeException {
		/** Restore message - SET_KILL(setkill=0, sim=1) */
		byte[] restoreMsg =
				CMSBuilder.create(MdtpMsgBuilder.buildSetKillMsg(false, true, mIdToken));
		MdtpApiSingleton.getInstance().processSignedMsg(restoreMsg);
	}
}
