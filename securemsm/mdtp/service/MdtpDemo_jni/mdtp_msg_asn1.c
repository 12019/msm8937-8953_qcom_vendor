/*
 * @file mdtp_msg_asn1.c
 * @brief
 * MDTP message ASN1 parser implementation.
 */

/*===========================================================================
  Copyright (c) 2015 Qualcomm Technologies Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary
 ===========================================================================*/
#define LOG_TAG "mdtp_msg_asn1"

#include <utils/Log.h>
#include "mdtp_asn1.h"

/*----------------------------------------------------------------------------
 * Global Variables
 * -------------------------------------------------------------------------*/

#define MDTP_ASN1_MAX_VERSION (1)

/* This is the ASN.1 definition of the MDTP message (MDTP API Spec RevB) */

ASN1_SEQUENCE(COMMAND_SET_MDTP_STATE) = {
		ASN1_SIMPLE(COMMAND_SET_MDTP_STATE, mdtpState, ASN1_ENUMERATED),
		ASN1_SIMPLE(COMMAND_SET_MDTP_STATE, subsystemSelection, ASN1_BIT_STRING)
}   ASN1_SEQUENCE_END(COMMAND_SET_MDTP_STATE)

IMPLEMENT_ASN1_FUNCTIONS(COMMAND_SET_MDTP_STATE)

ASN1_SEQUENCE(COMMAND_SET_LOCK_STATE) = {
		ASN1_SIMPLE(COMMAND_SET_LOCK_STATE, lockState, ASN1_ENUMERATED),
		ASN1_SIMPLE(COMMAND_SET_LOCK_STATE, subsystemSelection, ASN1_BIT_STRING)
} ASN1_SEQUENCE_END(COMMAND_SET_LOCK_STATE)

IMPLEMENT_ASN1_FUNCTIONS(COMMAND_SET_LOCK_STATE)

ASN1_SEQUENCE(COMMAND_SET_KILL_STATE) = {
		ASN1_SIMPLE(COMMAND_SET_KILL_STATE, killState, ASN1_ENUMERATED),
		ASN1_SIMPLE(COMMAND_SET_KILL_STATE, killFunctionality, ASN1_BIT_STRING)
} ASN1_SEQUENCE_END(COMMAND_SET_KILL_STATE)

IMPLEMENT_ASN1_FUNCTIONS(COMMAND_SET_KILL_STATE)

ASN1_CHOICE(MDTP_V1_COMMAND) = {
		ASN1_EXP(MDTP_V1_COMMAND, command.commandSetMdtpState, COMMAND_SET_MDTP_STATE, 0),
		ASN1_EXP(MDTP_V1_COMMAND, command.commandSetLockState, COMMAND_SET_LOCK_STATE, 1),
		ASN1_EXP(MDTP_V1_COMMAND, command.commandSetKillState, COMMAND_SET_KILL_STATE, 2)
} ASN1_CHOICE_END(MDTP_V1_COMMAND)

IMPLEMENT_ASN1_FUNCTIONS(MDTP_V1_COMMAND)

ASN1_SEQUENCE(MDTP_V1_MESSAGE) = {
		ASN1_SIMPLE(MDTP_V1_MESSAGE, idToken, ASN1_OCTET_STRING),
		ASN1_SIMPLE(MDTP_V1_MESSAGE, messageType, ASN1_ENUMERATED),
		ASN1_SIMPLE(MDTP_V1_MESSAGE, mdtpV1command, MDTP_V1_COMMAND)
} ASN1_SEQUENCE_END(MDTP_V1_MESSAGE)

IMPLEMENT_ASN1_FUNCTIONS(MDTP_V1_MESSAGE)

ASN1_SEQUENCE(WRAPPER_MDTP_MESSAGE) = {
		ASN1_SIMPLE(WRAPPER_MDTP_MESSAGE, version, ASN1_INTEGER),
		ASN1_SIMPLE(WRAPPER_MDTP_MESSAGE, mdtpMessage, ASN1_OCTET_STRING),
} ASN1_SEQUENCE_END(WRAPPER_MDTP_MESSAGE)

