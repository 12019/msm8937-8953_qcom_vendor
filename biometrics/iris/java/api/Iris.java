/*
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
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
public final class Iris implements Parcelable {
    private CharSequence mName;
    private int mGroupId;
    private int mIrisId;
    private int mValidated;

    public Iris(CharSequence name, int groupId, int irisId, int validated) {
        mName = name;
        mGroupId = groupId;
        mIrisId = irisId;
        mValidated = validated;
    }

    private Iris(Parcel in) {
        mName = in.readString();
        mGroupId = in.readInt();
        mIrisId = in.readInt();
        mValidated = in.readInt();
    }

    /**
     * Gets the human-readable name for the given iris.
     * @return name given to iris
     */
    public CharSequence getName() { return mName; }

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

    /**
     * @hide
     */
    public int getValidated() { return mValidated; }

    /**
     * @hide
     */
    public void setValidated(int validated) { mValidated = validated; }

    public int describeContents() {
        return 0;
    }

    public void writeToParcel(Parcel out, int flags) {
        out.writeString(mName.toString());
        out.writeInt(mGroupId);
        out.writeInt(mIrisId);
        out.writeInt(mValidated);
    }

    public static final Parcelable.Creator<Iris> CREATOR
            = new Parcelable.Creator<Iris>() {
        public Iris createFromParcel(Parcel in) {
            return new Iris(in);
        }

        public Iris[] newArray(int size) {
            return new Iris[size];
        }
    };
};
