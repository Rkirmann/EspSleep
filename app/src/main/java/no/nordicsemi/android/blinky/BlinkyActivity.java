/*
 * Copyright (c) 2018, Nordic Semiconductor
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package no.nordicsemi.android.blinky;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.icu.util.TimeZone;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.TimePicker;

import androidx.annotation.RequiresApi;
import androidx.appcompat.app.AppCompatActivity;
import androidx.lifecycle.ViewModelProvider;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.switchmaterial.SwitchMaterial;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.List;

import butterknife.BindView;
import butterknife.ButterKnife;
import butterknife.OnClick;
import no.nordicsemi.android.ble.livedata.state.ConnectionState;
import no.nordicsemi.android.blinky.adapter.DiscoveredBluetoothDevice;
import no.nordicsemi.android.blinky.utils.PasswordManager;
import no.nordicsemi.android.blinky.viewmodels.BlinkyViewModel;

@SuppressWarnings("ConstantConditions")
public class BlinkyActivity extends AppCompatActivity {
    public static final String EXTRA_DEVICE = "no.nordicsemi.android.blinky.EXTRA_DEVICE";

    private BlinkyViewModel viewModel;
    private WifiManager mWifiManager;
    private List<ScanResult> wifiList;
    private int index = 0;
    private PasswordManager passwordManager;

    @BindView(R.id.led_switch)
    SwitchMaterial led;
    @BindView(R.id.button_state)
    TextView buttonState;
    //@BindView(R.id.sync) Button sync;
    @BindView(R.id.ssid)
    TextView ssidField;
    @BindView(R.id.wifi_password)
    EditText pwField;
    @BindView(R.id.datePicker1)
    TimePicker timePicker;
    @BindView(R.id.sync)
    Button sync;

    private final BroadcastReceiver mWifiScanReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context c, Intent intent) {
            if (intent.getAction().equals(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION)) {
                wifiList = mWifiManager.getScanResults();
                Log.i("WIFI", "wifi scan results:");
                for (ScanResult mScanResult : wifiList) {
					Log.i("WIFI", mScanResult.SSID);
                }
                String bssid = "no wifi found";
                try {
                    bssid = wifiList.get(index).SSID;
                } catch (NullPointerException ignored) {
                }
                ssidField.setText(bssid);

                pwField.setText(passwordManager.getWifiMap().get(bssid));
            }
        }
    };

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_blinky);
        ButterKnife.bind(this);

        final Intent intent = getIntent();
        final DiscoveredBluetoothDevice device = intent.getParcelableExtra(EXTRA_DEVICE);
        final String deviceName = device.getName();
        final String deviceAddress = device.getAddress();
        passwordManager = new PasswordManager(getApplicationContext());

        final MaterialToolbar toolbar = findViewById(R.id.toolbar);
        toolbar.setTitle(deviceName != null ? deviceName : getString(R.string.unknown_device));
        toolbar.setSubtitle(deviceAddress);
        setSupportActionBar(toolbar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        // Configure the view model.
        viewModel = new ViewModelProvider(this).get(BlinkyViewModel.class);
        viewModel.connect(device);

        // Set up views.
        final TextView ledState = findViewById(R.id.led_state);
        final LinearLayout progressContainer = findViewById(R.id.progress_container);
        final TextView connectionState = findViewById(R.id.connection_state);
        final View content = findViewById(R.id.device_container);
        final View notSupported = findViewById(R.id.not_supported);


        //wifi scanner
        mWifiManager = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        registerReceiver(mWifiScanReceiver,
                new IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION));
        mWifiManager.startScan();


        //led.setOnCheckedChangeListener((buttonView, isChecked) -> viewModel.setLedState(isChecked));
        viewModel.getConnectionState().observe(this, state -> {
            switch (state.getState()) {
                case CONNECTING:
                    progressContainer.setVisibility(View.VISIBLE);
                    notSupported.setVisibility(View.GONE);
                    connectionState.setText(R.string.state_connecting);
                    sync.setEnabled(false);
                    break;
                case INITIALIZING:
                    connectionState.setText(R.string.state_initializing);
                    break;
                case READY:
                    progressContainer.setVisibility(View.GONE);
                    content.setVisibility(View.VISIBLE);
                    onConnectionStateChanged(true);
                    break;
                case DISCONNECTED:
                    if (state instanceof ConnectionState.Disconnected) {
                        final ConnectionState.Disconnected stateWithReason = (ConnectionState.Disconnected) state;
                        if (stateWithReason.isNotSupported()) {
                            progressContainer.setVisibility(View.GONE);
                            notSupported.setVisibility(View.VISIBLE);
                        }
                    }
                    // fallthrough
                case DISCONNECTING:
                    onConnectionStateChanged(false);
                    break;
            }
        });
        viewModel.getLedState().observe(this, isOn -> {
            ledState.setText(isOn ? R.string.turn_on : R.string.turn_off);
            led.setChecked(isOn);
        });
        viewModel.getButtonState().observe(this,
                pressed -> buttonState.setText(pressed ?
                        R.string.button_pressed : R.string.button_released));

        pwField.addTextChangedListener(new TextWatcher() {

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) { }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                if(s.toString().trim().length()==0){
                    sync.setEnabled(false);
                } else {
                    sync.setEnabled(true);
                }
            }

            @Override
            public void afterTextChanged(Editable s) { }
        });


    }


    @OnClick(R.id.action_clear_cache)
    public void onTryAgainClicked() {
        viewModel.reconnect();
    }


    @OnClick(R.id.nextWifi)
    public void onNextWifiClicked() {
        if (wifiList.size() > 1) {
            index++;
            if (index > wifiList.size() - 1)
                index = 0;
            String wifiPass = wifiList.get(index).SSID;
            ssidField.setText(wifiPass);
            pwField.setText(passwordManager.getWifiMap().get(wifiPass));
        }
    }

    // sync button - write to esp32
    @OnClick(R.id.sync)
    public void onSyncClicked() {
        // send led state
        //viewModel.setLedState(led.isChecked());
        // calculate local time in millis
        int offset = java.util.TimeZone.getDefault().getRawOffset() + TimeZone.getDefault().getDSTSavings();
        long now = System.currentTimeMillis() + offset;
        // send current time in seconds
        //viewModel.setTime(now / 1000L);
        // send alarm time
        //viewModel.setAlarmTime(timePicker.getHour(), timePicker.getMinute());
        // save wifi credentials to file
        String ssid = ssidField.getText().toString();
        String pw = pwField.getText().toString();
        passwordManager.write(ssid, pw);
        // send wifi credentials
        //viewModel.setWifi(ssid, pw);

        // try sending json
        try {
            String jsonString = new JSONObject()
                    .put("ledState", led.isChecked() ? 1 : 0)
                    .put("currentTime", now /1000L)
                    .put("alarmHour",timePicker.getHour())
                    .put("alarmMinute", timePicker.getMinute())
                    .put("ssid", ssidField.getText().toString())
                    .put("password", pwField.getText().toString())
                    .toString();
            viewModel.sendJson(jsonString);
        } catch (JSONException e) {
            e.printStackTrace();
        }

    }

    @RequiresApi(api = Build.VERSION_CODES.N)
    private void onConnectionStateChanged(final boolean connected) {
        //led.setEnabled(connected);
        if (!connected) {
            //led.setChecked(false);
            Log.i("System", "system offline");
            buttonState.setText(R.string.button_unknown);
            viewModel.reconnect();
        } else {
            Log.i("System", "system back online");
            Log.i("LED", "led is checked: " + led.isChecked());
            //viewModel.setTime(System.currentTimeMillis() / 1000L);

        }
    }
}
