#ifndef _MM_ION_MEM_HEADER_H__
#define _MM_ION_MEM_HEADER_H__
/*==============================================================================
*       MMIonMemory.h
*
*  DESCRIPTION:
*       Ion Memory interface for WFD
*
*
*  Copyright (c) 2015 Qualcomm Technologies, Inc. All Rights Reserved.
*  Qualcomm Technologies Proprietary and Confidential.
*===============================================================================
*/
/*==============================================================================
                             Edit History
================================================================================
   When            Who           Why
-----------------  ------------  -----------------------------------------------
12/20/2014         UV            InitialDraft
================================================================================
*/

#include "AEEStdDef.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*==============================================================================
FUNCTION:          MMIonMemoryMalloc

DESCRIPTION:
*//**       @brief          Create required buffer in Ion and returning file
                           descriptor.
*//**
@par     DEPENDENCIES:
                 None
*//* PARAMETERS:
*//**       @param          bool    bSecure    Allocate buffer in secure memory
                           uint32  nSize      Size required
                           uint32  nIonHeapType  type of the Heap
                           ex: ION_CP_MM_HEAP_ID
                               ION_QSECOM_HEAP_ID

*//*     RETURN VALUE:
*//**       @return
                           int (filedescriptor)
@par     SIDE EFFECTS:
                 None
*/ /*=========================================================================*/

int MMIonMemoryMalloc
(
    bool   bSecure,
    uint32 nSize,
    uint32 nIonHeapType
    );

/*==============================================================================
FUNCTION:          GetVirtualAddressForIonFd

DESCRIPTION:
*//**       @brief          Getting virtual address for filedescriptor

*//**
@par     DEPENDENCIES:
            None
*//* PARAMETERS:
*//**       @param       int     nFd        file descriptor
                        uint32  nSize      Size of allocated buffer for Fd
                        bool    bSecure    secure or non-secure

*//*     RETURN VALUE:
*//**       @return
                           char* ( VirtualAddress)
@par     SIDE EFFECTS:
            None
*/ /*=========================================================================*/

char* GetVirtualAddressForIonFd
(
    bool   bSecure,
    uint32 nSize,
    int    nFd
);

/*==============================================================================
FUNCTION:          MMIonMemoryFree

DESCRIPTION:
*//**       @brief          free the Ion memory allocated if nFd and pVirtAddress
                           is valid un-map the mapped memory if only pVirtAddress
                            is valid

*//**
@par     DEPENDENCIES:
          None
*//*PARAMETERS:
*//**       @param          bool    bSecure    secure or non-secure
                            uint32  nSize      Size of allocated buffer for Fd
                            int     nFd        file descriptor


*//*     RETURN VALUE:
*//**       @return
                           Success/Error
@par     SIDE EFFECTS:
          None
*/ /*=========================================================================*/

int MMIonMemoryFree
(
    int nFd,
    int nSize,
    char* pVirtAddress
);

#ifdef __cplusplus
}
#endif
#endif



