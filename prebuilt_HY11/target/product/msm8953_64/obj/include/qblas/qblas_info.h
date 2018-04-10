// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--


#ifndef QBLAS_INFO_H
#define QBLAS_INFO_H

#include <qblas_types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum QBLAS_INFO_TYPE { QBLAS_BLAS_CPU=1, QBLAS_BLAS_CPU_AND_DSP=2 };

struct qblas_info
{
    const char *productName;
    const char *vendor;
    qblas_int major;
    qblas_int minor;
    qblas_int mark;
    QBLAS_INFO_TYPE type;
    const char *cpuOptimization;
    const char *cpuArch;
    const char *buildDate;
};


void QBLASVersionString(const char **string);

void QBLASVersionInfo(qblas_info *info);

#ifdef __cplusplus
}
#endif

#endif // QBLAS_INFO_H
