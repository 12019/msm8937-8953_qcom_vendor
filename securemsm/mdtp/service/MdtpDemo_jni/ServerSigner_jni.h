/*
 * @file ServerSigner_jni.h
 * @brief
 * Implementation of JNI functions
 */
/*===========================================================================
  Copyright (c) 2015 Qualcomm Technologies Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary
 ===========================================================================*/

#ifndef __SERVER_SIGNER_JNI_H__
#define __SERVER_SIGNER_JNI_H__

JNIEXPORT jbyteArray JNICALL Java_com_qualcomm_qti_securemsm_mdtp_ServerSigner_CMS_CMSBuilder_nativeCreate(
		JNIEnv *, jobject, jbyteArray, jbyteArray, jbyteArray, jbyteArray, jbyteArray);

JNIEXPORT jbyteArray JNICALL Java_com_qualcomm_qti_securemsm_mdtp_ServerSigner_ASN1_MdtpMsgBuilder_nativeBuildSetStateMsg(
		JNIEnv *, jobject, jboolean, jboolean, jboolean, jbyteArray);

JNIEXPORT jbyteArray JNICALL Java_com_qualcomm_qti_securemsm_mdtp_ServerSigner_ASN1_MdtpMsgBuilder_nativeBuildSetLockMsg(
		JNIEnv *, jobject, jboolean, jboolean, jboolean, jbyteArray);

JNIEXPORT jbyteArray JNICALL Java_com_qualcomm_qti_securemsm_mdtp_ServerSigner_ASN1_MdtpMsgBuilder_nativeBuildSetKillMsg(
		JNIEnv *, jobject, jboolean, jboolean, jbyteArray);
#endif //__SERVER_SIGNER_JNI_H__
