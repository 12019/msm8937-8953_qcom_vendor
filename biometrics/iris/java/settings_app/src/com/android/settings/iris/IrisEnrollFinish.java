/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
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
 * limitations under the License
 */

package com.android.settings.iris;

import android.content.Context;
import android.content.Intent;
import android.hardware.iris.Iris;
import android.hardware.iris.IrisManager;
import android.os.Bundle;
import android.preference.Preference;
import android.view.View;
import android.widget.Button;

import com.android.settings.R;
import com.android.settings.iris.IrisSettings.IrisPreference;

import java.util.List;

/**
 * Activity which concludes fingerprint enrollment.
 */
public class IrisEnrollFinish extends IrisEnrollBase {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.iris_enroll_finish);
        setHeaderText(R.string.security_settings_iris_enroll_finish_title);
        Button addButton = (Button) findViewById(R.id.add_another_button);

        IrisManager irism = (IrisManager) getSystemService(Context.IRIS_SERVICE);
        int enrolled = irism.getEnrolledIris().size();
        int max = getResources().getInteger(
                com.android.internal.R.integer.config_fingerprintMaxTemplatesPerUser);
        if (enrolled >= max) {
            /* Don't show "Add" button if too many fingerprints already added */
            addButton.setVisibility(View.INVISIBLE);
        } else {
            addButton.setOnClickListener(this);
        }
    }

    @Override
    protected void onNextButtonClick() {
        setResult(RESULT_FINISHED);
        finish();
    }

    @Override
    public void onClick(View v) {
        if (v.getId() == R.id.add_another_button) {
            final Intent intent = getEnrollingIntent();
            intent.addFlags(Intent.FLAG_ACTIVITY_FORWARD_RESULT);
            startActivity(intent);
            finish();
        }
        super.onClick(v);
    }
}
