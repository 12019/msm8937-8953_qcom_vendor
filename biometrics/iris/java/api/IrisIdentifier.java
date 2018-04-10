/*
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Not a Contribution.
 * Apache license notifications and license are retained
 * for attribution purposes only.
 */

/*
 * Copyright (C) 2015 The Android Open Source Project
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
 */
package android.hardware.iris;

import android.os.Parcel;
import android.os.Parcelable;

/**
 * Container for iris metadata.
 * @hide
 */
public final class IrisIdentifier implements Parcelable {
    private int mGroupId;
    private int mIrisId;

    public IrisIdentifier(int groupId, int irisId) {
        mGroupId = groupId;
        mIrisId = irisId;
    }

    private IrisIdentifier(Parcel in) {
        mGroupId = in.readInt();
        mIrisId = in.readInt();
    }

    /**
     * Gets the device-specific iris id.  Used by Settings to map a name to a specific
     * iris template.
     * @return device-specific id for this iris
     * @hide
     */
    public int getIrisId() { return mIrisId; }

    /**
     * Gets the group id specified when the fingerprint was enrolled.
     * @return group id for the set of fingerprints this one belongs to.
     * @hide
     */
    public int getGroupId() { return mGroupId; }


    public int describeContents() {
        return 0;
    }

    public void writeToParcel(Parcel out, int flags) {
        out.writeInt(mGroupId);
        out.writeInt(mIrisId);
    }

    public static final Parcelable.Creator<IrisIdentifier> CREATOR
            = new Parcelable.Creator<IrisIdentifier>() {
        public IrisIdentifier createFromParcel(Parcel in) {
            return new IrisIdentifier(in);
        }

        public IrisIdentifier[] newArray(int size) {
            return new IrisIdentifier[size];
        }
    };
};