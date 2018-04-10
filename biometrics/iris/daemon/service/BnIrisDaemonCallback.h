/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef BNIRIS_DAEMON_CALLBACK_H_
#define BNIRIS_DAEMON_CALLBACK_H_

#include "IIrisDaemonCallback.h"

namespace android {

class BnIrisDaemonCallback : public BnInterface<IIrisDaemonCallback> {
public:
	virtual status_t onTransact(uint32_t code, const Parcel& data, Parcel* reply,
			uint32_t flags = 0);
};

}; // namespace android

#endif // BNIRIS_DAEMON_CALLBACK_H_