IMPLEMENT_ASN1_FUNCTIONS(WRAPPER_MDTP_MESSAGE)

/*----------------------------------------------------------------------------
 * Local Functions
 * -------------------------------------------------------------------------*/

/**
 * i2d_WRAPPER_MDTP_MESSAGE_bio
 *
 * Write a WRAPPER_MDTP_MESSAGE ASN1 structure into a given BIO
 *
 * @return - 0 - fail, 1 - success
 */
static int i2d_WRAPPER_MDTP_MESSAGE_bio(BIO *bp, WRAPPER_MDTP_MESSAGE *wrapperMdtpMsg)
{
	return ASN1_item_i2d_bio(ASN1_ITEM_rptr(WRAPPER_MDTP_MESSAGE), bp, wrapperMdtpMsg);
}

/**
 * i2d_MDTP_V1_MESSAGE_bio
 *
 * Write a MDTP_V1_MESSAGE ASN1 structure into a given BIO
 *
 * @return - 0 - fail, 1 - success
 */
static int i2d_MDTP_V1_MESSAGE_bio(BIO *bp, MDTP_V1_MESSAGE *mdtpV1Message)
{
	return ASN1_item_i2d_bio(ASN1_ITEM_rptr(MDTP_V1_MESSAGE), bp, mdtpV1Message);
}

/*----------------------------------------------------------------------------
 * External Functions
 * -------------------------------------------------------------------------*/
/**
 * mdtp_build_set_state_msg
 *
 * Create an ASN1 octet string holding the entire set state message payload
 *
 * @return - 0 for an error, positive for success.
 */
