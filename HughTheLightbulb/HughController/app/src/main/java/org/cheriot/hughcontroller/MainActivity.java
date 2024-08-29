package org.cheriot.hughcontroller;

import android.os.Bundle;
import android.text.Editable;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import com.google.android.material.textfield.TextInputEditText;
import com.hivemq.client.mqtt.datatypes.MqttQos;
import com.skydoves.colorpickerview.ColorPickerView;
import com.hivemq.client.mqtt.mqtt3.*;
import com.skydoves.colorpickerview.sliders.BrightnessSlideBar;

public class MainActivity extends AppCompatActivity implements android.text.TextWatcher, com.skydoves.colorpickerview.listeners.ColorEnvelopeListener {

    String MQTTName;
    Mqtt3BlockingClient MQTTClient;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        android.util.Log.i("MQTT", "App started");
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_main);
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });

        try {
            final String ClientIdCharacters =
                    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
            StringBuilder clientID = new StringBuilder();
            java.util.Random rand = new java.util.Random();
            for (int i=0 ; i<32 ; i++) {
                clientID.append(ClientIdCharacters.charAt(rand.nextInt(ClientIdCharacters.length())));
            }
            android.util.Log.i("MQTT Client ID", clientID.toString());
            MQTTClient = Mqtt3Client.builder()
                    .serverHost("test.mosquitto.org")
                    .serverPort(8886)
                    .sslWithDefaultConfig()
                    .automaticReconnectWithDefaultConfig()
                    .identifier(clientID.toString())
                    .buildBlocking();
            android.util.Log.i("MQTT Client connection", "Started");
            MQTTClient.connect();
            android.util.Log.i("MQTT Client connection", "Completed");
        } catch (Exception e) {
            android.util.Log.w("MQTT", "Connection error" + e.getMessage());
        }
        TextInputEditText button = findViewById(R.id.mqttid);
        button.addTextChangedListener(this);
        ColorPickerView colourPicker = findViewById(R.id.colorPickerView);
        colourPicker.setColorListener(this);
        BrightnessSlideBar brightnessSlideBar = findViewById(R.id.brightnessSlide);
        colourPicker.attachBrightnessSlider(brightnessSlideBar);
    }

    public void afterTextChanged(Editable s) {
        MQTTName = s.toString();
        android.util.Log.i("Set MQTTName", MQTTName);
    }

    public void	beforeTextChanged(CharSequence s, int start, int count, int after) {}

    public void	onTextChanged(CharSequence s, int start, int before, int count) {}

    public void onColorSelected(com.skydoves.colorpickerview.ColorEnvelope color, boolean fromUser) {
        if (MQTTName == null)
        {
            android.util.Log.i("MQTT", "Lightbulb name not yet set");
        }
        int[] argb = color.getArgb();
        android.util.Log.i("Colour R", String.valueOf(argb[1]));
        android.util.Log.i("Colour G", String.valueOf(argb[2]));
        android.util.Log.i("Colour B", String.valueOf(argb[3]));
        android.util.Log.i("MQTT Colour hex", color.getHexCode().toString());
        StringBuilder topic = new StringBuilder();
        topic.append("hugh-the-lightbulb/");
        topic.append(MQTTName);
        byte[] payloadData = new byte[3];
        payloadData[0] = (byte)argb[1];
        payloadData[1] = (byte)argb[2];
        payloadData[2] = (byte)argb[3];
        try
        {
            android.util.Log.i("MQTT", "Publishing message to topic " + topic.toString());
            MQTTClient.publishWith().topic(topic.toString()).qos(MqttQos.AT_LEAST_ONCE).payload(payloadData).send();
        } catch (Exception e)
        {
            android.util.Log.w("MQTT", "Publishing error" + e.getMessage());
        }
    }

}