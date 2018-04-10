/*=============================================================================
Copyright (c) 2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

package com.qualcomm.qti.securemsm.mdtp;

import com.qualcomm.qti.securemsm.mdtp.IMdtpFwlockConfig;

interface IMdtpService {
    int initialize();
    int shutdown();
    int getState(out int[] stateMask);
    int getVersion(out int[] libVersion, out int[] tzVersion);
    int setConfig(in int enable_local_pin_authentication, in byte[] pin);
    int processSignedMsg(in byte[] message);
    int deactivateLocal(in byte[] pin);
    int getIdToken(out byte[] idToken);
    int updateCrl(in byte[] crl, in byte[] certificate);
    int getRecoveryInfoSize(out int[] size);
    int getRecoveryInfo(out byte[] recoveryInfo);
    int fwLockConfig(in IMdtpFwlockConfig[] fwlockconfig);
}