int mdtp_build_set_state_msg(uint8_t active, uint8_t firmwareLockSelect, uint8_t simLockSelect, mdtp_buf_t *idToken, mdtp_buf_t *msg) {

	BIO* msg_bio = NULL;
	BIO* msg_v1_bio = NULL;
	mdtp_buf_t msg_v1_buf = {0,0};
	WRAPPER_MDTP_MESSAGE* wrapper_mdt_msg = NULL;
	MDTP_V1_MESSAGE* mdtp_v1_msg = NULL;
	int status = SIGNER_FAIL;

	wrapper_mdt_msg = WRAPPER_MDTP_MESSAGE_new();
	if (!wrapper_mdt_msg) {
		ALOGD("WRAPPER_MDTP_MESSAGE_new failed");
		return SIGNER_FAIL;
	}

	do {
		mdtp_v1_msg = MDTP_V1_MESSAGE_new();
		if (!mdtp_v1_msg) {
			ALOGD("MDTP_V1_MESSAGE_new failed");
			break;
		}

		if (SIGNER_OK != ASN1_OCTET_STRING_set(mdtp_v1_msg->idToken, idToken->data, idToken->len)) {
			ALOGD("ASN1_OCTET_STRING_set() failed");
			break;
		}

		if (SIGNER_OK != ASN1_ENUMERATED_set(mdtp_v1_msg->messageType, MDTP_SET_STATE)) {
			ALOGD("ASN1_ENUMERATED_set() failed");
			break;
		}

		mdtp_v1_msg->mdtpV1command->type = MDTP_SET_STATE;

		mdtp_v1_msg->mdtpV1command->command.commandSetMdtpState = COMMAND_SET_MDTP_STATE_new();
		if (!mdtp_v1_msg->mdtpV1command->command.commandSetMdtpState)
		{
			ALOGD("COMMAND_SET_MDTP_STATE_new failed");
			break;
		}

		if (SIGNER_OK != ASN1_ENUMERATED_set(mdtp_v1_msg->mdtpV1command->command.commandSetMdtpState->mdtpState, active)) {
			ALOGD("ASN1_ENUMERATED_set() failed");
			break;
		}

		if (SIGNER_OK != ASN1_BIT_STRING_set_bit(
				mdtp_v1_msg->mdtpV1command->command.commandSetMdtpState->subsystemSelection, 0, firmwareLockSelect)) {
			ALOGD("ASN1_BIT_STRING_set_bit() failed");
			break;
		}

		if (SIGNER_OK != ASN1_BIT_STRING_set_bit(
				mdtp_v1_msg->mdtpV1command->command.commandSetMdtpState->subsystemSelection, 1, simLockSelect)) {
			ALOGD("ASN1_BIT_STRING_set_bit() failed");
			break;
		}

		mdtp_v1_msg->mdtpV1command->command.commandSetMdtpState->subsystemSelection->flags = ASN1_STRING_FLAG_BITS_LEFT | 6;

		if (SIGNER_OK != ASN1_INTEGER_set(wrapper_mdt_msg->version, MDTP_ASN1_MAX_VERSION)) {
			ALOGD("ASN1_INTEGER_set() failed");
			break;
		}

		msg_v1_bio = BIO_new(BIO_s_mem());
		if (!i2d_MDTP_V1_MESSAGE_bio(msg_v1_bio, mdtp_v1_msg)) {
			ALOGD("i2d_MDTP_V1_MESSAGE_bio() failed");
			break;
		}

		msg_v1_buf.len = BIO_get_mem_data(msg_v1_bio, &msg_v1_buf.data);
		if (!msg_v1_buf.len || !msg_v1_buf.data) {
			ALOGD("BIO_get_mem_data() failed");
			break;
		}

		if (SIGNER_OK != ASN1_OCTET_STRING_set(wrapper_mdt_msg->mdtpMessage, msg_v1_buf.data, msg_v1_buf.len)) {
			ALOGD("ASN1_OCTET_STRING_set() failed");
			break;
		}

		msg_bio = BIO_new(BIO_s_mem());
		if (!i2d_WRAPPER_MDTP_MESSAGE_bio(msg_bio, wrapper_mdt_msg)) {
			ALOGD("i2d_WRAPPER_MDTP_MESSAGE_bio() failed");
			break;
		}

		msg->len = BIO_get_mem_data(msg_bio, &msg->data);
		if (!msg->len || !msg->data) {
			ALOGD("BIO_get_mem_data() failed");
			break;
		}
		status = SIGNER_OK;
	} while(0);

	WRAPPER_MDTP_MESSAGE_free(wrapper_mdt_msg);
	MDTP_V1_MESSAGE_free(mdtp_v1_msg);
	BIO_free_all(msg_v1_bio);
	return status;
}

/**
 * mdtp_build_set_lock_msg
 *
 * Create an ASN1 octet string holding the entire set state message payload
 *
 * @return - 0 for an error, positive for success.
 */
