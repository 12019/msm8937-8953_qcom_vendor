/*=============================================================================
Copyright (c) 2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

package com.qualcomm.qti.securemsm.mdtp.ServerSigner.ASN1;

import android.util.Log;

public class MdtpMsgBuilder {
	private static final String TAG = "Mdtp.ServerSigner: ASN1";

	static {
		System.loadLibrary("mdtpdemojni");
	}

	private static native byte[] nativeBuildSetStateMsg(
			boolean active, boolean firmwareLockSelect, boolean simLockSelect, byte[] idToken);

	private static native byte[] nativeBuildSetLockMsg(
			boolean lock, boolean firmwareLockSelect, boolean simLockSelect, byte[] idToken);

	private static native byte[] nativeBuildSetKillMsg(boolean kill, boolean SimSelect, byte[] idToken);

	/**
	 * Build MDTP Set_State message in ASN1 format.
	 * The Set_State message is used for MDTP activation and deactivation.
	 * @param[in]  active  true for activation, false for deactivation.
	 * @param[in]  firmwareLockSelect  true if firmware lock should be used (currently: always).
	 * @param[in]  simLockSelect  true if sim lock should be activated as well.
	 * @param[in]  idToken the ID token.
	 */
	public static byte[] buildSetStateMsg(boolean active, boolean firmwareLockSelect, boolean simLockSelect, byte[] idToken) {
		return nativeBuildSetStateMsg(active, firmwareLockSelect, simLockSelect, idToken);
	}

	/**
	 * Build MDTP Set_Lock message in ASN1 format.
	 * The Set_Lock message is used mainly for MDTP SIM lock and unlock.
	 * @param[in]  lock  true for lock, false for unlock.
	 * @param[in]  firmwareLockSelect  true if firmware should be locked/unlocked (currently: never).
	 * @param[in]  simLockSelect  true if sim should be locked/unlocked.
	 * @param[in]  idToken the ID token.
	 */
	public static byte[] buildSetLockMsg(boolean lock, boolean firmwareLockSelect, boolean simLockSelect, byte[] idToken) {
		return nativeBuildSetLockMsg(lock, firmwareLockSelect, simLockSelect, idToken);
	}

	/**
	 * Build MDTP Set_Kill message in ASN1 format.
	 * The Set_Kill message is used for MDTP Kill (E911 mode) and Restore.
	 * @param[in]  kill  true for Kill (E911 mode), false for Restore.
	 * @param[in]  SimSelect  true if SIM functionality should be killed (currently: always).
	 * @param[in]  idToken the ID token.
	 */
	public static byte[] buildSetKillMsg(boolean kill, boolean SimSelect, byte[] idToken) {
		return nativeBuildSetKillMsg(kill, SimSelect, idToken);
	}
}
