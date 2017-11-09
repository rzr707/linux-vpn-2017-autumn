package eu.freecluster.blog_vano.user.vpnclient;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.graphics.drawable.AnimatedVectorDrawable;
import android.os.Bundle;

import android.content.Intent;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;

public class VpnClient extends Activity {
    private boolean isConnected = false;
    private String serverAddress;
    private String serverPort;
    private String sharedSecret;
    private ImageView buttonImageView;
    private Spinner   serverSpinner;


    private final Countries countries = new Countries(new CountryObject[] {
            new CountryObject(R.drawable.ic_flag_of_ukraine, "Home LAN server",
                    "192.168.0.104", "8000"),
            new CountryObject(R.drawable.ic_flag_of_the_united_states, "United States",
                    "192.241.141.236", "8000")
    });


    public interface Prefs {
        String NAME = "connection";
        String SERVER_ADDRESS = "server.address";
        String SERVER_PORT = "server.port";
        String SHARED_SECRET = "shared.secret";
        String SPINNER_POSITION = "spinner.position";
        String BUTTON_STATE = "button.state";
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.form);

        // forbid landscape screen orientation:
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);

        serverSpinner   = (Spinner) findViewById(R.id.serverSpinner);
        buttonImageView = (ImageView) findViewById(R.id.buttonImageView);

        serverSpinner.setAdapter(new ServerSpinnerAdapter(VpnClient.this,
                R.layout.custom_spinner_object,
                countries.getCountriesNames()));

        // Load preferences, such like chosen server and button state:
        final SharedPreferences prefs = getSharedPreferences(Prefs.NAME, MODE_PRIVATE);
        serverSpinner.setSelection(prefs.getInt(Prefs.SPINNER_POSITION, 0));
        serverAddress = prefs.getString(Prefs.SERVER_ADDRESS,
                countries.getCountriesNames()[serverSpinner.getSelectedItemPosition()]);
        serverPort = prefs.getString(Prefs.SERVER_PORT,
                countries.getServerPorts()[serverSpinner.getSelectedItemPosition()]);
        isConnected = prefs.getBoolean(Prefs.BUTTON_STATE, false);

        sharedSecret = "test"; // @todo: remove shared secret

        AnimatedVectorDrawable drawable
                = (AnimatedVectorDrawable) getDrawable(!isConnected ?
                R.drawable.animated_unlock :
                R.drawable.animated_lock);
        buttonImageView.setImageDrawable(drawable);
        drawable.start();
        buttonImageView.refreshDrawableState();



        Log.i("ABSOLUTE_PATH", getApplicationContext().getFilesDir().getAbsolutePath());

        buttonImageView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                    if (!isConnected) {
                        prefs.edit()
                                .putString(Prefs.SERVER_ADDRESS, countries.getIpAddresses()[serverSpinner.getSelectedItemPosition()])
                                .putString(Prefs.SERVER_PORT, countries.getServerPorts()[serverSpinner.getSelectedItemPosition()])
                                .putString(Prefs.SHARED_SECRET, "test")
                                .putInt(Prefs.SPINNER_POSITION, serverSpinner.getSelectedItemPosition())
                                .putBoolean(Prefs.BUTTON_STATE, !isConnected)
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
                    prefs.edit().putBoolean(Prefs.BUTTON_STATE, isConnected).commit();
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


    // check textview input
    public boolean isTextViewEmpty(TextView tv) {
        return tv.getText().toString().trim().isEmpty();
    }

    public class ServerSpinnerAdapter extends ArrayAdapter {
        public ServerSpinnerAdapter(Context context, int textViewResourceId, String[] objects) {
            super(context, textViewResourceId, objects);
        }

        public View getCustomView(int pos, View convertView, ViewGroup parent) {
            // inflating the layout for the custom spinner:
            LayoutInflater inflater = getLayoutInflater();
            View layout = inflater.inflate(R.layout.custom_spinner_object,
                    parent,
                    false);

            // declaring and typecasting the TextView in the inflated layout:
            TextView tvServerCountry = (TextView) layout.findViewById(R.id.tvServerCountry);
            // set the text using the array:
            tvServerCountry.setText(countries.getCountriesNames()[pos]);

            // declaring and typecasting the ImageView in the inflated layout:
            ImageView img = (ImageView) layout.findViewById(R.id.imgServerCountry);
            // set the pic using the array:
            img.setImageResource(countries.getCountriesIds()[pos]);

            /*
             Here we can change font size and color:
             */

            return layout;
        }

        @Override
        public View getDropDownView(int pos, View convertView, ViewGroup parent) {
            return getCustomView(pos, convertView, parent);
        }

        @Override
        public View getView(int pos, View convertView, ViewGroup parent) {
            return getCustomView(pos, convertView, parent);
        }
    }

    public class Countries {
        private CountryObject[] countries;

        public Countries(CountryObject[] countries) {
            this.countries = countries;
        }

        public Integer[] getCountriesIds() {
            Integer[] result = new Integer[countries.length];
            for(int i = 0; i < countries.length; ++i) {
                result[i] = countries[i].getFlagId();
            }
            return result;
        }

        public String[] getCountriesNames() {
            String[] result = new String[countries.length];
            for(int i = 0; i < countries.length; ++i) {
                result[i] = countries[i].getCountryName();
            }
            return result;
        }

        public String[] getIpAddresses() {
            String[] result = new String[countries.length];
            for(int i = 0; i < countries.length; ++i) {
                result[i] = countries[i].getIpAddr();
            }
            return result;
        }

        public String[] getServerPorts() {
            String[] result = new String[countries.length];
            for(int i = 0; i < countries.length; ++i) {
                result[i] = countries[i].getPort();
            }
            return result;
        }
    }

    public class CountryObject {
        private int    flagId;
        private String countryName;
        private String ipAddr;
        private String port;

        public CountryObject(int flagId, String countryName, String ipAddr, String port) {
            this.flagId = flagId;
            this.countryName = countryName;
            this.ipAddr = ipAddr;
            this.port = port;
        }

        public int getFlagId()         { return flagId;      }
        public String getCountryName() { return countryName; }
        public String getIpAddr()      { return ipAddr;      }
        public String getPort()        { return port;        }
    }
}