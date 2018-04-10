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
 * limitations under the License.
 */

package com.android.settings.iris;


import android.annotation.Nullable;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.app.admin.DevicePolicyManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.hardware.iris.Iris;
import android.hardware.iris.IrisManager;
import android.hardware.iris.IrisManager.AuthenticationCallback;
import android.hardware.iris.IrisManager.AuthenticationResult;
import android.hardware.iris.IrisManager.RemovalCallback;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.Handler;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceGroup;
import android.preference.PreferenceScreen;
import android.text.Annotation;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.TextPaint;
import android.text.method.LinkMovementMethod;
import android.text.style.URLSpan;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import com.android.internal.logging.MetricsLogger;
import com.android.settings.ChooseLockGeneric;
import com.android.settings.ChooseLockSettingsHelper;
import com.android.settings.HelpUtils;
import com.android.settings.R;
import com.android.settings.SettingsPreferenceFragment;
import com.android.settings.SubSettings;

import java.util.List;

/**
 * Settings screen for Iris
 */
public class IrisSettings extends SubSettings {
    /**
     * Used by the iris settings wizard to indicate the wizard is
     * finished, and each activity in the wizard should finish.
     * <p>
     * Previously, each activity in the wizard would finish itself after
     * starting the next activity. However, this leads to broken 'Back'
     * behavior. So, now an activity does not finish itself until it gets this
     * result.
     */
    static final int RESULT_FINISHED = RESULT_FIRST_USER;
    private static final long LOCKOUT_DURATION = 30000; // time we have to wait for iris to reset, ms

    @Override
    public Intent getIntent() {
        Intent modIntent = new Intent(super.getIntent());
        modIntent.putExtra(EXTRA_SHOW_FRAGMENT, IrisSettingsFragment.class.getName());
        return modIntent;
    }

    @Override
    protected boolean isValidFragment(String fragmentName) {
        if (IrisSettingsFragment.class.getName().equals(fragmentName)) return true;
        return false;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        CharSequence msg = getText(R.string.security_settings_iris_preference_title);
        setTitle(msg);
    }

    public static class IrisSettingsFragment extends SettingsPreferenceFragment
        implements OnPreferenceChangeListener {
        private static final int MAX_RETRY_ATTEMPTS = 20;
        private static final int RESET_HIGHLIGHT_DELAY_MS = 500;

        private static final String TAG = "IrisSettings";
        private static final String KEY_IRIS_ITEM_PREFIX = "key_iris_item";
        private static final String KEY_IRIS_ADD = "key_iris_add";
        private static final String KEY_IRIS_ENABLE_KEYGUARD_TOGGLE =
                "irist_enable_keyguard_toggle";
        private static final String KEY_LAUNCHED_CONFIRM = "launched_confirm";

        private static final int MSG_REFRESH_IRIS_TEMPLATES = 1000;
        private static final int MSG_IRIS_AUTH_SUCCESS = 1001;
        private static final int MSG_IRIS_AUTH_FAIL = 1002;
        private static final int MSG_IRIS_AUTH_ERROR = 1003;
        private static final int MSG_IRIS_AUTH_HELP = 1004;

        private static final int CONFIRM_REQUEST = 101;
        private static final int CHOOSE_LOCK_GENERIC_REQUEST = 102;

        private static final int ADD_IRIS_REQUEST = 10;

        protected static final boolean DEBUG = true;

        private IrisManager mIrisManager;
        private boolean mInIrisLockout;
        private byte[] mToken;
        private boolean mLaunchedConfirm;
        private Drawable mHighlightDrawable;

        private RemovalCallback mRemoveCallback = new RemovalCallback() {

            @Override
            public void onRemovalSucceeded(Iris iris) {
                mHandler.obtainMessage(MSG_REFRESH_IRIS_TEMPLATES,
                        iris.getIrisId(), 0).sendToTarget();
            }

            @Override
            public void onRemovalError(Iris iris, int errMsgId, CharSequence errString) {
                final Activity activity = getActivity();
                if (activity != null) {
                    Toast.makeText(activity, errString, Toast.LENGTH_SHORT);
                }
            }
        };
        private final Handler mHandler = new Handler() {
            @Override
            public void handleMessage(android.os.Message msg) {
                switch (msg.what) {
                    case MSG_REFRESH_IRIS_TEMPLATES:
                        removeIrisPreference(msg.arg1);
                        updateAddPreference();
                    break;
                }
            };
        };

        /**
         * @param errMsgId
         */
        protected void handleError(int errMsgId, CharSequence msg) {
            switch (errMsgId) {
                case IrisManager.IRIS_ERROR_CANCELED:
                    return; // Only happens if we get preempted by another activity. Ignored.
                case IrisManager.IRIS_ERROR_LOCKOUT:
                    return;
                default:
                    // Activity can be null on a screen rotation.
                    final Activity activity = getActivity();
                    if (activity != null) {
                        Toast.makeText(activity, msg , Toast.LENGTH_SHORT);
                    }
                break;
            }
        }


        @Override
        protected int getMetricsCategory() {
            return MetricsLogger.FINGERPRINT;
        }

        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            if (savedInstanceState != null) {
                mToken = savedInstanceState.getByteArray(
                        ChooseLockSettingsHelper.EXTRA_KEY_CHALLENGE_TOKEN);
                mLaunchedConfirm = savedInstanceState.getBoolean(
                        KEY_LAUNCHED_CONFIRM, false);
            }

            Activity activity = getActivity();
            mIrisManager = (IrisManager) activity.getSystemService(
                    Context.IRIS_SERVICE);

            // Need to authenticate a session token if none
            if (mToken == null && mLaunchedConfirm == false) {
                mLaunchedConfirm = true;
                launchChooseOrConfirmLock();
            }
        }

