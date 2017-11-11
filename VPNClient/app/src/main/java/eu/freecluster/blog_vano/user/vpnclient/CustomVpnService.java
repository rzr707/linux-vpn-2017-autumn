package eu.freecluster.blog_vano.user.vpnclient;

/**
 * Created by ivan on 01.10.2017.
 */

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.Message;
import android.os.ParcelFileDescriptor;
import android.util.Log;
import android.util.Pair;
import android.widget.Toast;

import com.wolfssl.WolfSSLException;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

public class CustomVpnService /* renamed from 'VpnService' */ extends android.net.VpnService implements Handler.Callback {
    private static final String TAG = CustomVpnService.class.getSimpleName();

    public static final String ACTION_CONNECT = "eu.freecluster.blog_vano.user.vpnclient.START";
    public static final String ACTION_DISCONNECT = "eu.freecluster.blog_vano.user.vpnclient.STOP";

    private Handler mHandler;

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
        }

        // Create the intent to "configure" the connection (just start VpnClient).
        mConfigureIntent = PendingIntent.getActivity(this, 0, new Intent(this, VpnClient.class),
                PendingIntent.FLAG_UPDATE_CURRENT);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && ACTION_DISCONNECT.equals(intent.getAction())) {
            disconnect();
            return START_NOT_STICKY;
        } else {
            try {
                connect();
            } catch (WolfSSLException e) {
                e.printStackTrace();
            }
            return START_STICKY;
        }
    }

    @Override
    public void onDestroy() {
        disconnect();
    }

    @Override
    public boolean handleMessage(Message message) {
        Toast.makeText(this, message.what, Toast.LENGTH_SHORT).show();
        if (message.what != R.string.disconnected) {
            updateForegroundNotification(message.what);
        }
        return true;
    }

    private void connect() throws WolfSSLException {
        // Become a foreground service. Background services can be VPN services too, but they can
        // be killed by background check before getting a chance to receive onRevoke().
        updateForegroundNotification(R.string.connecting);
        mHandler.sendEmptyMessage(R.string.connecting);

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
        startConnection(new eu.freecluster.blog_vano.user.vpnclient.VpnConnection(
                this, mNextConnectionId.getAndIncrement(), server, port, getApplicationContext()));
    }

    private void startConnection(final eu.freecluster.blog_vano.user.vpnclient.VpnConnection connection) throws WolfSSLException {
        // Replace any existing connecting thread with the  new one.
        final Thread thread = new Thread(connection, "VpnConnectionThread");
        setConnectingThread(thread);

        // Handler to mark as connected once onEstablish is called.
        connection.setConfigureIntent(mConfigureIntent);
        connection.setOnEstablishListener(new eu.freecluster.blog_vano.user.vpnclient.VpnConnection.OnEstablishListener() {
            public void onEstablish(ParcelFileDescriptor tunInterface) {
                mHandler.sendEmptyMessage(R.string.connected);

                mConnectingThread.compareAndSet(thread, null);
                setConnection(new Connection(thread, tunInterface));
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
        if (oldConnection != null) {
            try {
                oldConnection.first.interrupt();
                oldConnection.second.close();
            } catch (IOException e) {
                Log.e(TAG, "Closing VPN interface", e);
            }
        }
    }

    private void disconnect() {
        mHandler.sendEmptyMessage(R.string.disconnected);
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
}