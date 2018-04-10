/*=============================================================================
Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

package com.qualcomm.qti.securemsm.mdtp;

import android.content.Context;
/** @addtogroup fwlockEnumtype
@{ */
public interface MdtpApi {

	static final int STATE_ACTIVATED = 0;				/**< MDTP is activated on the device. */
	static final int STATE_DEACTIVATED = 1;				/**< MDTP is not activated on the device.*/
	static final int STATE_SIMLOCKED = 0;				/**< Device is SIM locked. */
	static final int STATE_SIMUNLOCKED = 1;				/**< Device is not SIM locked.*/
	static final int STATE_E911_MODE = 0;				/**< Device is in Emergency only mode. */
	static final int STATE_NOT_E911_MODE = 1;			/**< Device is not in Emergency only mode.*/
	static final int STATE_TAMPERED = 0;				/**< MDTP is in tampered state. */
	static final int STATE_UNTAMPERED = 1;				/**< MDTP is not in tampered state.*/

	static final int MDTP_ID_TOKEN_LEN = 16;			/**< MDTP token length. */
	static final int MDTP_PIN_LEN  = 8;					/**< Allowed length of PIN. */
	static final int MDTP_MAX_MSG_LEN  = 4096;			/**< Maximum message length. */
	static final int MDTP_MAX_CRL_LEN  = 2048;			/**< Maximum CRL length. */
	static final int MDTP_MAX_CERT_LEN = 2048;			/**< Maximum certificate length. */

	/**
	 * MDTP version.
	 */
	public class MdtpVersion {
		public int mLibVersion; /**< MDTP version number for MDTP library. */
		public int mTzVersion;  /**< MDTP version number for the TrustZone component. */
	}

	/**
	 * MDTP state.
	 */
	public class MdtpState {
		public int mActivated;		/**< MDTP is activated or not. */
		public int mSimLocked; 		/**< Device is SIM locked or not. */
		public int mEmergencyOnly; 	/**< Device is in Emergency only mode or not. */
		public int mTampered; 		/**< MDTP is in tampered state or not. */
	}

	/**
	 * MDTP configuration.
	 */
	public class MdtpConfig {
		public boolean mEnableLocalPinAuthentication; /**< Enables local authentication using a PIN. */
		public String mPin;                        /**< PIN provided by the user for local deactivation. */
	}
	/** @} */ /* end_addtogroup fwlockEnumtype */
        /** @addtogroup fwlockDataStructure
        @{ */
	/**
	 * MDTP Firmware Lock partition configuration.
	 */
	public class MdtpFwlockConfig {
		public static final int FWLOCK_MODE_SINGLE = 0; /**< A single hash on the entire partition. */
		public static final int FWLOCK_MODE_BLOCK = 1;  /**< A hash per #MDTP_FWLOCK_BLOCK_SIZE sized block. */
		public static final int FWLOCK_MODE_FILES = 2;  /**< A hash for blocks that contain the files listed in the configuration. */

		public static final int MDTP_FWLOCK_BLOCK_SIZE = 1024*1024*16;    /**< Hashable block size. */
		public static final int FWLOCK_MAX_BLOCKS = 512;                  /**< Maximum number of hashable blocks. */
		public static final int FWLOCK_MAX_FILES = 100;                   /**< Maximum number of files to protect. */
		public static final int MDTP_FWLOCK_MAX_PARTITIONS = 3;           /** Maximum number of partitions to protect. */
		public static final int MDTP_FWLOCK_MAX_PARTITION_NAME_LEN = 100; /**< Maximum length of partition name. */

		/**
		 * Constructor
		 */
		public MdtpFwlockConfig() {
			mForceVerifyBlock = null;
			mFilesToProtect = null;
			mPartitionName = "";
		}

		public String mPartitionName;       /**< Partition name. */
		public int mHashMode;               /**< Single hash or hash per block. */
		public byte mVerifyRatio;           /**< Statistically verify only a given partition ratio (1-100). */
		public byte[] mForceVerifyBlock;    /** Verify only given block numbers. Relevant only for MDTP_FWLOCK_MODE_BLOCK mode. */
		public String[] mFilesToProtect;    /**< Verify only the blocks of the files in the list.
                                            The list can also contain directories.
                                            In this case all the files under that directory are added to the list recursively.
                                            Relevant only for #MDTP_FWLOCK_MODE_FILES mode. */
	}
        /** @} */ /* end_addtogroup fwlockDataStructure */

	/**@ingroup fwlockfunctions
	 * Starts the MDTP Service application.
	 *
	 * @param[in]  ctx  Context of the calling activity.
	 *
	 * @return None.
	 */
	void start(Context ctx) throws RuntimeException;

	/**@ingroup fwlockfunctions
	 * Stops the MDTP Service application.
	 *
	 * @param[in]  ctx  Context of the calling activity.
	 *
	 * @return None.
	 */
	void stop(Context ctx) throws RuntimeException;

	/**@ingroup fwlockfunctions
	 * Binds the connection to the system service application.
	 *
	 * @param[in]  ctx  Context of the calling activity.
	 *
	 * @return None.
	 */
	void bind(Context ctx) throws RuntimeException;

