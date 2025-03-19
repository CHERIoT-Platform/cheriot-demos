package org.cheriot.hughcontroller;

import android.os.Bundle;
import android.text.Editable;
import android.view.*;
import android.content.Intent;

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

import com.google.zxing.integration.android.IntentIntegrator;
import com.google.zxing.integration.android.IntentResult;

import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.NativeLong;


public class MainActivity extends AppCompatActivity implements android.view.View.OnClickListener, com.skydoves.colorpickerview.listeners.ColorEnvelopeListener {

    String MQTTName;
    Mqtt3BlockingClient MQTTClient;

    // Set to true to use the demo.cheriot.org MQTT server (may not be running!).
    final boolean UseCHERIoTDemoServer = false;

    byte key[];
    final byte[] context = new byte[8];

    public interface Hydrogen extends Library {
        Hydrogen INSTANCE = (Hydrogen)
                Native.loadLibrary("hydrogen", Hydrogen.class);

        /*
        int
        hydro_secretbox_encrypt(uint8_t *c, const void *m_, size_t mlen, uint64_t msg_id,
                                const char    ctx[hydro_secretbox_CONTEXTBYTES],
                                const uint8_t key[hydro_secretbox_KEYBYTES])
         */
        int hydro_secretbox_encrypt(byte[] cyphertextOut, byte[] message, NativeLong messageLength, long messageId, byte[] context, byte[] key);
    }

    byte[] hydro_secretbox_encrypt(byte[] message) {
        final int hydro_secretbox_HEADERBYTES = (20 + 16);
        if (key == null) {
            return null;
        }
        byte[] cypherText = new byte[message.length + hydro_secretbox_HEADERBYTES];
        if (context.length != 8) {
            android.util.Log.e("libhydrogen", "Invalid context length");
            return null;
        }
        if (key.length != 32) {
            android.util.Log.e("libhydrogen", "Invalid key length");
            return null;
        }
        for (int i = 0; i < context.length; i++) {
            context[i] = 0;
        }
        int result = Hydrogen.INSTANCE.hydro_secretbox_encrypt(cypherText, message, new NativeLong(message.length), 0, context, key);
        if (result != 0) {
            android.util.Log.e("libhydrogen", "hydro_secretbox_encrypt failed with: " + result);
            return null;
        }
        return cypherText;
    }

    @Override
    protected void onSaveInstanceState (Bundle outState)
    {
        super.onSaveInstanceState(outState);
        outState.putByteArray("key", key);
        outState.putString("topic", MQTTName);
    }

    @Override
    protected void onRestoreInstanceState (Bundle savedInstanceState)
    {
        super.onRestoreInstanceState(savedInstanceState);
        key = savedInstanceState.getByteArray("key");
        MQTTName = savedInstanceState.getString("topic");
        initialiseMQTT();
    }

    private void initialiseMQTT()
    {
        try {
            final String ClientIdCharacters =
                    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
            StringBuilder clientID = new StringBuilder();
            java.util.Random rand = new java.util.Random();
            for (int i = 0; i < 32; i++) {
                clientID.append(ClientIdCharacters.charAt(rand.nextInt(ClientIdCharacters.length())));
            }
            android.util.Log.i("MQTT Client ID", clientID.toString());
            String server = UseCHERIoTDemoServer ? "demo.cheriot.org" : "test.mosquitto.org";
            int port = UseCHERIoTDemoServer ? 8883 : 8886;
            MQTTClient = Mqtt3Client.builder()
                    .serverHost(server)
                    .serverPort(port)
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
    }

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

        initialiseMQTT();

        android.widget.Button btnQRScan = findViewById(R.id.btnQRScan);
        btnQRScan.setOnClickListener(this);

        android.widget.Button btnForgetKey = findViewById(R.id.btnForgetKey);
        btnForgetKey.setOnClickListener(this);

        ColorPickerView colourPicker = findViewById(R.id.colorPickerView);
        colourPicker.setColorListener(this);
        BrightnessSlideBar brightnessSlideBar = findViewById(R.id.brightnessSlide);
        colourPicker.attachBrightnessSlider(brightnessSlideBar);
    }


    public void onColorSelected(com.skydoves.colorpickerview.ColorEnvelope color, boolean fromUser) {
        if (MQTTName == null || key == null) {
            android.util.Log.i("MQTT", "Lightbulb name not yet set");
        }
        int[] argb = color.getArgb();
        android.util.Log.i("Colour R", String.valueOf(argb[1]));
        android.util.Log.i("Colour G", String.valueOf(argb[2]));
        android.util.Log.i("Colour B", String.valueOf(argb[3]));
        byte[] payloadData = new byte[3];
        payloadData[0] = (byte) argb[1];
        payloadData[1] = (byte) argb[2];
        payloadData[2] = (byte) argb[3];
        payloadData = hydro_secretbox_encrypt(payloadData);
        if (payloadData == null) {
            android.util.Log.e("libhydrogen", "Encryption failed!");
            return;
        }
        try {
            android.util.Log.i("MQTT", "Publishing message to topic " + MQTTName);
            MQTTClient.publishWith().topic(MQTTName).qos(MqttQos.AT_LEAST_ONCE).payload(payloadData).send();
        } catch (Exception e) {
            android.util.Log.w("MQTT", "Publishing error" + e.getMessage());
        }
    }


    @Override
    public void onClick(View view) {
        if (view.getId() == R.id.btnQRScan) {
            IntentIntegrator intentIntegrator = new IntentIntegrator(this);
            intentIntegrator.setPrompt("Scan a QR Code on Hugh the Lightbulb");
            // This is undocumented, but asking for the data as an 8-bit character set lets us pass through 8-bit data, even though the API really wants to give us a string.
            intentIntegrator.addExtra("CHARACTER_SET", "ISO-8859-1");
            android.util.Log.i("QR Code", "Scanning QR code");
            intentIntegrator.initiateScan();
        } else if (view.getId() == R.id.btnForgetKey) {
            key = new byte[32];
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        IntentResult intentResult = IntentIntegrator.parseActivityResult(requestCode, resultCode, data);
        if (intentResult != null) {
            android.util.Log.i("QR Code", intentResult.toString());
            String resultString = intentResult.getContents();

            byte[] result;
            android.util.Log.i("QR Code", "Length: " + intentResult.getContents().length());
            try {
                result = resultString.getBytes("ISO-8859-1");
            } catch (Exception e) {
                android.util.Log.e("QR Code", "Failed charset conversion");
                return;
            }

            if (result.length < 40) {
                android.util.Log.e("QR Code", "Invalid length: " + result.length);
                return;
            }
            // Extract the key
            key = java.util.Arrays.copyOfRange(result, 8, 40);
            // Extract the data
            try {
                MQTTName = "hugh-the-lightbulb/" + new String(java.util.Arrays.copyOfRange(result, 0, 8), "ISO-8859-1");
                android.util.Log.i("QR Code", "got QR data: " + MQTTName);
            } catch (Exception e) {
            }
        }
    }


}