package apriorit.vpnclient;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.util.Log;
import android.util.Pair;
import android.widget.Toast;

import com.wolfssl.WolfSSLException;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

public class CustomVpnService /* renamed from 'VpnService' */ extends android.net.VpnService implements Handler.Callback {
    public static final int BIND_SIGNAL = 0;
    public static final int UNBIND_SIGNAL = 1;
    public static final int SIGNAL_SUCCESS_DISCONNECT = 2;
    public static final int SIGNAL_SUCCESS_CONNECT = 3;
    public static final int SIGNAL_FAIL_CONNECT = 4;
    public static final int SIGNAL_SERVICE_STOP = 5;
    public static final  int max_rec_count = 10;
    private static final String TAG = CustomVpnService.class.getSimpleName();
    private Handler mHandler = null;
    private final IBinder mBinder = new LocalBinder();
    private Messenger messageHandler = null;
    private boolean already_bind = false; // to use correct messageHandler
    private boolean already_unbind = false;
    public boolean service_can_be_closed = false;
    private boolean already_show_connect = false;
    public ParcelFileDescriptor old_vpn_interface = null;

    private int reconnect_count = 0;
    private boolean interrupt = false;

    /**
     * Class used for the client Binder.  Because we know this service always
     * runs in the same process as its clients, we don't need to deal with IPC.
     */
    public class LocalBinder extends Binder {
        CustomVpnService getService() {
            // Return this instance of LocalService so clients can call public methods
            return CustomVpnService.this;
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        if(!already_bind)
        {
            Bundle extras = intent.getExtras();
            messageHandler = (Messenger) extras.get("ConStat");
            already_bind = true;
        }
        return mBinder;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        if(!already_unbind) {
            already_show_connect = false;
            SendMessage(UNBIND_SIGNAL);
            already_unbind = true;
        }
        return true;
    }

    public void SetDisconnect(int signal) {
        if(interrupt)
            return;

        interrupt = true;
        SetDisconnectMessage(signal);
        disconnect();

        if(old_vpn_interface != null) {
            try {
                old_vpn_interface.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    public void SendWaitMessage() {
        mHandler.sendEmptyMessage(R.string.wait);
    }

    public void SendCanBeClosed() {
        if(service_can_be_closed)
                return;
        SendMessage(BIND_SIGNAL);
        service_can_be_closed = true;
    }

    private void SendMessage(int signal)
    {
        Message message = Message.obtain();
        message.arg1 = signal;
        try {
            messageHandler.send(message);
        } catch (RemoteException e) {
            Log.e(TAG, "Can't send from service to client", e);
        }
    }

    private static class Connection extends Pair<Thread, ParcelFileDescriptor> {
        public Connection(Thread thread, ParcelFileDescriptor pfd) {
            super(thread, pfd);
        }
    }

    private final AtomicReference<Thread> mConnectingThread = new AtomicReference<>();
    private final AtomicReference<Connection> mConnection = new AtomicReference<>();

    private AtomicInteger mNextConnectionId = new AtomicInteger(1);

    private PendingIntent mConfigureIntent;

    @Override
    public void onCreate() {
        // The handler is only used to show messages.
        if (mHandler == null) {
            mHandler = new Handler(this);
            try {
                connect(false);
            } catch (WolfSSLException e) {
                e.printStackTrace();
            }
        }

        // Create the intent to "configure" the connection (just start VpnClient).
        mConfigureIntent = PendingIntent.getActivity(this, 0, new Intent(this, VpnClient.class),
                PendingIntent.FLAG_UPDATE_CURRENT);
    }

    @Override
    public boolean handleMessage(Message message) {
        Toast.makeText(this, message.what, Toast.LENGTH_SHORT).show();
        if (message.what != R.string.disconnected) {
            updateForegroundNotification(message.what);
        }
        return true;
    }

    private void connect(boolean reconnect) throws WolfSSLException {
        // Become a foreground service. Background services can be VPN services too, but they can
        // be killed by background check before getting a chance to receive onRevoke().
        if(!reconnect) {
            updateForegroundNotification(R.string.connecting);
            mHandler.sendEmptyMessage(R.string.connecting);
        }

        // Extract information from the shared preferences.
        final SharedPreferences prefs = getSharedPreferences(VpnClient.Prefs.NAME, MODE_PRIVATE);
        final String server = prefs.getString(VpnClient.Prefs.SERVER_ADDRESS, "");
        final int port;
        try {
            port = Integer.parseInt(prefs.getString(VpnClient.Prefs.SERVER_PORT, ""));
        } catch (NumberFormatException e) {
            Log.e(TAG, "Bad port: " + prefs.getString(VpnClient.Prefs.SERVER_PORT, null), e);
            return;
        }

        // Kick off a connection.
        startConnection(new apriorit.vpnclient.VpnConnection(
                this, mNextConnectionId.getAndIncrement(), server, port, getApplicationContext()));
    }

    private void startConnection(final apriorit.vpnclient.VpnConnection connection) throws WolfSSLException {
        // Replace any existing connecting thread with the  new one.
        final Thread thread = new Thread(connection, "VpnConnectionThread");
        setConnectingThread(thread);

        // Handler to mark as connected once onEstablish is called.
        connection.setConfigureIntent(mConfigureIntent);
        connection.setOnEstablishListener(new apriorit.vpnclient.VpnConnection.OnEstablishListener() {
            public void onEstablish(ParcelFileDescriptor tunInterface) {
                mConnectingThread.compareAndSet(thread, null);
                setConnection(new Connection(thread, tunInterface));
                if(!already_show_connect) {
                    mHandler.sendEmptyMessage(R.string.connected);
                    already_show_connect = true;
                    SendMessage(SIGNAL_SUCCESS_CONNECT);
                }
                SendCanBeClosed();
            }
        });
        thread.start();
    }

    private void setConnectingThread(final Thread thread) {
        final Thread oldThread = mConnectingThread.getAndSet(thread);
        if (oldThread != null) {
            oldThread.interrupt();
        }
    }

    private void setConnection(final Connection connection) {
        final Connection oldConnection = mConnection.getAndSet(connection);
        if (oldConnection != null)
                oldConnection.first.interrupt();
    }

    private void SetDisconnectMessage(int signal) {
        mHandler.sendEmptyMessage(signal == SIGNAL_SUCCESS_DISCONNECT ?
                R.string.disconnected :
                R.string.can_not_connect);
        SendMessage(signal);
    }

    private void disconnect() {
        setConnectingThread(null);
        setConnection(null);
        stopForeground(true);
    }

    private void updateForegroundNotification(final int message) {
        // this could be changed to NotificationCompat.Builder in API >= 22
        startForeground(1, new Notification.Builder(this)
                .setSmallIcon(R.drawable.lock_locked)
                .setContentTitle("VPN Connection")
                .setContentText(getString(message))
                .setContentIntent(mConfigureIntent)
                .build());
    }
    public void SetDefaultRecCount() {
        reconnect_count = 0;
    }

    public void Reconnect() {
        if(interrupt) {
            reconnect_count = max_rec_count;
            return;
        }
        if(reconnect_count < max_rec_count) {
            reconnect_count++;
            Log.e(TAG, "Reconnect ");
            disconnect();
            try {
                connect(true);
            } catch (WolfSSLException e) {
                e.printStackTrace();
            }
        }
        else {
            try {
                if(old_vpn_interface!=null)
                    old_vpn_interface.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
            SetDisconnectMessage(SIGNAL_FAIL_CONNECT);

        }
    }
}