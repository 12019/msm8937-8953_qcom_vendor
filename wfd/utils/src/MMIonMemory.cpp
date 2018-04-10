/*==============================================================================
*       WFDMMIonMem.cpp
*
*  DESCRIPTION:
*       Ion Memory Interface
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

/*==============================================================================
**               Includes and Public Data Declarations
**==============================================================================
*/

/* =============================================================================

                     INCLUDE FILES FOR MODULE

================================================================================
*/
#include "MMIonMemory.h"
#include <errno.h>
#include "WFDMMLogs.h"
#include <sys/mman.h>
#include <linux/msm_ion.h>
#include <fcntl.h>
#include <sys/ioctl.h>


#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "MMIONMEMORY"
#endif

#define ALIGN_ION (4096)
#define PAGE_ALIGNMENT_MASK   (ALIGN_ION-1)

/*==============================================================================

 FUNCTION:          MMIonMemoryMalloc

 DESCRIPTION:
*//**       @brief          allocation of Ion Memory

*//**
@par     DEPENDENCIES:
                    None
*//*
 PARAMETERS:
*//**       @param        bSecure
                          nSize
                          nIonHeapType

*//*     RETURN VALUE:
*//**       @return
                    fd (file descriptor)
@par     SIDE EFFECTS:
                    None
*/ /*==========================================================================*/

int MMIonMemoryMalloc
(
    bool   bSecure,
    uint32 nSize,
    uint32 nIonHeapType
)
{
    WFDMMLOGH3("MMIonMemoryMalloc() bSecure = %d,Size = %d,IonHeapType = %d \n",
               bSecure, nSize, nIonHeapType);

    struct  ion_allocation_data   sAllocData;
    struct  ion_fd_data           sFdData;
    int                           nResult = -1;

    memset(&sAllocData, 0, sizeof(ion_allocation_data));
    sAllocData.len = nSize;
    sAllocData.align = 4096;
    sAllocData.heap_id_mask = ION_HEAP(nIonHeapType);

    int nIonDevfd = open("/dev/ion", O_RDONLY);
    if(nIonDevfd < 0)
    {
        WFDMMLOGE("nIonDevfd open Device failed \n");
        return -ENODEV;
    }

    if(bSecure)
    {
        sAllocData.flags |= ION_SECURE;
        sAllocData.align = 1024 * 1024;
    }

    nResult = ioctl(nIonDevfd, ION_IOC_ALLOC, &sAllocData);
    if(nResult < 0)
    {
        WFDMMLOGE1("Failed ION_IOC_ALLOC in MMIonMemoryMalloc %s \n ",
                   strerror(errno));
        close(nIonDevfd);
        return -ENOMEM;
    }

    sFdData.handle = sAllocData.handle;
    nResult = ioctl(nIonDevfd, ION_IOC_SHARE, &sFdData);
    if(nResult < 0)
    {
        WFDMMLOGE("Failed ION_IOC_SHARE MMIonMemoryMalloc \n");
        sFdData.handle = sAllocData.handle;
        ioctl(nIonDevfd, ION_IOC_FREE, &sFdData.handle);
        close(nIonDevfd);
        return -EINVAL;
    }
    close(nIonDevfd);
    WFDMMLOGH1("Success for MMIonMemoryMalloc Fd= %d\n", sFdData.fd);
    return sFdData.fd;
}