int mdtp_build_set_lock_msg(uint8_t lock, uint8_t firmwareLockSelect, uint8_t simLockSelect, mdtp_buf_t *idToken, mdtp_buf_t *msg) {

	BIO* msg_bio = NULL;
	BIO* msg_v1_bio = NULL;
	mdtp_buf_t msg_v1_buf = {0,0};
	WRAPPER_MDTP_MESSAGE* wrapper_mdt_msg = NULL;
	MDTP_V1_MESSAGE* mdtp_v1_msg = NULL;
	int status = SIGNER_FAIL;

	wrapper_mdt_msg = WRAPPER_MDTP_MESSAGE_new();
	if (!wrapper_mdt_msg) {
		ALOGD("WRAPPER_MDTP_MESSAGE_new failed");
		return SIGNER_FAIL;
	}

	do {
		mdtp_v1_msg = MDTP_V1_MESSAGE_new();
		if (!mdtp_v1_msg) {
			ALOGD("MDTP_V1_MESSAGE_new failed");
			break;
		}

		if (SIGNER_OK != ASN1_OCTET_STRING_set(mdtp_v1_msg->idToken, idToken->data, idToken->len)) {
			ALOGD("ASN1_OCTET_STRING_set() failed");
			break;
		}

		if (SIGNER_OK != ASN1_ENUMERATED_set(mdtp_v1_msg->messageType, MDTP_SET_LOCK)) {
			ALOGD("ASN1_ENUMERATED_set() failed");
			break;
		}

		mdtp_v1_msg->mdtpV1command->type = MDTP_SET_LOCK;

		mdtp_v1_msg->mdtpV1command->command.commandSetLockState = COMMAND_SET_LOCK_STATE_new();
		if (!mdtp_v1_msg->mdtpV1command->command.commandSetLockState)
		{
			ALOGD("COMMAND_SET_MDTP_STATE_new failed");
			break;
		}

		if (SIGNER_OK != ASN1_ENUMERATED_set(mdtp_v1_msg->mdtpV1command->command.commandSetLockState->lockState, lock)) {
			ALOGD("ASN1_ENUMERATED_set() failed");
			break;
		}

		if (SIGNER_OK != ASN1_BIT_STRING_set_bit(
				mdtp_v1_msg->mdtpV1command->command.commandSetLockState->subsystemSelection, 0, firmwareLockSelect)) {
			ALOGD("ASN1_BIT_STRING_set_bit() failed");
			break;
		}

		if (SIGNER_OK != ASN1_BIT_STRING_set_bit(
				mdtp_v1_msg->mdtpV1command->command.commandSetLockState->subsystemSelection, 1, simLockSelect)) {
			ALOGD("ASN1_BIT_STRING_set_bit() failed");
			break;
		}

		mdtp_v1_msg->mdtpV1command->command.commandSetLockState->subsystemSelection->flags = ASN1_STRING_FLAG_BITS_LEFT | 6;

		if (SIGNER_OK != ASN1_INTEGER_set(wrapper_mdt_msg->version, MDTP_ASN1_MAX_VERSION)) {
			ALOGD("ASN1_INTEGER_set() failed");
			break;
		}

		msg_v1_bio = BIO_new(BIO_s_mem());
		if (!i2d_MDTP_V1_MESSAGE_bio(msg_v1_bio, mdtp_v1_msg)) {
			ALOGD("i2d_MDTP_V1_MESSAGE_bio() failed");
			break;
		}

		msg_v1_buf.len = BIO_get_mem_data(msg_v1_bio, &msg_v1_buf.data);
		if (!msg_v1_buf.len || !msg_v1_buf.data) {
			ALOGD("BIO_get_mem_data() failed");
			break;
		}

		if (SIGNER_OK != ASN1_OCTET_STRING_set(wrapper_mdt_msg->mdtpMessage, msg_v1_buf.data, msg_v1_buf.len)) {
			ALOGD("ASN1_OCTET_STRING_set() failed");
			break;
		}

		msg_bio = BIO_new(BIO_s_mem());
		if (!i2d_WRAPPER_MDTP_MESSAGE_bio(msg_bio, wrapper_mdt_msg)) {
			ALOGD("i2d_WRAPPER_MDTP_MESSAGE_bio() failed");
			break;
		}

		msg->len = BIO_get_mem_data(msg_bio, &msg->data);
		if (!msg->len || !msg->data) {
			ALOGD("BIO_get_mem_data() failed");
			break;
		}
		status = SIGNER_OK;
	} while(0);

	WRAPPER_MDTP_MESSAGE_free(wrapper_mdt_msg);
	MDTP_V1_MESSAGE_free(mdtp_v1_msg);
	BIO_free_all(msg_v1_bio);
	return status;
}

/**
 * mdtp_build_set_kill_msg
 *
 * Create an ASN1 octet string holding the entire set state message payload
 *
 * @return - 0 for an error, positive for success.
 */
