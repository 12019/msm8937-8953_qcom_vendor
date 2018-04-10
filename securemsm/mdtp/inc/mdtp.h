#ifndef MDTP_H
#define MDTP_H

/** @file mdtp.h
 * @brief
 * This file contains the definitions of the constants, data structures
 * and interfaces for the MDTP module.
 */
/*===========================================================================

Copyright (c) 2014-2016 Qualcomm Technologies, Inc. All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential.

 ===========================================================================*/

/*===========================================================================

 EDIT HISTORY FOR FILE

 when         who     what, where, why
 ----------   ---     -------------------------------------------------------
 02/21/2016   c_amirk Updated PIN status responds.
 02/14/2016   ablay   Extend the get_state API.
 07/22/2015   mdgreen Add support for PIN validation lockout.
 02/11/2015   ablay   Add support for ISV CA CRLs.
 12/02/2014   ablay   Added block mapping and FOTA support.
 11/04/2014   ablay   API rev B.
 06/10/2014   ablay   API rev A.

 ===========================================================================*/

/*----------------------------------------------------------------------------
 * Macro Declarations
 * -------------------------------------------------------------------------*/

#define MDTP_ID_TOKEN_LEN                  (16)
#define MDTP_PIN_LEN                       (8)
#define MDTP_MAX_PIN_RETRIES               (3)
#define MDTP_MAX_MSG_LEN                   (4096)
#define MDTP_MAX_CRL_LEN                   (2048)
#define MDTP_MAX_CERT_LEN                  (2048)
#define MDTP_MAX_CRLS                      (20)
#define MDTP_FWLOCK_BLOCK_SIZE             (1024*1024*16)
#define MDTP_FWLOCK_MAX_BLOCKS             (512)
#define MDTP_FWLOCK_MAX_FILES              (100)
#define MDTP_FWLOCK_MAX_FILE_NAME_LEN      (100)
#define MDTP_FWLOCK_MAX_PARTITIONS         (3)
#define MDTP_FWLOCK_MAX_PARTITION_NAME_LEN (100)
#define MDTP_STATE_ACTIVATED_MSK           (0x00000001)
#define MDTP_STATE_SIMLOCKED_MSK           (0x00000002)
#define MDTP_STATE_E911_MODE_MSK           (0x00000004)
#define MDTP_STATE_TAMPERED_MSK            (0x00000008)

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

#pragma pack(push, mdtp, 1)

/** MDTP error types. */
typedef enum {
    MDTP_SUCCESS = 0,                 /**< Success error code. */
    MDTP_FAILURE,                     /**< General failure error code. */
    MDTP_MSG_VERIFICATION_FAILURE,    /**< Signature verification failure */
    MDTP_ERR_SIZE = 0x7FFFFFFF
} mdtp_status_t;

/** MDTP state. */
typedef enum {
    MDTP_STATE_ACTIVATED,        /**< MDTP is not activated on the device. */
    MDTP_STATE_DEACTIVATED,      /**< MDTP is activated on the device. */
    MDTP_STATE_SIZE = 0x7FFFFFFF
} mdtp_state_t;

/** MDTP version. */
typedef struct mdtp_version {
    uint32 lib_version;   /**< MDTP version number for MDTP lib. */
    uint32 tz_version;   /**< MDTP version number for the TZ component. */
} mdtp_version_t;

/** MDTP PIN. */
typedef struct mdtp_pin {
    char mdtp_pin[MDTP_PIN_LEN+1];  /**< A null terminated PIN. */
} mdtp_pin_t;

/** MDTP configuration. */
typedef struct mdtp_cfg {
    uint8 enable_local_pin_authentication;  /**< Allow local authentication using a PIN. */
    mdtp_pin_t mdtp_pin;                    /**< Null terminated PIN provided by the user for local deactivation.
                                                 PIN length should be MDTP_PIN_LEN digits. */
} mdtp_cfg_t;

/** MDTP sized buffer type. */
typedef struct mdtp_buffer {
    size_t len;             /**< Buffer length in bytes. */
    unsigned char* data;    /**< Buffer data. */
} mdtp_buffer_t;

/** MDTP identification token. */
typedef struct mdtp_id_token {
    unsigned char data[MDTP_ID_TOKEN_LEN];   /**< UUID representing MDTP activation. */
} mdtp_id_token_t;

