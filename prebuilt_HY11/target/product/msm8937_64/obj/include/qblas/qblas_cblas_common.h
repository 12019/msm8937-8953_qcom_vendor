// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--




#ifndef QBLAS_CBLAS_COMMON_H
#define QBLAS_CBLAS_COMMON_H

enum CBLAS_ORDER {CblasRowMajor = 101, CblasColMajor = 102};
enum CBLAS_TRANSPOSE {CblasNoTrans = 111, CblasTrans = 112, CblasConjTrans = 113};
enum CBLAS_UPLO {CblasUpper = 121, CblasLower = 122};
enum CBLAS_DIAG {CblasNonUnit = 131, CblasUnit = 132};
enum CBLAS_SIDE {CblasLeft = 141, CblasRight = 142};

#endif // QBLAS_CBLAS_COMMON_H