        @Override
        public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
            super.onViewCreated(view, savedInstanceState);
            TextView v = (TextView) LayoutInflater.from(view.getContext()).inflate(
                    R.layout.iris_settings_footer, null);
            v.setText(LearnMoreSpan.linkify(getText(R.string.security_settings_iris_enroll_disclaimer),
                    getString(getHelpResource())));
            v.setMovementMethod(new LinkMovementMethod());
            getListView().addFooterView(v);
            getListView().setFooterDividersEnabled(false);
        }

        protected void removeIrisPreference(int irisId) {
            String name = genKey(irisId);
            Preference prefToRemove = findPreference(name);
            if (prefToRemove != null) {
                if (!getPreferenceScreen().removePreference(prefToRemove)) {
                    Log.w(TAG, "Failed to remove preference with key " + name);
                }
            } else {
                Log.w(TAG, "Can't find preference to remove: " + name);
            }
        }

        /**
         * Important!
         *
         * Don't forget to update the SecuritySearchIndexProvider if you are doing any change in the
         * logic or adding/removing preferences here.
         */
        private PreferenceScreen createPreferenceHierarchy() {
            PreferenceScreen root = getPreferenceScreen();
            if (root != null) {
                root.removeAll();
            }
            addPreferencesFromResource(R.xml.security_settings_iris);
            root = getPreferenceScreen();
            addIrisItemPreferences(root);
            return root;
        }

        private void addIrisItemPreferences(PreferenceGroup root) {
            root.removeAll();
            final List<Iris> items = mIrisManager.getEnrolledIris();
            final int irisCount = items.size();
            for (int i = 0; i < irisCount; i++) {
                final Iris item = items.get(i);
                IrisPreference pref = new IrisPreference(root.getContext());
                pref.setKey(genKey(item.getIrisId()));
                pref.setTitle(item.getName());
                pref.setIris(item);
                pref.setPersistent(false);
                root.addPreference(pref);
                pref.setOnPreferenceChangeListener(this);
            }
            Preference addPreference = new Preference(root.getContext());
            addPreference.setKey(KEY_IRIS_ADD);
            addPreference.setTitle(R.string.iris_add_title);
            addPreference.setIcon(R.drawable.ic_add_24dp);
            root.addPreference(addPreference);
            addPreference.setOnPreferenceChangeListener(this);
            updateAddPreference();
        }

        private void updateAddPreference() {
            /* Disable preference if too many iris added */
            final int max = getContext().getResources().getInteger(
                    com.android.internal.R.integer.config_fingerprintMaxTemplatesPerUser);
            boolean tooMany = mIrisManager.getEnrolledIris().size() >= max;
            CharSequence maxSummary = tooMany ?
                    getContext().getString(R.string.iris_add_max, max) : "";
            Preference addPreference = findPreference(KEY_IRIS_ADD);
            addPreference.setSummary(maxSummary);
            addPreference.setEnabled(!tooMany);
        }

        private static String genKey(int id) {
            return KEY_IRIS_ITEM_PREFIX + "_" + id;
        }

        @Override
        public void onResume() {
            super.onResume();
            // Make sure we reload the preference hierarchy since iris may be added,
            // deleted or renamed.
            updatePreferences();
        }

        private void updatePreferences() {
            createPreferenceHierarchy();
        }

        @Override
        public void onPause() {
            super.onPause();
        }

        @Override
        public void onSaveInstanceState(final Bundle outState) {
            outState.putByteArray(ChooseLockSettingsHelper.EXTRA_KEY_CHALLENGE_TOKEN,
                    mToken);
            outState.putBoolean(KEY_LAUNCHED_CONFIRM, mLaunchedConfirm);
        }

        @Override
        public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, Preference pref) {
            final String key = pref.getKey();
            if (KEY_IRIS_ADD.equals(key)) {
                Intent intent = new Intent();
                intent.setClassName("com.android.settings",
                        IrisEnrollEnrolling.class.getName());
                intent.putExtra(ChooseLockSettingsHelper.EXTRA_KEY_CHALLENGE_TOKEN, mToken);
                startActivityForResult(intent, ADD_IRIS_REQUEST);
            } else if (pref instanceof IrisPreference) {
                IrisPreference irisRef = (IrisPreference) pref;
                final Iris iris = irisRef.getIris();
                showRenameDeleteDialog(iris);
                return super.onPreferenceTreeClick(preferenceScreen, pref);
            }
            return true;
        }

        private void showRenameDeleteDialog(final Iris iris) {
            RenameDeleteDialog renameDeleteDialog = new RenameDeleteDialog();
            Bundle args = new Bundle();
            args.putParcelable("iris", iris);
            renameDeleteDialog.setArguments(args);
            renameDeleteDialog.setTargetFragment(this, 0);
            renameDeleteDialog.show(getFragmentManager(), RenameDeleteDialog.class.getName());
        }

        @Override
        public boolean onPreferenceChange(Preference preference, Object value) {
            boolean result = true;
            final String key = preference.getKey();
            if (KEY_IRIS_ENABLE_KEYGUARD_TOGGLE.equals(key)) {
                Log.v(TAG, "enable keyguard");
            } else {
                Log.v(TAG, "Unknown key:" + key);
            }
            return result;
        }

        @Override
        protected int getHelpResource() {
            return R.string.help_url_iris;
        }

        @Override
        public void onActivityResult(int requestCode, int resultCode, Intent data) {
            super.onActivityResult(requestCode, resultCode, data);
            if (requestCode == CHOOSE_LOCK_GENERIC_REQUEST
                    || requestCode == CONFIRM_REQUEST) {
                if (resultCode == RESULT_FINISHED || resultCode == RESULT_OK) {
                    // The lock pin/pattern/password was set. Start enrolling!
                    if (data != null) {
                        mToken = data.getByteArrayExtra(
                                ChooseLockSettingsHelper.EXTRA_KEY_CHALLENGE_TOKEN);
                    }
                }
            }

            if (mToken == null) {
                // Didn't get an authentication, finishing
                getActivity().finish();
            }
        }

        @Override
        public void onDestroy() {
            super.onDestroy();
        }

        private Drawable getHighlightDrawable() {
            if (mHighlightDrawable == null) {
                final Activity activity = getActivity();
                if (activity != null) {
                    mHighlightDrawable = activity.getDrawable(R.drawable.preference_highlight);
                }
            }
            return mHighlightDrawable;
        }

        private void highlightIrisItem(int irisId) {
            String prefName = genKey(irisId);
            IrisPreference fpref = (IrisPreference) findPreference(prefName);
            final Drawable highlight = getHighlightDrawable();
            if (highlight != null) {
                final View view = fpref.getView();
                final int centerX = view.getWidth() / 2;
                final int centerY = view.getHeight() / 2;
                highlight.setHotspot(centerX, centerY);
                view.setBackground(highlight);
                view.setPressed(true);
                mHandler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        view.setPressed(false);
                        view.setBackground(null);
                    }
                }, RESET_HIGHLIGHT_DELAY_MS);
            }
        }

        private void launchChooseOrConfirmLock() {
            Intent intent = new Intent();
            long challenge = mIrisManager.preEnroll();
            ChooseLockSettingsHelper helper = new ChooseLockSettingsHelper(getActivity(), this);
            if (!helper.launchConfirmationActivity(CONFIRM_REQUEST,
                    getString(R.string.security_settings_iris_preference_title),
                    null, null, challenge)) {
                intent.setClassName("com.android.settings", ChooseLockGeneric.class.getName());
                intent.putExtra(ChooseLockGeneric.ChooseLockGenericFragment.MINIMUM_QUALITY_KEY,
                        DevicePolicyManager.PASSWORD_QUALITY_SOMETHING);
                intent.putExtra(ChooseLockGeneric.ChooseLockGenericFragment.HIDE_DISABLED_PREFS,
                        true);
                intent.putExtra(ChooseLockSettingsHelper.EXTRA_KEY_HAS_CHALLENGE, true);
                intent.putExtra(ChooseLockSettingsHelper.EXTRA_KEY_CHALLENGE, challenge);
                startActivityForResult(intent, CHOOSE_LOCK_GENERIC_REQUEST);
            }
        }

        private void deleteIris(Iris iris) {
            mIrisManager.remove(iris, mRemoveCallback);
        }

        private void renameIris(int irisId, String newName) {
            mIrisManager.rename(irisId, newName);
            updatePreferences();
        }

        public static class RenameDeleteDialog extends DialogFragment {

            private Iris mIris;
            private EditText mDialogTextField;
            private String mIrisName;
            private Boolean mTextHadFocus;
            private int mTextSelectionStart;
            private int mTextSelectionEnd;

            @Override
            public Dialog onCreateDialog(Bundle savedInstanceState) {
                mIris = getArguments().getParcelable("iris");
                if (savedInstanceState != null) {
                    mIrisName = savedInstanceState.getString("irisName");
                    mTextHadFocus = savedInstanceState.getBoolean("textHadFocus");
                    mTextSelectionStart = savedInstanceState.getInt("startSelection");
                    mTextSelectionEnd = savedInstanceState.getInt("endSelection");
                }
                final AlertDialog alertDialog = new AlertDialog.Builder(getActivity())
                        .setView(R.layout.iris_rename_dialog)
                        .setPositiveButton(R.string.security_settings_iris_enroll_dialog_ok,
                                new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick(DialogInterface dialog, int which) {
                                        final String newName =
                                                mDialogTextField.getText().toString();
                                        final CharSequence name = mIris.getName();
                                        if (!newName.equals(name)) {
                                            if (DEBUG) {
                                                Log.v(TAG, "rename " + name + " to " + newName);
                                            }
                                            IrisSettingsFragment parent
                                                    = (IrisSettingsFragment)
                                                    getTargetFragment();
                                            parent.renameIris(mIris.getIrisId(),
                                                    newName);
                                        }
                                        dialog.dismiss();
                                    }
                                })
                        .setNegativeButton(
                                R.string.security_settings_iris_enroll_dialog_delete,
                                new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick(DialogInterface dialog, int which) {
                                        onDeleteClick(dialog);
                                    }
                                })
                        .create();
                alertDialog.setOnShowListener(new DialogInterface.OnShowListener() {
                    @Override
                    public void onShow(DialogInterface dialog) {
                        mDialogTextField = (EditText) alertDialog.findViewById(
                                R.id.iris_rename_field);
                        CharSequence name = mIrisName == null ? mIris.getName() : mIrisName;
                        mDialogTextField.setText(name);
                        if (mTextHadFocus == null) {
                            mDialogTextField.selectAll();
                        } else {
                            mDialogTextField.setSelection(mTextSelectionStart, mTextSelectionEnd);
                        }
                    }
                });
                if (mTextHadFocus == null || mTextHadFocus) {
                    // Request the IME
                    alertDialog.getWindow().setSoftInputMode(
                            WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
                }
                return alertDialog;
            }

            private void onDeleteClick(DialogInterface dialog) {
                if (DEBUG) Log.v(TAG, "Removing irisId=" + mIris.getIrisId());
                IrisSettingsFragment parent
                        = (IrisSettingsFragment) getTargetFragment();
                if (parent.mIrisManager.getEnrolledIris().size() > 1) {
                    parent.deleteIris(mIris);
                } else {
                    ConfirmLastDeleteDialog lastDeleteDialog = new ConfirmLastDeleteDialog();
                    Bundle args = new Bundle();
                    args.putParcelable("iris", mIris);
                    lastDeleteDialog.setArguments(args);
                    lastDeleteDialog.setTargetFragment(getTargetFragment(), 0);
                    lastDeleteDialog.show(getFragmentManager(),
                            ConfirmLastDeleteDialog.class.getName());
                }
                dialog.dismiss();
            }

            @Override
            public void onSaveInstanceState(Bundle outState) {
                super.onSaveInstanceState(outState);
                if (mDialogTextField != null) {
                    outState.putString("irisName", mDialogTextField.getText().toString());
                    outState.putBoolean("textHadFocus", mDialogTextField.hasFocus());
                    outState.putInt("startSelection", mDialogTextField.getSelectionStart());
                    outState.putInt("endSelection", mDialogTextField.getSelectionEnd());
                }
            }
        }

        public static class ConfirmLastDeleteDialog extends DialogFragment {

            private Iris mIris;

            @Override
            public Dialog onCreateDialog(Bundle savedInstanceState) {
                mIris = getArguments().getParcelable("iris");
                final AlertDialog alertDialog = new AlertDialog.Builder(getActivity())
                        .setTitle(R.string.iris_last_delete_title)
                        .setMessage(R.string.iris_last_delete_message)
                        .setPositiveButton(R.string.iris_last_delete_confirm,
                                new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick(DialogInterface dialog, int which) {
                                        IrisSettingsFragment parent
                                                = (IrisSettingsFragment) getTargetFragment();
                                        parent.deleteIris(mIris);
                                        dialog.dismiss();
                                    }
                                })
                        .setNegativeButton(
                                R.string.cancel,
                                new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick(DialogInterface dialog, int which) {
                                        dialog.dismiss();
                                    }
                                })
                        .create();
                return alertDialog;
            }
        }
    }

    public static class IrisPreference extends Preference {
        private Iris mIris;
        private View mView;

        public IrisPreference(Context context, AttributeSet attrs, int defStyleAttr,
                int defStyleRes) {
            super(context, attrs, defStyleAttr, defStyleRes);
        }
        public IrisPreference(Context context, AttributeSet attrs, int defStyleAttr) {
            this(context, attrs, defStyleAttr, 0);
        }

        public IrisPreference(Context context, AttributeSet attrs) {
            this(context, attrs, com.android.internal.R.attr.preferenceStyle);
        }

        public IrisPreference(Context context) {
            this(context, null);
        }

        public View getView() { return mView; }

        public void setIris(Iris item) {
            mIris = item;
        }

        public Iris getIris() {
            return mIris;
        }

        @Override
        protected void onBindView(View view) {
            super.onBindView(view);
            mView = view;
        }
    };

    private static class LearnMoreSpan extends URLSpan {

        private static final Typeface TYPEFACE_MEDIUM =
                Typeface.create("sans-serif-medium", Typeface.NORMAL);

        private LearnMoreSpan(String url) {
            super(url);
        }

        @Override
        public void onClick(View widget) {
            Context ctx = widget.getContext();
            Intent intent = HelpUtils.getHelpIntent(ctx, getURL(), ctx.getClass().getName());
            try {
                ((Activity) ctx).startActivityForResult(intent, 0);
            } catch (ActivityNotFoundException e) {
                Log.w(IrisSettingsFragment.TAG,
                        "Actvity was not found for intent, " + intent.toString());
            }
        }

        @Override
        public void updateDrawState(TextPaint ds) {
            super.updateDrawState(ds);
            ds.setUnderlineText(false);
            ds.setTypeface(TYPEFACE_MEDIUM);
        }

        public static CharSequence linkify(CharSequence rawText, String uri) {
            SpannableString msg = new SpannableString(rawText);
            Annotation[] spans = msg.getSpans(0, msg.length(), Annotation.class);
            SpannableStringBuilder builder = new SpannableStringBuilder(msg);
            for (Annotation annotation : spans) {
                int start = msg.getSpanStart(annotation);
                int end = msg.getSpanEnd(annotation);
                LearnMoreSpan link = new LearnMoreSpan(uri);
                builder.setSpan(link, start, end, msg.getSpanFlags(link));
            }
            return builder;
        }
    }
}
