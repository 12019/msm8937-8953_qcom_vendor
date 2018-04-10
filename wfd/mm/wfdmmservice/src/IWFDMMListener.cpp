/*==============================================================================
*       IWFDMMListener.cpp
*
*  DESCRIPTION:
*       WFD MM Service Listener Interface implementation
*
*
*  Copyright (c) 2015 Qualcomm Technologies, Inc. All Rights Reserved.
*  Qualcomm Technologies Proprietary and Confidential.
*
*   Not a Contribution.
*  Apache license notifications and license are retained
*  for attribution purposes only.
* Copyright (C) 2010 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*===============================================================================
*/
/*==============================================================================
                             Edit History
================================================================================
   When            Who           Why
-----------------  ------------  -----------------------------------------------
10/10/2014         UV            InitialDraft
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
#include "IWFDMMListener.h"

int BpWFDMMListener::notify
(
    int event,
    void* PvtData
)
{
    Parcel sIn;
    Parcel sOut;
    sIn.writeInterfaceToken(IWFDMMListener::getInterfaceDescriptor());
    sIn.writeInt32((int)event);
    sIn.writeInt64((long)PvtData);
    remote()->transact(NOTIFY, sIn, &sOut);
    return 0;
}

IMPLEMENT_META_INTERFACE(WFDMMListener, "wfd.native.mm.Listener");

int BnWFDMMListener::onTransact
(
    uint32_t code,
    const Parcel& data,
    Parcel* reply,
    uint32_t flags
)
{
    CHECK_INTERFACE(IWFDMMListener, data, reply);
    (void)reply;
    (void)flags;
    switch (code)
    {
        case NOTIFY:
        int event = data.readInt32();
        void* PvtData = (void*)data.readInt64();
        int result = notify(event, PvtData);
        break;
    };
    return 0;
}


