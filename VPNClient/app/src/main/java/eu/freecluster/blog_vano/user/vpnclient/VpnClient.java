package eu.freecluster.blog_vano.user.vpnclient;

import android.app.Activity;
import android.graphics.drawable.AnimatedVectorDrawable;
import android.os.Bundle;

import android.content.Intent;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;

public class VpnClient extends Activity {
    private boolean isConnected = false;
    private TextView serverAddress;
    private TextView serverPort;
    private TextView sharedSecret;
    private ImageView buttonImageView;

    public interface Prefs {
        String NAME = "connection";
        String SERVER_ADDRESS = "server.address";
        String SERVER_PORT = "server.port";
        String SHARED_SECRET = "shared.secret";
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.form);

        serverAddress   = (TextView) findViewById(R.id.address);
        serverPort      = (TextView) findViewById(R.id.port);
        sharedSecret    = (TextView) findViewById(R.id.secret);
        buttonImageView = (ImageView) findViewById(R.id.buttonImageView);

        final SharedPreferences prefs = getSharedPreferences(Prefs.NAME, MODE_PRIVATE);
        serverAddress.setText(prefs.getString(Prefs.SERVER_ADDRESS, ""));
        serverPort.setText(prefs.getString(Prefs.SERVER_PORT, ""));
        sharedSecret.setText(prefs.getString(Prefs.SHARED_SECRET, ""));

        buttonImageView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if(correctInputs()) {
                    if (!isConnected) {
                        prefs.edit()
                                .putString(Prefs.SERVER_ADDRESS, serverAddress.getText().toString())
                                .putString(Prefs.SERVER_PORT, serverPort.getText().toString())
                                .putString(Prefs.SHARED_SECRET, sharedSecret.getText().toString())
                                .commit();

                        Intent intent = android.net.VpnService.prepare(VpnClient.this);
                        if (intent != null) {
                            // start vpn service:
                            startActivityForResult(intent, 0);
                        } else {
                            onActivityResult(0, RESULT_OK, null);
                        }
                    } else {
                        startService(getServiceIntent().setAction(CustomVpnService.ACTION_DISCONNECT));
                    }
                    // start lock/unlock animation:
                    startLockAnimation(isConnected, buttonImageView);
                    isConnected = !isConnected;
                }
            }
        });

    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        if (result == RESULT_OK) {
            startService(getServiceIntent().setAction(CustomVpnService.ACTION_CONNECT));
        }
    }

    private Intent getServiceIntent() {
        return new Intent(this, CustomVpnService.class);
    }

    // animation setting:
    public void startLockAnimation(boolean isLocked, ImageView view) {
        AnimatedVectorDrawable drawable
                = (AnimatedVectorDrawable) getDrawable(isLocked ?
                R.drawable.animated_unlock :
                R.drawable.animated_lock);

        view.setImageDrawable(drawable);
        drawable.start();
    }

    public boolean correctInputs() {
        ArrayList<TextView> list = new ArrayList<TextView>();
        list.add(serverAddress);
        list.add(serverPort);
        list.add(sharedSecret);

        for(TextView view : list) {
            if(isTextViewEmpty(view)) {
                Toast.makeText(getApplicationContext(),
                        "One of the fields is empty!",
                        Toast.LENGTH_SHORT).show();
                return false;
            }
        }

        /**
         * Port valid value test:
         */
        int port = Integer.parseInt(serverPort.getText().toString().trim());
        Log.d("PORT TEST", "Port is " + port);
        if(port < 1 || port > 0xFFFF) {
            Toast.makeText(getApplicationContext(), "Incorrect port!", Toast.LENGTH_SHORT).show();
            return false;
        }

        return true;
    }

    // check textview input
    public boolean isTextViewEmpty(TextView tv) {
        return tv.getText().toString().trim().isEmpty();
    }

    /*
    @Override
    public void onPause() {
        super.onPause();
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(this);
        SharedPreferences.Editor editor = preferences.edit();
        editor.putBoolean("LockButtonStatus", isConnected);
        editor.commit();
        //editor.apply();
    }

    @Override
    public void onResume() {
        super.onResume();
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(this);
        isConnected = preferences.getBoolean("LockButtonStatus", false);
        startLockAnimation(isConnected, buttonImageView);
    }*/
}