/** MDTP Firmware Lock Hash mode. */
typedef enum mdtp_fwlock_mode {
    MDTP_FWLOCK_MODE_SINGLE = 0,             /**< A single hash on the entire partition. */
    MDTP_FWLOCK_MODE_BLOCK,                  /**< A hash per MDTP_FWLOCK_BLOCK_SIZE sized block. */
    MDTP_FWLOCK_MODE_FILES,                  /**< A hash for blocks that contain the files that listed in the configuration. */
    MDTP_FWLOCK_MODE_SIZE = 0x7FFFFFFF
} mdtp_fwlock_mode_t;

/** MDTP Firmware Lock partition configuration. */
typedef struct mdtp_fwlock_partition_cfg {
    char name[MDTP_FWLOCK_MAX_PARTITION_NAME_LEN];      /**< Partition name. */
    mdtp_fwlock_mode_t hash_mode;                       /**< Single hash or hash per block. */
    uint8 verify_ratio;                                 /**< Statistically verify only a given partition ratio (1-100). */
    uint8 force_verify_block[MDTP_FWLOCK_MAX_BLOCKS];   /**< Verify only given block numbers. Relevant only for @c MDTP_FWLOCK_MODE_BLOCK mode. */
    char files_to_protect[MDTP_FWLOCK_MAX_FILES][MDTP_FWLOCK_MAX_FILE_NAME_LEN];
                                                        /**< Verify only the blocks of the files in the list.
                                                        The list can contain also directories.
                                                        In this case all the files under that directory, will be added to the list recursively.
                                                        Relevant only for @c MDTP_FWLOCK_MODE_FILES mode.
                                                        The file/directory names are null-terminated strings.
                                                        The first empty string (i.e. starts with 0) in the array, ends the list. */
} mdtp_fwlock_partition_cfg_t;

/** MDTP Firmware Lock configuration. */
typedef struct mdtp_fwlock_cfg {
    uint8 num_partitions;                       /**< Number of partitions to protect. */
    mdtp_fwlock_partition_cfg_t *partitions;    /**< Lock configuration per partition. */
} mdtp_fwlock_cfg_t;

#pragma pack(pop, mdtp)

/*----------------------------------------------------------------------------
 * MDTP API functions
 * -------------------------------------------------------------------------*/

/*****************************************************************************************************************/
/* MDTP management API */
/*****************************************************************************************************************/

/**
  Initializes the MDTP library. This will initialize system resources needed by the MDTP library.
  This must be done before calling any other MDTP API function.

  @return
  @li @c MDTP_SUCCESS -- Success. \n
  @li @c MDTP_FAILURE -- Failure.

  @dependencies
  None
*/
mdtp_status_t mdtp_initialize();

/**
  Shutdowns the MDTP library. This will free system resources used by the MDTP library.
  After calling this function, further calls to MDTP API will requires a call to mdtp_initialize() first.

  @return
  @li @c MDTP_SUCCESS -- Success. \n
  @li @c MDTP_FAILURE -- Failure.

  @dependencies
  None
*/
mdtp_status_t mdtp_shutdown();

/**
  Returns the current states of the MDTP.
  @li [0] Activated
  @li [1] SIM locked
  @li [2] Emergency only mode
  @li [3] Tampered

  @param[out]  state_mask  Bitmap of current MDTP states. \n

  @return
  @li @c MDTP_SUCCESS -- Success. \n
  @li @c MDTP_FAILURE -- Failure.

  @dependencies
  None
*/
mdtp_status_t mdtp_get_state(uint32* state_mask);

/**
  Returns the version of the MDTP library.

  @param[out]  mdtp_version  Current version of the MDP library.

  @return
  @li @c MDTP_SUCCESS -- Success. \n
  @li @c MDTP_FAILURE -- Failure.

  @dependencies
  None
*/
mdtp_status_t mdtp_get_version(mdtp_version_t* mdtp_version);

/**
  Provides a way to override default configuration for MDTP protection. Default configuration
  does not allow local pin authentication. This function can only be called prior to MDTP activation.

  @param[in]  mdtp_config   general configuration of the MDTP protection.

  @return
  @li @c MDTP_SUCCESS -- Success. \n
  @li @c MDTP_FAILURE -- Failure.

  @dependencies
  None.
*/
mdtp_status_t mdtp_config(mdtp_cfg_t* mdtp_cfg);