/*==============================================================================

FUNCTION:          GetVirtualAddressForIonFd

DESCRIPTION:
*//**       @brief          getting Virtual address with file descriptor

*//**
@par     DEPENDENCIES:
            None
*//*
PARAMETERS:
*//**       @param       bSecure
                         nSize
                         nFd

*//*     RETURN VALUE:
*//**       @return
                       pVirtAddress
@par     SIDE EFFECTS:
            None
*/ /*==========================================================================*/
char* GetVirtualAddressForIonFd
(
    bool   bSecure,
    uint32 nSize,
    int    nFd
)
{
    WFDMMLOGH3("GetVirtualAddressForIonFd inputs Fd=%d, Size=%d,Secure=%d \n",
               nFd,nSize,bSecure);
    int                           nResult    = -1;
    int                           nIonDevfd  = -1;
    struct  ion_fd_data           sFdData;
    char* pVirtAddr = NULL;
    memset(&sFdData, 0, sizeof(ion_fd_data));


    nIonDevfd = open("/dev/ion", O_RDONLY);
    if(nIonDevfd < 0)
    {
        WFDMMLOGE("Failed open Device in GetVirtualAddressForIonFd \n");
        goto FnEXIT;
    }

    sFdData.fd = nFd;
    nResult = ioctl(nIonDevfd, ION_IOC_IMPORT, &sFdData);
    if(nResult < 0)
    {
        WFDMMLOGE1("Failed ION_IOC_IMPORT nFd=%d GetVirtualAddressForIonFd \n",
                  nFd);

        goto FnEXIT1;
    }

    if(bSecure)
    {
        WFDMMLOGH("Secure case cannot get VirtAddress");
        goto FnEXIT1;
    }

    pVirtAddr = (char*)mmap(NULL, nSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                     sFdData.fd, 0);
    if(pVirtAddr == MAP_FAILED)
    {
        WFDMMLOGE2("Failed get VirtAddress  for nFd=%d,%s Error = %s \n",
                   nFd, strerror(errno));
        pVirtAddr = NULL;
    }
    WFDMMLOGH4("Success GetVirtualAddressForIonFd Fd=%d, handle=%p, %s=%p\n",
               nFd, sFdData.handle, "pVirtAddr", pVirtAddr);
FnEXIT1:
    if(nIonDevfd >= 0)
    {
        close(nIonDevfd);
    }
FnEXIT:
    return pVirtAddr;
}

/*==============================================================================

FUNCTION:          MMIonFreeMemory

DESCRIPTION:
*//**       @brief          MMIonFreeMemory  to unmap if pVirtAddress is valid
                        free File descriptor if nFd valid
                        free handle allocated

*//**
@par     DEPENDENCIES:
         None
*//*
PARAMETERS:
*//**       @param        nFd
                          nSize
                          pVirtAddress

*//*     RETURN VALUE:
*//**       @return
                    result
@par     SIDE EFFECTS:
         None
*/ /*==========================================================================*/
int MMIonMemoryFree
(
    int nFd,
    int nSize,
    char *pVirtAddress
)
{

    WFDMMLOGH3("MMIonMemoryFree Fd=%d,nSize=%d,pVirtAddress=%p \n",
               nFd, nSize, pVirtAddress);
    int                           nResult    = -1;
    int                           nIonDevfd  = -1;
    struct  ion_fd_data           sFdData;
    memset(&sFdData, 0, sizeof(ion_fd_data));

    if(pVirtAddress != NULL)
    {
        nResult = munmap(pVirtAddress, nSize);
        if(nResult < 0)
        {
            WFDMMLOGE2("Failed munmap pVirtAdd=%d MMIonFreeMemory,fd=%d \n",
                       pVirtAddress, nFd);
            WFDMMLOGE1("Failed munmap pVirtAdderess err MMIonFreeMemory= %s \n",
                       strerror(errno));
        }
    }

    if(nFd == -1)
    {
        WFDMMLOGE1("nFd is %d to close device in MMIonMemoryFree \n", nFd);
        nResult = 0;
        return nResult;
    }

    nIonDevfd = open("/dev/ion", O_RDONLY);
    if(nIonDevfd < 0)
    {
        WFDMMLOGE("nIonDevfd open Device failed in MMIonFreeMemory \n");
        return nIonDevfd;
    }

    sFdData.fd = nFd;
    nResult = ioctl(nIonDevfd, ION_IOC_IMPORT, &sFdData);
    if(nResult < 0)
    {
        WFDMMLOGE1("Failed ION_IOC_IMPORT for nFd = %d in MMIonFreeMemory \n",
                   nFd);
        close(sFdData.fd);
        goto closefd;
    }
    close(sFdData.fd);
    nResult = ioctl(nIonDevfd, ION_IOC_FREE, &sFdData.handle);
    if(nResult < 0 )
    {
        WFDMMLOGE1("Failed ION_IOC_FREE for nFd = %d in MMIonFreeMemory \n",
                  nFd);
        goto closefd;
    }

    WFDMMLOGH2("Success ION_IOC_FREE in MMIonMemoryFree fd=%d pVirtAdd=%p \n",
               nFd, pVirtAddress);

closefd:
    close(nIonDevfd);
    return nResult;
}

