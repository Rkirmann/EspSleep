package no.nordicsemi.android.blinky.utils;

import android.content.Context;
import android.security.keystore.KeyGenParameterSpec;
import android.util.Log;

import androidx.security.crypto.EncryptedFile;
import androidx.security.crypto.MasterKeys;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.GeneralSecurityException;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class PasswordManager {

    private File file;
    private KeyGenParameterSpec keyGenParameterSpec;
    private String masterKeyAlias;
    private EncryptedFile encryptedFile;
    private Map<String, String> wifiMap;

    public PasswordManager(Context context) {
        // Although you can define your own key generation parameter specification, it's
        // recommended that you use the value specified here.
        keyGenParameterSpec = MasterKeys.AES256_GCM_SPEC;
        String filename = "my_sensitive_data.txt";
        file = new File(context.getFilesDir(), filename);

        try {
            masterKeyAlias = MasterKeys.getOrCreate(keyGenParameterSpec);
            encryptedFile = new EncryptedFile.Builder(
                    file,
                    context,
                    masterKeyAlias,
                    EncryptedFile.FileEncryptionScheme.AES256_GCM_HKDF_4KB
            ).build();
        } catch (GeneralSecurityException | IOException e) {
            e.printStackTrace();
        }

        try {
            wifiMap = read();
        } catch (GeneralSecurityException | IOException e) {
            e.printStackTrace();
        }

    }

    public Map<String, String> getWifiMap() {
        return wifiMap;
    }

    public Map<String, String> read() throws GeneralSecurityException, IOException {
        FileInputStream fis = encryptedFile.openFileInput();
        Map<String, String> tempMap = new HashMap<>();


        try (BufferedReader reader =
                     new BufferedReader(new InputStreamReader(fis))) {
            String line = reader.readLine();
            // Log.d("filereader", line);
            while (line != null) {
                List<String> split = Arrays.asList(line.split(";"));
                if (split.size() > 1) {
                    String lineSSID = split.get(0);
                    String linePassword = split.get(1);
                    tempMap.put(lineSSID, linePassword);
                }
                line = reader.readLine();
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
        return tempMap;
    }

    public void write(String ssid, String password) {
        if (file.exists())
           Log.d("Filereader", "File deleted: " + file.delete());
        wifiMap.put(ssid, password);
        try {
            try (BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(encryptedFile.openFileOutput()))) {
                for (Map.Entry<String, String> entry : wifiMap.entrySet()) {
                    writer.write( entry.getKey() + ";" + entry.getValue() );
                    writer.newLine();
                }
            }
        } catch (IOException | GeneralSecurityException e) {
            Log.e("Writer", e.toString());
        }
    }
}
