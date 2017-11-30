package apriorit.vpnclient;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.ServiceConnection;
import android.content.pm.ActivityInfo;
import android.graphics.drawable.AnimatedVectorDrawable;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Build;
import android.os.Bundle;

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.PopupMenu;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class VpnClient extends Activity{
    public static final int TRY_CONNECT = 0;
    public static final int CONNECT_SUCCESS = 1;
    public static final int DISCONNECT_SUCCESS = 2;
    public static final int CONNECT_FALSE = 3;
    public static final String EXTRA_MESSAGE = "apriorit.vpnclient.VpnClient.StartUp";
    public static final String OPEN_MESSAGE = "apriorit.vpnclient.VpnClient.AddCustomServer";
    public static final String BOOT_ON = "on";
    public static final String BOOT_OFF = "off";
    public static final String BOOT_REAL_MESSAGE = "START_BOOT";
    private CustomVpnService mService;
    private ImageView buttonImageView;
    private Spinner   serverSpinner;
    private SharedPreferences prefs;
    private ImageButton button_menu;
    private ArrayList<String> country_list;

    private ArrayAdapter<String> serverSpinnerAdapter;
    private boolean vpn_active = false;
    private boolean button_state = false;
    private boolean service_start = false;
    private boolean auto_runned = false;
    private int list_count = 0;

    private final Countries countries = new Countries(new CountryObject[] {
            new CountryObject(R.drawable.ic_flag_of_france, "France",
                    "5.135.153.169", "8000"),
            new CountryObject(R.drawable.ic_flag_of_the_united_states, "USA",
                    "192.241.141.236", "8000")
    });

    public interface Prefs {
        String NAME = "connection";
        String SERVER_ADDRESS = "server.address";
        String SERVER_PORT = "server.port";
        String SPINNER_POSITION = "spinner.position";
        String BUTTON_STATE = "button.state";
        String BOOTS = "BOOT_ON_START_VPN";
    }

    public class MessageHandler extends Handler {
        @Override
        public void handleMessage(Message message) {
            switch (message.arg1)
            {
                case CustomVpnService.BIND_SIGNAL:
                    service_start = true;
                    break;
                case CustomVpnService.UNBIND_SIGNAL:
                    service_start = false;
                    vpn_active = false;
                    break;
                case CustomVpnService.SIGNAL_VPN_FAIL:
                case CustomVpnService.SIGNAL_SERVICE_STOP:
                    startLockAnimation(vpn_active ? DISCONNECT_SUCCESS: CONNECT_FALSE, buttonImageView);
                    service_start = false;
                    vpn_active = false;
                    button_state = false;
                    unbindService(mConnection);
                    serverSpinner.setEnabled(true);
                    break;
                case CustomVpnService.SIGNAL_SUCCESS_DISCONNECT:
                case CustomVpnService.SIGNAL_FAIL_CONNECT:
                    startLockAnimation(vpn_active ? DISCONNECT_SUCCESS: CONNECT_FALSE, buttonImageView);
                    serverSpinner.setEnabled(true);
                    service_start = false;
                    vpn_active = false;
                    button_state = false;
                    break;
                case CustomVpnService.SIGNAL_SUCCESS_CONNECT:
                    startLockAnimation(CONNECT_SUCCESS, buttonImageView);
                    vpn_active = true;
                    break;
            }
        }
    }

    public Handler messageHandler = new MessageHandler();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.form);

        // forbid landscape screen orientation:
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);

        serverSpinner   = (Spinner) findViewById(R.id.serverSpinner);
        buttonImageView = (ImageView) findViewById(R.id.buttonImageView);

        country_list = new ArrayList<>(Arrays.asList(countries.getCountriesNames()));

        serverSpinnerAdapter = new ServerSpinnerAdapter<>(VpnClient.this,
                R.layout.custom_spinner_object,
                country_list);

        serverSpinner.setAdapter(serverSpinnerAdapter);

        list_count = countries.GetCount();
        // Load preferences, such like chosen server and button state:
        prefs = getSharedPreferences(Prefs.NAME, MODE_PRIVATE);
        serverSpinner.setSelection(prefs.getInt(Prefs.SPINNER_POSITION, 0) >= list_count ? 0 :
                                                     prefs.getInt(Prefs.SPINNER_POSITION, 0));
        button_menu = (ImageButton) findViewById(R.id.buttonMenu);
        button_menu.setOnClickListener(buttonMenuClickListener);

        if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            AnimatedVectorDrawable drawable =
                    (AnimatedVectorDrawable) getDrawable(R.drawable.unlocked_at_start);
            buttonImageView.setImageDrawable(drawable);
            drawable.start();
            buttonImageView.refreshDrawableState();
        }


        Log.i("ABSOLUTE_PATH", getApplicationContext().getFilesDir().getAbsolutePath());

        buttonImageView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if(service_start == false &&service_start == button_state) {
                        if (!hasInternetConnection()) {
                            Toast.makeText(getApplicationContext(),
                                    "Error: can't access internet",
                                    Toast.LENGTH_LONG).show();
                            return;
                        }

                        prefs.edit()
                                .putString(Prefs.SERVER_ADDRESS, countries.getIpAddresses()[serverSpinner.getSelectedItemPosition()])
                                .putString(Prefs.SERVER_PORT, countries.getServerPorts()[serverSpinner.getSelectedItemPosition()])
                                .putInt(Prefs.SPINNER_POSITION, serverSpinner.getSelectedItemPosition())
                                .putBoolean(Prefs.BUTTON_STATE, !service_start)
                                .commit();

                    final int port_client;
                    try {
                        port_client = Integer.parseInt(prefs.getString(VpnClient.Prefs.SERVER_PORT, ""));
                    } catch (NumberFormatException e) {
                        Log.e("Bad port: " + prefs.getString(VpnClient.Prefs.SERVER_PORT, null),"client");
                        Toast.makeText(getApplicationContext(),
                                "Bad port",
                                Toast.LENGTH_LONG).show();
                        return;
                    }


                        Intent intent = android.net.VpnService.prepare(VpnClient.this);
                        if (intent != null) {
                            // start vpn service:
                            startActivityForResult(intent.putExtra("ConStat", new Messenger(messageHandler)), 0);
                        } else {
                            onActivityResult(0, RESULT_OK, null);
                        }
                        startLockAnimation(TRY_CONNECT, buttonImageView);
                        button_state = true;
                } else if(service_start == true && service_start == button_state){
                            mService.SetDisconnect(vpn_active ?
                                    CustomVpnService.SIGNAL_SUCCESS_DISCONNECT
                                    : CustomVpnService.SIGNAL_FAIL_CONNECT);
                            unbindService(mConnection);
                            button_state = false;
                } else if(mService !=null)
                    mService.SendWaitMessage();

                serverSpinner.setEnabled(!button_state);
            }
        });

        if(!auto_runned)
        {
            Intent intent_super = getIntent();
            if (intent_super != null) {
                String message = intent_super.getStringExtra(EXTRA_MESSAGE);
                if (message != null && message.equals(BOOT_REAL_MESSAGE)) {
                    buttonImageView.callOnClick();
                    //onBackPressed();
                }
            }
            auto_runned = true;
        }
    }

    View.OnClickListener buttonMenuClickListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            showPopupMenu(v);
        }
    };

    private void showPopupMenu(View v) {
        PopupMenu popupMenu = new PopupMenu(this, v);
        popupMenu.inflate(R.menu.popupmenu);
        popupMenu.getMenu().getItem(0).setChecked(prefs.getString(Prefs.BOOTS, "").equals(BOOT_ON));

        popupMenu
                .setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {

                    @Override
                    public boolean onMenuItemClick(MenuItem item) {
                        switch (item.getItemId()) {
                            case R.id.menu1:
                                item.setChecked(!item.isChecked());
                                prefs.edit()
                                        .putString(Prefs.BOOTS, item.isChecked() ? BOOT_ON : BOOT_OFF)
                                        .apply();
                                Toast.makeText(getApplicationContext(),
                                        "Boot on turn on is " + (item.isChecked() ?
                                                "enabled" :
                                                "disabled"),
                                        Toast.LENGTH_SHORT).show();
                                return true;
                            case R.id.menu2:
                                Intent intent = new Intent(getBaseContext(), AddCustomServer.class);
                                startActivityForResult(intent,1);
                                return true;
                            default:
                                return false;
                        }
                    }
                });
        popupMenu.show();
    }

    @Override
    public void onBackPressed() {
        moveTaskToBack(true);
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        if (request==0) {
            if(result == RESULT_OK) {
                bindService(getServiceIntent().putExtra("ConStat", new Messenger(messageHandler)), mConnection, Context.BIND_AUTO_CREATE);
            }
            else
            {
                startLockAnimation(vpn_active ? DISCONNECT_SUCCESS: CONNECT_FALSE, buttonImageView);
                serverSpinner.setEnabled(true);
                service_start = false;
                vpn_active = false;
                button_state = false;
            }
        }
        if (request == 1) {
            if (result == RESULT_OK) {
                if(list_count == countries.GetCount())
                    country_list.add("");
                Bundle bundle = data.getExtras();
                countries.AddNewCountry(
                        bundle.getString(AddCustomServer.Prefs.NEW_NAME),
                        bundle.getString(AddCustomServer.Prefs.NEW_ADDRESS),
                        bundle.getString(AddCustomServer.Prefs.NEW_PORT));
                serverSpinnerAdapter.notifyDataSetChanged();
            }
        }
    }

    @Override
    protected void onDestroy() {
        getApplicationContext().unbindService(mConnection);
        super.onDestroy();
    }

    /** Defines callbacks for service binding, passed to bindService() */
    private ServiceConnection mConnection = new ServiceConnection() {

        @Override
        public void onServiceConnected(ComponentName className,
                                       IBinder service) {
            // We've bound to LocalService, cast the IBinder and get LocalService instance
            CustomVpnService.LocalBinder binder = (CustomVpnService.LocalBinder) service;
            mService = binder.getService();
        }

        @Override
        public void onServiceDisconnected(ComponentName arg0) {
        }
    };

    private Intent getServiceIntent() {
        return new Intent(this, CustomVpnService.class);
    }

    // animation setting:
    public void startLockAnimation(int lock_type, ImageView view) {
        boolean isLowerThanLollipop = Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP;
        int id = 0;
        switch(lock_type)
        {
            case TRY_CONNECT:
                id = isLowerThanLollipop ? R.drawable.orange_lock : R.drawable.try_lock;
                break;
            case CONNECT_SUCCESS:
                id = isLowerThanLollipop ? R.drawable.green_lock : R.drawable.animated_lock;
                break;
            case DISCONNECT_SUCCESS:
                id = isLowerThanLollipop ? R.drawable.lock : R.drawable.animated_unlock;
                break;
            case CONNECT_FALSE:
                id = isLowerThanLollipop ? R.drawable.lock : R.drawable.unlocked_at_start;
                break;

        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            AnimatedVectorDrawable drawable = (AnimatedVectorDrawable) getDrawable(id);
            view.setImageDrawable(drawable);
            drawable.start();
        } else {
            buttonImageView.setImageResource(id);
            buttonImageView.refreshDrawableState();
        }
    }

    public boolean hasInternetConnection() {
        ConnectivityManager mgr = (ConnectivityManager) this.getSystemService(Context.CONNECTIVITY_SERVICE);
        return (mgr.getNetworkInfo(ConnectivityManager.TYPE_MOBILE).getState() == NetworkInfo.State.CONNECTED
                || mgr.getNetworkInfo(ConnectivityManager.TYPE_WIFI).getState() == NetworkInfo.State.CONNECTED);
    }

    public class ServerSpinnerAdapter<T> extends ArrayAdapter<T> {
        public ServerSpinnerAdapter(Context context, int textViewResourceId, List<T> objects) {
            super(context, textViewResourceId, objects);
        }

        public View getCustomView(int pos, View convertView, ViewGroup parent) {
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

}