/**
  Process a signed message sent to device by MDTP services. Signed messages are used for sensitive operations,
  such as MDTP activation, deactivation, etc. The structure of the content and the different messages supported
  by the MDTP is described in "MDTP Interface Specification" document. The function will process the message,
  validate its authenticity, and execute the command it carries.

  @param[in]    signed_msg signed message.

  @return
  @li @c MDTP_SUCCESS -- Success. \n
  @li @c MDTP_FAILURE -- Failure.

  @dependencies
  None
*/
mdtp_status_t mdtp_process_signed_msg(mdtp_buffer_t* signed_msg);

/**
  Deactivates the MDTP protection, using local pin authentication, instead of a signed deactivation message arriving
  from MDTP services.  Local pin authentication  must be explicitly allowed, and pin must be set via configuration
  prior to MDTP activation. @see mdtp_config.

  @param[in]  mdtp_pin  PIN of MDTP_PIN_LEN digits.

  @return
  @li @c MDTP_SUCCESS -- Success. \n
  @li @c MDTP_FAILURE -- Failure.

  @dependencies
  None
*/
mdtp_status_t mdtp_deactivate_local(mdtp_pin_t* mdtp_pin);

/**
  Get an identity token to uniquely identify a specific instance of MDTP activation/deactivation. Only when MDTP state
  is deactivated, and a token has not been previously generated, a new unique token shall be generated and persisted.
  Upon MDTP deactivation the identity token is cleared and a subsequent call to this API shall generate a new token.

  @param[out]  id_token     16-byte token.

  @return
  @li @c MDTP_SUCCESS -- Success. \n
  @li @c MDTP_FAILURE -- Failure.

  @dependencies
  None.
*/
mdtp_status_t mdtp_get_id_token(mdtp_id_token_t* id_token);

/**
  Updates the MDTP with a given RFC5280 CRL

  @param[in]  crl  Certificate Revocation List (CRL), according to RFC5280.
  @param[in]  cert  The X509 certificate the CRL is signed with. This is to identify who signed the CRL (the root CA or its
                    subordinates CAs)

  @return
  @li @c MDTP_SUCCESS -- Success. \n
  @li @c MDTP_FAILURE -- Failure.

  @dependencies
  None
*/
mdtp_status_t mdtp_update_crl(mdtp_buffer_t* crl, mdtp_buffer_t* cert);

/**
  Get the MDTP recovery information.

  The recovery information is used to force MDTP into deactivated state, to be used in rare cases where MDTP persistent storage
  was corrupted by user action such as a low level device repair which re-flashed all storage.
  Notes:
  @li Recovery information <b>must not be kept on device</b>. Instead it should be sent to MDTP services.
  @li This call allocates a buffer, and returns its pointer and size. The caller is responsible for freeing the buffer when done.

  @param[out]  mdtp_recovery_info  Buffer holding the recovery information.

  @return
  @li @c MDTP_SUCCESS -- Success. \n
  @li @c MDTP_FAILURE -- Failure.

  @dependencies
  None
*/
mdtp_status_t mdtp_get_recovery_info(mdtp_buffer_t* mdtp_recovery_info);

/*****************************************************************************************************************/
/* MDTP lock functionality API */
/*****************************************************************************************************************/

/**
  Provides a way to modify the configuration for Firmware Lock protection.
  The configuration received by this function, will be merged with the default configuration.
  The only partition that can be contained in a configuration is 'system', and the only mode is @c MDTP_FWLOCK_MODE_FILES.
  So, merging the received configuration in this function is mainly merging the file lists of the configurations (default and received).
  Also, the higher verify_ratio will be taken.

  This function can only be called prior to MDTP activation.

  Notes:
  @li Default configuration can be set by the OEM when building the MDTP Trusted Component.
  @li Using this function can only add files to the default configuration list, but not remove.

  @param[in]  fwlock_cfg   Configuration of Firmware Lock protection.

  @return
  @li @c MDTP_SUCCESS -- Success. \n
  @li @c MDTP_FAILURE -- Failure.

  @dependencies
  None.
*/
mdtp_status_t mdtp_fwlock_config(mdtp_fwlock_cfg_t* fwlock_cfg);

/*****************************************************************************************************************/

#endif /* MDTP_H */