int mdtp_build_set_kill_msg(uint8_t kill, uint8_t simSelect, mdtp_buf_t *idToken, mdtp_buf_t *msg) {

	BIO* msg_bio = NULL;
	BIO* msg_v1_bio = NULL;
	mdtp_buf_t msg_v1_buf = {0,0};
	WRAPPER_MDTP_MESSAGE* wrapper_mdt_msg = NULL;
	MDTP_V1_MESSAGE* mdtp_v1_msg = NULL;
	int status = SIGNER_FAIL;

	wrapper_mdt_msg = WRAPPER_MDTP_MESSAGE_new();
	if (!wrapper_mdt_msg) {
		ALOGD("WRAPPER_MDTP_MESSAGE_new failed");
		return SIGNER_FAIL;
	}

	do {
		mdtp_v1_msg = MDTP_V1_MESSAGE_new();
		if (!mdtp_v1_msg) {
			ALOGD("MDTP_V1_MESSAGE_new failed");
			break;
		}

		if (SIGNER_OK != ASN1_OCTET_STRING_set(mdtp_v1_msg->idToken, idToken->data, idToken->len)) {
			ALOGD("ASN1_OCTET_STRING_set() failed");
			break;
		}

		if (SIGNER_OK != ASN1_ENUMERATED_set(mdtp_v1_msg->messageType, MDTP_SET_KILL)) {
			ALOGD("ASN1_ENUMERATED_set() failed");
			break;
		}

		mdtp_v1_msg->mdtpV1command->type = MDTP_SET_KILL;

		mdtp_v1_msg->mdtpV1command->command.commandSetKillState = COMMAND_SET_KILL_STATE_new();
		if (!mdtp_v1_msg->mdtpV1command->command.commandSetKillState)
		{
			ALOGD("COMMAND_SET_MDTP_STATE_new failed");
			break;
		}

		if (SIGNER_OK != ASN1_ENUMERATED_set(mdtp_v1_msg->mdtpV1command->command.commandSetKillState->killState, kill)) {
			ALOGD("ASN1_ENUMERATED_set() failed");
			break;
		}

		if (SIGNER_OK != ASN1_BIT_STRING_set_bit(
				mdtp_v1_msg->mdtpV1command->command.commandSetKillState->killFunctionality, 0, simSelect)) {
			ALOGD("ASN1_BIT_STRING_set_bit() failed");
			break;
		}

		mdtp_v1_msg->mdtpV1command->command.commandSetKillState->killFunctionality->flags = ASN1_STRING_FLAG_BITS_LEFT | 7;

		if (SIGNER_OK != ASN1_INTEGER_set(wrapper_mdt_msg->version, MDTP_ASN1_MAX_VERSION)) {
			ALOGD("ASN1_INTEGER_set() failed");
			break;
		}

		msg_v1_bio = BIO_new(BIO_s_mem());
		if (!i2d_MDTP_V1_MESSAGE_bio(msg_v1_bio, mdtp_v1_msg)) {
			ALOGD("i2d_MDTP_V1_MESSAGE_bio() failed");
			break;
		}

		msg_v1_buf.len = BIO_get_mem_data(msg_v1_bio, &msg_v1_buf.data);
		if (!msg_v1_buf.len || !msg_v1_buf.data) {
			ALOGD("BIO_get_mem_data() failed");
			break;
		}

		if (SIGNER_OK != ASN1_OCTET_STRING_set(wrapper_mdt_msg->mdtpMessage, msg_v1_buf.data, msg_v1_buf.len)) {
			ALOGD("ASN1_OCTET_STRING_set() failed");
			break;
		}

		msg_bio = BIO_new(BIO_s_mem());
		if (!i2d_WRAPPER_MDTP_MESSAGE_bio(msg_bio, wrapper_mdt_msg)) {
			ALOGD("i2d_WRAPPER_MDTP_MESSAGE_bio() failed");
			break;
		}

		msg->len = BIO_get_mem_data(msg_bio, &msg->data);
		if (!msg->len || !msg->data) {
			ALOGD("BIO_get_mem_data() failed");
			break;
		}
		status = SIGNER_OK;
	} while(0);

	WRAPPER_MDTP_MESSAGE_free(wrapper_mdt_msg);
	MDTP_V1_MESSAGE_free(mdtp_v1_msg);
	BIO_free_all(msg_v1_bio);
	return status;
}
