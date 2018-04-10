/*=============================================================================
Copyright (c) 2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

package com.qualcomm.qti.securemsm.mdtp.MdtpDemo;

import java.lang.reflect.Method;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Fragment;
import android.app.FragmentManager;
import android.app.FragmentTransaction;
import android.app.ProgressDialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.provider.Settings;
import android.text.Editable;
import android.text.InputType;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;
import android.widget.Toast;

/**
 * This class provides the UI for the Setup Wizard application
 */

abstract interface MdtpActionListener {
    public void onActivate();
    public void onLocalDeactivate();
    public void onRemoteDeactivate();
    public void onAuthorizationRequest();
}

/**
 * @author danyr
 *
 */
public class SetupWizardActivity extends Activity implements MdtpActionListener {

    private static final String MDTP_SETUPWIZARD_ENABLE_SYSPROP = "mdtp.setupwizard.enable";
    private static String TAG = "MdtpSetupWizardActivity";
    private boolean isActivationInProgress = false;
    private TextView mTextViewMdtpState;
    private Button mButtonContinue;

    /**
     * Wizard activity fragment abstract class. Implements MDTP state/actions for the UI.
     * Requires the parent activity to implement MdtpActionListener interface. This interface allows
     * the fragments to request MDTP actions (Activate/deactivate/...) from the main activity.
     */
    private abstract class MdtpWizardFragment extends Fragment {
        private boolean mInitialized = false;
        protected boolean mAllowSetupContinue = false;
        protected boolean mMdtpActivated = false;
        protected boolean mMdtpAuthorized = false;

        protected MdtpActionListener mActionListener;

        /**
         * Notifies the fragment that MDTP is activated
         * Should be called by parent activity/view to change the state of the fragment
         */
        public void setMdtpStateActivated(boolean state) {
            mMdtpActivated = state;
            updateState();
        }

        /**
         * Notifies the fragment that MDTP was authorized by MDTP services
         * Should be called by parent activity/view to change the state of the fragment
         */
        public void setMdtpStateAuthorized(boolean state) {
            mMdtpAuthorized = state;
            updateState();
        }

        /**
         * When overriding make sure to check the inherited method result before updating the UI state
         */
        public boolean updateState() {
            return mInitialized;
        }

        /**
         * override with layout id that the fragment will be displaying
         */
        protected abstract int getLayoutId();

        @Override
        public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
            View view = inflater.inflate(getLayoutId(), container,false);
            mInitialized = true;
            return view;
        }
        @Override
        public void onAttach(Activity activity) {
            super.onAttach(activity);
            try {
                mActionListener = (MdtpActionListener) activity;
            } catch (ClassCastException e) {
                throw new ClassCastException(activity.toString() + " must implement MdtpActionListener");
            }
        }