	/**@ingroup fwlockfunctions
	 * Unbinds the connection from the system service application.
	 *
	 * @param[in]  ctx  Context of the calling activity.
	 *
	 * @return None.
	 */
	void unBind(Context ctx) throws RuntimeException;

	/**@ingroup fwlockfunctions
	 * Checks whether the API has successfully bound to the MDTP service.
	 *
	 * @return TRUE \n
	 *         FALSE
	 */
	boolean isBound();

	/**@ingroup fwlockfunctions
	 * Returns the current state of the MDTP.
	 *
	 * @return MdtpState: \n
	 *         mActivated:     STATE_ACTIVATED or STATE_DEACTIVATED \n
	 *         mSimLocked:     STATE_SIMLOCKED or STATE_SIMUNLOCKED \n
	 *         mEmergencyOnly: STATE_E911_MODE or STATE_NOT_E911_MODE\n
	 *         mTampered:      STATE_TAMPERED  or STATE_UNTAMPERED \n
	 */
	MdtpState getState() throws RuntimeException;

	/**@ingroup fwlockfunctions
	 * Returns the version of the MDTP library.
	 *
	 * @return MdtpVersion
	 *
	 */
	MdtpVersion getVersion() throws RuntimeException;

	/**@ingroup fwlockfunctions
	 * Provides a way to override default configuration for MDTP protection. Default configuration
	 * does not allow local PIN authentication. This function can only be called prior to MDTP activation.
	 *
	 * @param[in]  config  General configuration of the MDTP protection.
	 *
	 * @return None.
	 */
	void setConfig(MdtpConfig config) throws RuntimeException;

	/**@ingroup fwlockfunctions
	 * Processes a signed message sent to device by MDTP services. Signed messages are used for sensitive operations,
	 * such as MDTP activation and deactivation. The structure of the content and the different messages supported
	 * by the MDTP is described in @xhyperref{80NR0991,80-NR099-1}. This function processes the message,
	 * validates its authenticity, and executes the command it carries.
	 *
	 * @param[in]    message Signed message.
	 *
	 * @return None.
	 */
	void processSignedMsg(byte[] message) throws RuntimeException;

	/**@ingroup fwlockfunctions
	 * Deactivates the MDTP protection, using local PIN authentication instead of a signed deactivation message arriving
	 * from MDTP services. Local PIN authentication must be explicitly allowed, and the PIN must be set via configuration
	 * prior to MDTP activation (see MdtpConfig).
	 *
	 * @param[in]  pin  PIN of #MDTP_PIN_LEN digits.
	 *
	 * @return None.
	 */
	void deactivateLocal(String pin) throws RuntimeException;

	/**@ingroup fwlockfunctions
	 * Gets an identity token to identify a specific instance of MDTP activation or deactivation. A new token is generated
	 * only when the MDTP state is Deactivated, and a token has not been previously generated.
	 * Upon MDTP deactivation, the identity token is cleared and a subsequent call to this API generates a new token.
	 *
	 * @return byte[] -- Buffer holding the recovery information.
	 *
	 */
	byte[] getIdToken() throws RuntimeException;

	/**@ingroup fwlockfunctions
	 * Updates the MDTP with a given @xhyperref{RFC5280, RFC 5280} CRL.
	 *
	 * @param[in]  crl  Certificate Revocation List (CRL), according to RFC 5280.
	 * @param[in]  certificate  X509 certificate the CRL is signed with; identifies the signer of the CRL (the root CA or its
	 * subordinates CAs)
	 *
	 */
	void updateCrl(byte[] crl, byte[] certificate) throws RuntimeException;

	/**@ingroup fwlockfunctions
	 * Gets the MDTP recovery information.
	 * The recovery information is used to force MDTP into a deactivated state, to be used in rare cases where a user action
	 * such as a low level device repair, which reflashes all storage, corrupted MDTP persistent storage.
	 *
	 * @note1hang Recovery information must not be kept on device; instead it should be sent to MDTP services.
	 *
	 * @return byte[] -- Buffer holding the recovery information.
	 *
	*/
	byte[] getRecoveryInfo() throws RuntimeException;

	/**@ingroup fwlockfunctions
	 * Modifies the configuration for Firmware Lock protection.
	 * The configuration received by this function is merged with the default configuration.
	 * System partition is the only partition that can be contained in a configuration, and the only mode is MDTP_FWLOCK_MODE_FILES.
	 * Merging the received configuration in this function is primarily merging the file lists of the configurations (default and received).
	 * The higher verify_ratio is taken.
	 *
	 * This function can only be called prior to MDTP activation.
	 *
	 * @note1hang
	 * Default configuration can be set by the OEM when building the MDTP Trusted Component.
	 * Using this function can only add files to the default configuration list, but does not remove files.
	 *
	 * @param[in]  fwlockConfig   Configuration of Firmware Lock protection.
	 *
	 * @return None.
	 *
	*/
	void fwLockConfig(MdtpFwlockConfig[] fwlockConfig) throws RuntimeException;
}
