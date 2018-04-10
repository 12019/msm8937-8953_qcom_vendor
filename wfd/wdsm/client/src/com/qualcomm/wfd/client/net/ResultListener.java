/*
* Copyright (c) 2016 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

package com.qualcomm.wfd.client.net;

import android.util.Log;

public interface ResultListener<ResultType, ErrorType> {
    void onSuccess(ResultType result);
    void onFailure(ErrorType err);

    ResultListener NullListener = new ResultListener() {
        @Override
        public void onSuccess(Object result) {
            Log.v("NullListener", "onSuccess " + result);
        }

        @Override
        public void onFailure(Object err) {
            Log.e("NullListener", "onFailure " + err);
        }
    };
}