        /**
         * Parent view should call this to check if the fragment state allows the wizard to continue
         */
        public boolean allowedToContinue() {
            return mAllowSetupContinue;
        }

    }


    /**
     * ActivityFragment that implements user factory reset protection screen
     */
    private class ResetProtectionFragment extends MdtpWizardFragment {
        private Button mButtonRequestUnblock;
        private Button mButtonRequestRemoteDeactivate;
        private Button mButtonRequestLocalDeactivate;

        @Override
        public View onCreateView(LayoutInflater inflater, ViewGroup container,
                Bundle savedInstanceState) {
            View view = super.onCreateView(inflater, container, savedInstanceState);
            mButtonRequestUnblock = (Button) view.findViewById(R.id.wiz_unblock);
            mButtonRequestRemoteDeactivate = (Button) view.findViewById(R.id.wiz_remote);
            mButtonRequestLocalDeactivate = (Button) view.findViewById(R.id.wiz_local);

            mButtonRequestUnblock.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    mActionListener.onAuthorizationRequest();
                }
            });

            mButtonRequestRemoteDeactivate.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    mActionListener.onRemoteDeactivate();
                }
            });

            mButtonRequestLocalDeactivate.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    mActionListener.onLocalDeactivate();
                }
            });
            return view;
        }

        @Override
        public boolean updateState() {
            if (!super.updateState())
                return false;

            //unblock continue setup if mdtp had authorized it or was deactivated
            if (mMdtpAuthorized || !mMdtpActivated) {
                mAllowSetupContinue = true;
            }

            mButtonRequestRemoteDeactivate.setEnabled(mMdtpActivated);
            mButtonRequestLocalDeactivate.setEnabled(mMdtpActivated);
            mButtonRequestUnblock.setEnabled(!mMdtpAuthorized && mMdtpActivated);

            return true;
        }

        @Override
        protected int getLayoutId() {
            return R.layout.setup_wizard_reset_prot;
        }
    }

    /**
     * ActivityFragment that implements user sign up screen
     */
    private class WizardUserSignUpFragment extends MdtpWizardFragment {
        private Button mButtonServiceActivate;
        private Button mButtonConfiguration;
        private TextView mTextViewSignupMessageLabel;

        //sign up should always allows user to continue with the setup flow
        @Override
        public boolean allowedToContinue() {
            return true;
        }
        @Override
        public View onCreateView(LayoutInflater inflater, ViewGroup container,
                Bundle savedInstanceState) {
            View view = super.onCreateView(inflater, container, savedInstanceState);
            mButtonServiceActivate = (Button) view.findViewById(R.id.button_service_activate);
            mButtonConfiguration = (Button) view.findViewById(R.id.button_configuration);
            mTextViewSignupMessageLabel = (TextView) view.findViewById(R.id.textView_signup_message_label);

            mButtonServiceActivate.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    mActionListener.onActivate();
                }
            });

            mButtonConfiguration.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    Intent intent = new Intent(getActivity(), ConfigurationActivity.class);
                    startActivity(intent);
                }
            });
            return view;
        }

        @Override
        public boolean updateState() {
            if (!super.updateState())
                return false;
            Log.d(TAG, "opt-in updateState reached");
            mButtonServiceActivate.setEnabled(!mMdtpActivated);
            mButtonConfiguration.setEnabled(!mMdtpActivated);
            mTextViewSignupMessageLabel
                    .setText(mMdtpActivated ? R.string.label_signup_message_activated
                            : R.string.label_signup_message);

            return true;
        }

        @Override
        protected int getLayoutId() {
            return R.layout.setup_wizard_sign_up;
        }
    }

    /**
     * ActivityFragment that implements device boot block screen UI and logic
     */
    private class WizardBlockDeviceBootFragment extends MdtpWizardFragment {
        @Override
        protected int getLayoutId() {
            return R.layout.setup_wizard_block_boot;
        }

        //this screen should block the device boot
        @Override
        public boolean allowedToContinue() {
            return false;
        }
    }

    private boolean mIsInitialized = false;
    private MdtpWizardFragment mActiveFragment;
    public boolean mFailedToBindService = false;
    protected String mLocalPin="";



    /**
     * Offload MDTP services to a non-UI thread
     */
    private class SendActivateMsg extends AsyncTask<Integer, Void, Integer> {

        public static final int CHECK_BIND_CMD = 1;
        public static final int DEACTIVATE_CMD = 2;
        public static final int DEACTIVATE_LCL_CMD = 3;
        public static final int ACTIVATE_CMD = 4;

        private Context mContext;
        private ProgressDialog mProgressDialog;

        public SendActivateMsg(Context context) {
            mContext = context;
        }

        protected Integer doInBackground(Integer... ints) {
            try {
                if (ints[0] == CHECK_BIND_CMD) {
                    Server.getInstance().initialize(mContext);
                    Log.d(TAG, "CHECK_BIND_CMD");
                    /** Check if application has successfully bound to MDTP service */
                    int i;
                    // wait for 30*100 (3 seconds) - service initialization can take time on boot
                    final int maxWaitIter = 30;
                    for (i = 0; (Server.getInstance().isBound() == false) && (i < maxWaitIter); i++) {
                        Log.d(TAG, "Waiting for server bind...");
                        Thread.sleep(100);
                    }
                    /** not bound to service */
                    if (i == maxWaitIter) {
                        Log.d(TAG, "Timeout while binding to the service");
                        return -CHECK_BIND_CMD;
                    }
                    // bind success - update state of the buttons
                    Log.d(TAG, "Succeffully binded to the service");
                } else if (ints[0] == DEACTIVATE_CMD) {
                    Log.d(TAG, "DEACTIVATE_CMD");
                    Server.getInstance().deactivate(mContext, Device.getInstance().getEnableSimLock());
                } else if (ints[0] == ACTIVATE_CMD) {
                    Log.d(TAG, "sending activation request");
                    Server.getInstance().activate(mContext, Device.getInstance().getEnableSimLock());
                    isActivationInProgress = true;
                } else if (ints[0] == DEACTIVATE_LCL_CMD) {
                    Log.d(TAG, "DEACTIVATE_LCL_CMD");
                    //perform local unlock only if the PIN is not empty
                    if (!mLocalPin.isEmpty()) {
                        Device.getInstance().setPin(mLocalPin);
                        Device.getInstance().deactivateLocal(mContext);
                    } else {
                        Log.d(TAG, "PIN not specified. skipping local deactivation");
                        return -DEACTIVATE_LCL_CMD;
                    }
                }
            } catch (Exception e) {
                Log.d(TAG, "Exception while accesing the service " + e.toString());
                isActivationInProgress = false;
                return -1*ints[0];
            }
            return 0;
        }

        protected void onPreExecute() {
            //Show progress bar for every async (long) operation
            mProgressDialog = new ProgressDialog(mContext);
            mProgressDialog.setCancelable(false);
            mProgressDialog.setTitle("In progress...");
            mProgressDialog.setProgressStyle(android.R.style.Widget_ProgressBar_Small);
            mProgressDialog.show();
        }

        protected void onPostExecute(Integer sts) {
            Log.d(TAG, "onPostExecute - sts: " + sts);
            if (sts == -CHECK_BIND_CMD) {
                Toast.makeText(mContext, "Failed to bind to MDTP service", Toast.LENGTH_LONG)
                .show();
                mFailedToBindService = true; //tells the wizard to block the boot
            } else if (sts == -DEACTIVATE_CMD) {
                Toast.makeText(mContext, "Remote deactivate failed", Toast.LENGTH_LONG).show();
            } else if (sts == -DEACTIVATE_LCL_CMD) {
                Toast.makeText(mContext, "Local deactivate failed", Toast.LENGTH_LONG).show();
            } else if (sts == -ACTIVATE_CMD) {
                Toast.makeText(mContext, "Activation failed", Toast.LENGTH_LONG).show();
            }
            isActivationInProgress = false;
            //Update UI state
            updateState();
            mProgressDialog.dismiss();
        }
    }

    /**
     * Update view according to the current MDTP state
     */
    private void updateState() {
        boolean isActivated = Server.getInstance().isActivated();

        if (!mIsInitialized) {
            //First time initialization
            mIsInitialized = true;
            //We create the UI fragment according to the MDTP state
            setContentView(R.layout.activity_setup_wizard);
            mTextViewMdtpState = (TextView) findViewById(R.id.textView_mdtp_state);
            mButtonContinue = (Button) findViewById(R.id.wiz_cont);

            if (mFailedToBindService) {
                //MDTP service error - block the device
                initBlockDeviceBootView();
            }
            else if (isActivated) {
                 //MDTP is active on first boot - show factory reset protection screen
                initFactoryResetProtView();
            }
            else {
                //MDTP is inactive on first boot - allow user to sign up for the service
                initSignUpView();
            }
            //assume that initially the device MDTP is not authorized by the remote service
            mActiveFragment.setMdtpStateAuthorized(false);
        }

        //Update UI components according to the MDTP state and wizard scenario
        mActiveFragment.setMdtpStateActivated(isActivated);
        mTextViewMdtpState.setText(isActivated ? R.string.state_activated : R.string.state_deactivated );
        mButtonContinue.setEnabled(mActiveFragment.allowedToContinue() && !isActivationInProgress);
        Log.d(TAG, "mActiveFragment.allowedToContinue() - " + mActiveFragment.allowedToContinue());
        if (isActivated)
            mButtonContinue.setText(R.string.wiz_cont);
    }

    /**
     * Adds the current (mActiveFragment) wizard UI fragment to the wizard main layout
     */
    private void addWizardFragmentToLayout() {
        FragmentManager fragmentManager = getFragmentManager();
        FragmentTransaction fragmentTransaction = fragmentManager.beginTransaction();
        fragmentTransaction.add(R.id.wizard_container, mActiveFragment);
        fragmentTransaction.commit();
    }

    /**
     * Initialize the MDTP Sign-up screen
     * This layout allows user optional MDTP activation on first device boot
     */
    private void initSignUpView() {
        // Create a new Fragment to be placed in the activity layout
        Log.d(TAG,"initializing sign up view");
        mActiveFragment = new WizardUserSignUpFragment();
        addWizardFragmentToLayout();

        //Set the "continue" button text to skip
        mButtonContinue.setText(R.string.skip_activation);
        setTitle(R.string.wiz_title_sign_up);
    }

    /**
     * Initialize the MDTP factory reset protection screen
     * This layout is used when MDTP was found active on the first/clean boot
     */
    private void initFactoryResetProtView() {
        Log.d(TAG,"initializing FactoryResetProtView");
        mActiveFragment = new ResetProtectionFragment();
        addWizardFragmentToLayout();
        setTitle(R.string.wiz_title_reset_prot);
    }

    /**
     * Initialize the MDTP Boot block screen
     * This layout is used to block the device boot if MDTP services cannot be accessed
     * Can be due to MDTP internal error or if attacker succeeded to block MDTP services
     */
    private void initBlockDeviceBootView() {
        Log.d(TAG,"initializing initBlockDeviceBootView");
        mActiveFragment = new WizardBlockDeviceBootFragment();
        addWizardFragmentToLayout();
        setTitle(R.string.wiz_title_boot_block);
    }

    /********************** Activity Life Cycle *********************/

    /**
     * Create view
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "onCreate");
        super.onCreate(savedInstanceState);
        //finish activity right now if the demo wizard is not allowed to run
        boolean shouldUse = shouldUseDemoWizard();
        Log.d(TAG, "shouldUseDemoWizard:" + shouldUse);
        if (!shouldUse) {
            this.onStop();
            return;
        }
        Server.getInstance().loadState(this);
        Device.getInstance().loadState(this);
    }

    /**
     * Start view
     */
    @Override
    protected void onStart() {
        super.onStart();
        Log.d(TAG, "onStart");
        Server.getInstance().start(this);
    }

    /**
     * Resume view
     */
    @Override
    protected void onResume() {
        super.onResume();
        Log.d(TAG, "onResume");
        //this will also async update the activity UI by calling updateState
        Server.getInstance().bind(this);
        new SendActivateMsg(this).execute(SendActivateMsg.CHECK_BIND_CMD);
    }

    /**
     * Pause view
     */
    @Override
    protected void onPause() {
        super.onPause();
        Server.getInstance().unBind(this);
        Log.d(TAG, "onPause");
    }

    @Override
    protected void onStop() {
        super.onStop();
        Log.d(TAG, "onStop");
        boolean shouldUse = shouldUseDemoWizard();
        if (!shouldUse) {
            Log.d(TAG, "Disabling DemoWizard");
            disableWizardAndContinue();
            finish();
            return;
        }
    }

    /********************** UI/buttons event handlers *********************/

    /**
     * Wizard continue/unblock button handler
     */
    public void oclCont(View v) {
        disableWizardAndContinue();
    }

    /**
     * Server console button handler
     */
    public void oclServiceConsole(View v) {
        Intent intent = new Intent(this, ServerActivity.class);
        startActivity(intent);
    }

    /**
     * Launch emergency dialer activity
     */
    public void oclEmergencyDialer(View v) {
        final Intent intent = new Intent("com.android.phone.EmergencyDialer.DIAL");
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                        | Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
        startActivity(intent);
    }

    /**
     * Starts WiFi setup
     * Can be used to enable remote services activation/authorization during the setup
     */
    public void oclStartWifiWizard(View v) {
        Log.d(TAG, "oclStartWifiWizard");
        Intent intent = new Intent("com.android.net.wifi.SETUP_WIFI_NETWORK");
        intent.setComponent(
            ComponentName.unflattenFromString("com.android.settings/com.android.settings.wifi.WifiSetupActivity"));
        intent.addCategory("android.intent.category.DEFAULT");
        startActivity(intent);
    }

    /**
     * Call this function when finishing the wizard.
     * IMPORTANT: Do not use dontKillApp=false on Adnroid M, as the
     *            device can become stuck during the boot sequence.
     * Finishing the wizard should disable and finish activity
     */
    private void disableWizardAndContinue() {
        getPackageManager().setComponentEnabledSetting(getComponentName(),
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED, 0);
    }

    /**
     * Displays dialog for input of the local deactivation PIN
     * If PIN is valid calls DEACTIVATE task, otherwise dialog is dismissed
     */
    private void showPINDialog() {
        final AlertDialog.Builder pinAuth = new AlertDialog.Builder(this);
        final LayoutInflater inflater = LayoutInflater.from(this);
        final View view = inflater.inflate(R.layout.pin_dialog, null);
        final EditText pinText = (EditText) view.findViewById(R.id.edittext_pin);
        final Button okButton = (Button) view.findViewById(R.id.pin_dialog_button_ok);
        final Activity activity = this;

        pinAuth.setTitle(R.string.pin_dialog_title);
        pinAuth.setCancelable(false);
        pinAuth.setView(view);

        final AlertDialog pinDialog = pinAuth.create();

        okButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final String pin = pinText.getText().toString();
                //check PIN validity (5 digits)
                if (!pin.matches("[0-9]{5}")) {
                    pinText.setError(getString(R.string.pin_input_validation_warning));
                    return;
                }
                mLocalPin = pin; //will be used by DEACTIVATE_LCL_CMD
                pinDialog.dismiss();
                //Send the deactivate message to device MDTP service
                new SendActivateMsg(activity).execute(SendActivateMsg.DEACTIVATE_LCL_CMD);
            }
        });
        final Button cancelButton = (Button) view.findViewById(R.id.pin_dialog_button_cancel);
        cancelButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                pinDialog.dismiss();
            }
        });
        pinDialog.show();
    }

    /********************** MdtpActionListener implementation *********************/

    @Override
    public void onActivate() {
        Log.d(TAG, "MdtpActionListener onActivate");
        new SendActivateMsg(this).execute(SendActivateMsg.ACTIVATE_CMD);
    }

    @Override
    public void onLocalDeactivate() {
        Log.d(TAG, "MdtpActionListener onLocalDeactivate");
        showPINDialog(); // will call DEACTIVATE_LCL_CMD after PIN entry
    }

    @Override
    public void onRemoteDeactivate() {
        Log.d(TAG, "MdtpActionListener onRemoteDeactivate");
        new SendActivateMsg(this).execute(SendActivateMsg.DEACTIVATE_CMD);
    }

    @Override
    public void onAuthorizationRequest() {
        Log.d(TAG, "MdtpActionListener onAuthorizationRequest");
        boolean isAuthorized = Server.getInstance().getAllowWizardUnblock();
        mActiveFragment.setMdtpStateAuthorized(isAuthorized);
        String authMesage = isAuthorized ? "MDTP successsfully authorized" : "MDTP not authorized!";
        Toast.makeText(this, authMesage, Toast.LENGTH_LONG).show();
        updateState();
    }

    /********************** support functions *********************/

    /**
     * Enable the wizard only if this is the first boot and the production system property is set
     */
    private boolean shouldUseDemoWizard() {
        String propWizardEnabled = getSystemProperty(MDTP_SETUPWIZARD_ENABLE_SYSPROP, "undefined");
        Log.d(TAG, "shouldUseMDTPWizard " + MDTP_SETUPWIZARD_ENABLE_SYSPROP + " is " + propWizardEnabled);

        //we should check if the wizard is explicitly enabled
        if (propWizardEnabled.charAt(0) == '1') {
            return true;
        }
        //otherwise the demo wizard UI should never appear or block the boot
        return false;
    }

    /**
     * Reads Android system property using undocumented framework class
     * Should be used with care due potential future incompatibility
     * @param propName - property name to read
     * @param defaultValue - value to return if the property was defined in the system
     */
    private String getSystemProperty(String propName,String defaultValue) {
        String prop = "";
        try {
            Class classFromName = null;
            classFromName = Class.forName("android.os.SystemProperties");
            Method method;
            method = classFromName.getDeclaredMethod("get", String.class);
            prop = (String)method.invoke(null, propName);
            if (prop.isEmpty())
                prop = defaultValue;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return prop;
    }
}
