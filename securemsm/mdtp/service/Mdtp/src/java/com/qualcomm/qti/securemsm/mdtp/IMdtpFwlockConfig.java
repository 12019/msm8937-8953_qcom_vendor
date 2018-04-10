/*=============================================================================
Copyright (c) 2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

package com.qualcomm.qti.securemsm.mdtp;

import android.os.Parcel;
import android.os.Parcelable;

public final class IMdtpFwlockConfig implements Parcelable {
	public String mPartitionName = "";
	public int mHashMode = 0;
	public byte mVerifyRatio = 0;
	public byte[] mForceVerifyBlock = new byte[0];
	public String[] mFilesToProtect = new String[0];

	public static final Creator<IMdtpFwlockConfig> CREATOR =
			new Creator<IMdtpFwlockConfig>() {
		public IMdtpFwlockConfig createFromParcel(Parcel in) {
			return new IMdtpFwlockConfig(in);
		}

		public IMdtpFwlockConfig[] newArray(int size) {
			return new IMdtpFwlockConfig[size];
		}
	};

	public IMdtpFwlockConfig() {
	}

	private IMdtpFwlockConfig(Parcel in) {
		readFromParcel(in);
	}

	@Override
	public void writeToParcel(Parcel out, int flags) {
		out.writeByteArray(mPartitionName.getBytes());
		out.writeInt(mHashMode);
		out.writeByte(mVerifyRatio);
		out.writeByteArray(mForceVerifyBlock);
		out.writeInt(mFilesToProtect.length);
		for (int i = 0; i < mFilesToProtect.length; i++) {
			out.writeByteArray(mFilesToProtect[i].getBytes());
		}
	}

	public void readFromParcel(Parcel in) {
		byte[] str = in.createByteArray();
		mPartitionName = new String(str);
		mHashMode = in.readInt();
		mVerifyRatio = in.readByte();
		mForceVerifyBlock = in.createByteArray();
		int numFTP = in.readInt();
		mFilesToProtect = new String[numFTP];
		for (int i = 0; i < mFilesToProtect.length; i++) {
			byte[] file = in.createByteArray();
			mFilesToProtect[i] = new String(file);
		}
	}

	@Override
	public int describeContents() {
		return 0;
	}
}
