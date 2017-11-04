package eu.freecluster.blog_vano.user.vpnclient;

/**
 * Created by ivan on 01.10.2017.
 */

import static java.nio.charset.StandardCharsets.US_ASCII;

import android.app.PendingIntent;
import android.content.Context;
import android.net.ConnectivityManager;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.SocketAddress;
import java.net.SocketException;
import java.nio.ByteBuffer;
import java.nio.channels.DatagramChannel;
import java.util.Arrays;
import java.util.concurrent.TimeUnit;
import java.io.DataInputStream;
import java.io.DataOutputStream;

import com.wolfssl.*;


public class VpnConnection implements Runnable {
    /**
     * Load wolfSSL shared JNI library:
     */
    static {
        System.loadLibrary("wolfssl");
        System.loadLibrary("wolfssljni");
    }

    WolfSSL        sslLib             = null;
    WolfSSLContext sslCtx             = null;
    WolfSSLSession ssl                = null;
    DataInputStream dtlsInputStream   = null;
    DataOutputStream dtlsOutputStream = null;

    /**
     * Callback interface to let the {@link CustomVpnService} know about new connections
     * and update the foreground notification with connection status
     */
    public interface OnEstablishListener {
        void onEstablish(ParcelFileDescriptor tunInterface);
    }

    /** Maximum packet size is constrained by the MTU, which is given as a signed short. */
    private static final int MAX_PACKET_SIZE = Short.MAX_VALUE / 2 - 1;

    /** Time to wait in between losing the connection and retrying. */
    private static final long RECONNECT_WAIT_MS = TimeUnit.SECONDS.toMillis(3);

    /** Time between keepalives if there is no traffic at the moment.
     **/
    private static final long KEEPALIVE_INTERVAL_MS = TimeUnit.SECONDS.toMillis(5);

    /** Time to wait without receiving any response before assuming the server is gone. */
    private static final long RECEIVE_TIMEOUT_MS = TimeUnit.SECONDS.toMillis(200);

    /**
     * Time between polling the VPN interface for new traffic, since it's non-blocking.
     */
    private static final long IDLE_INTERVAL_MS = TimeUnit.MILLISECONDS.toMillis(20);
    private static final int MAX_HANDSHAKE_ATTEMPTS = 50;

    private final android.net.VpnService mService;
    private final int mConnectionId;

    private final String mServerName;
    private final int mServerPort;
    private final byte[] mSharedSecret;

    private PendingIntent mConfigureIntent;
    private OnEstablishListener mOnEstablishListener;

    /**
     * The main class where secure connection with server will be established and packets
     * will be forwarded from android device and into it.
     *
     * @param service - an instance of android VPN Service
     * @param connectionId - connection ID
     * @param serverName   - server name
     * @param serverPort   - port to connect
     * @param sharedSecret - @todo: remove this, not used anymore
     * @param appContext   - application context, needed for loading android assets
     * @throws WolfSSLException - DTLS-specified exceptions
     */
    public VpnConnection(final android.net.VpnService service, final int connectionId,
                         final String serverName, final int serverPort, final byte[] sharedSecret,
                         Context appContext) throws WolfSSLException {

        /**
         * Load CA certificate from android assets:
         */
        String text = "ca_cert.pem";
        try {
            Log.i("ASSETS_LIST",
                    Arrays.toString(appContext.getAssets().list(""))
            );
        } catch (IOException e) {
            e.printStackTrace();
        }
        byte[] buffer = null;
        InputStream is;
        try {
            is = appContext.getAssets().open(text);
            int size = is.available();
            buffer = new byte[size];
            is.read(buffer);
            is.close();
        } catch (IOException e) {
            e.printStackTrace();
        }


        mService = service;
        mConnectionId = connectionId;

        mServerName = serverName;
        mServerPort= serverPort;
        mSharedSecret = sharedSecret;

        sslLib = new WolfSSL();
        sslLib.setLoggingCb(new MyLoggingCallback()); // @todo: remove this (useless)

        // Configure SSL Context for DTLS connections:
        sslCtx = new WolfSSLContext(WolfSSL.DTLSv1_2_ClientMethod());

        int status = 0;
        try {
            status = sslCtx.loadVerifyBuffer(buffer, buffer.length, WolfSSL.SSL_FILETYPE_PEM);
        } catch (WolfSSLJNIException e) {
            e.printStackTrace();
        }
        if(status != WolfSSL.SSL_SUCCESS) {
            Log.e("WOLFSSL_SSL_FAILURE", "Failed to load ca certificate");
            System.exit(1);
        }
    }

    /**
     * Optionally, set an intent to configure the VPN. This is {@code null} by default.
     */
    public void setConfigureIntent(PendingIntent intent) {
        mConfigureIntent = intent;
    }
    public void setOnEstablishListener(OnEstablishListener listener) {
        mOnEstablishListener = listener;
    }

    @Override
    public void run() {
        try {
            Log.i(getTag(), "Starting");
            final SocketAddress serverAddress = new InetSocketAddress(mServerName, mServerPort);
            // Try to create the tunnel for 1 time:
            if(run(serverAddress)) { // @TODO: Make check internet access through ConnectivityManager
                // Sleep for a while. This also checks if we got interrupted.
                Thread.sleep(3000);
            }
            Log.i(getTag(), "Giving up");
        } catch (IOException | InterruptedException | IllegalArgumentException e) {
            Log.e(getTag(), "Connection failed, exiting", e);
        }
    }

    private boolean run(SocketAddress server)
            throws IOException, InterruptedException, IllegalArgumentException {

        // Create SSL Session instance:
        try {
            ssl = new WolfSSLSession(sslCtx);
        } catch (WolfSSLException e) {
            e.printStackTrace();
        }

        ParcelFileDescriptor iface = null;
        boolean connected = false;
        // Create a DatagramSocket as the VPN tunnel.
        try  {
            DatagramSocket dgramSock = new DatagramSocket();
            // Protect the tunnel before connecting to avoid loopback.
            if (!mService.protect(dgramSock)) {
                throw new IllegalStateException("Cannot protect the tunnel");
            }

            int status;
            final InetAddress hostAddr = InetAddress.getByName(mServerName);
            final InetSocketAddress serverAddress = new InetSocketAddress(hostAddr, mServerPort);

            status = ssl.dtlsSetPeer(serverAddress);
            if (status != WolfSSL.SSL_SUCCESS) {
                Log.e("WOLFSSL_DTLS_SET_PEER",
                        "Failed to set DTLS peer");
                throw new RuntimeException("dtlsSetPeerException");
            }

            /* register callbacks: */
            ConnectRecvCallback connCallback = new ConnectRecvCallback();
            MyRecvCallback rcb = new MyRecvCallback();
            MySendCallback scb = new MySendCallback();
            MyIOCtx ioctx = new MyIOCtx(dtlsOutputStream, dtlsInputStream, dgramSock,
                    hostAddr, mServerPort);

            try {
                sslCtx.setIORecv(connCallback);
                sslCtx.setIOSend(scb);
                ssl.setIOReadCtx(ioctx);
                ssl.setIOWriteCtx(ioctx);
            } catch (WolfSSLJNIException e) {
                e.printStackTrace();
            }
            Log.i("IO_CALLBACKS_DTLS", "Registered I/O callbacks");

            /* call wolfSSL_connect */
            status = ssl.connect();
            if (status != WolfSSL.SSL_SUCCESS) {
                int err = ssl.getError(status);
                String errString = sslLib.getErrorString(err);
                Log.e("WOLFSSL_CONNECT", "Connect failed. Code: " + err +
                        ", description: " + errString);
                return false;
            }
            showPeer(ssl);

            // Now we are connected. Set the flag.
            connected = true;

            iface = handshake(ssl);
            FileInputStream  in = new FileInputStream(iface.getFileDescriptor());
            FileOutputStream out = new FileOutputStream(iface.getFileDescriptor());

            try {
                sslCtx.setIORecv(rcb);
            } catch (WolfSSLJNIException e) {
                e.printStackTrace();
            }

            // Allocate the buffer for a single packet.
            ByteBuffer packet = ByteBuffer.allocate(MAX_PACKET_SIZE);

            /* Here Runnable instance will be created.
             * It will listen in new thread for
             * incoming packets from outside (dtls)
             */
            DgramReaderRunnable r = new DgramReaderRunnable(dgramSock, ssl, out);
            Thread reciever = new Thread(r);
            reciever.start();

            dgramSock.setSoTimeout(0);

            // Here packets are forwarding from tunnel to secured socket:
            int ctrlPktCounter = 0;
            while (true) {
                    int len = in.read(packet.array());
                    if (len > 0) {
                        packet.limit(len);
                        len = ssl.write(packet.array(), len);
                        packet.clear();
                        Log.i("SSL_WRITE_SUCCESS", "Written " + len + " bytes of data.");
                    }
                    if(++ctrlPktCounter == 150) {
                        ctrlPktCounter = 0;
                        packet.put((byte) 0).limit(1);
                        for (int i = 0; i < 3; ++i) {
                            packet.position(0);
                            ssl.write(packet.array(), 1);
                            Log.i("CTRL_MSG_SENT", "Control message sent to server");
                        }
                        packet.clear();
                    }
                Thread.sleep(IDLE_INTERVAL_MS);
                Thread.yield();
            }
        } catch (SocketException e) {
            Log.e(getTag(), "Cannot use socket", e);
        } finally {
            if (iface != null) {
                try {
                    iface.close();
                    // ssl.freeSSL(); // @TODO: watch for memory leaks here
                } catch (IOException e) {
                    Log.e(getTag(), "Unable to close interface", e);
                }
            }
        }
        return connected;
    }

    /**
     * The handshake method is needed to
     * establish Point-to-Point tunnel connection with server.
     * Here the configuration data will be received from server, such as
     * client's tunnel IP address, packet MTU and routing rules.
     * @param ssl - current SSL (DTLS) session object
     * @return - configured ParcelFileDescriptor. It will be used like dev/tun file.
     * @throws IOException - thrown if after MAX_HANDSHAKE_ATTEMPTS we did not get parameters
     *                       from server;
     * @throws InterruptedException
     */
    private ParcelFileDescriptor handshake(WolfSSLSession ssl)
            throws IOException, InterruptedException {
        // To build a secured tunnel, we should perform mutual authentication
        // and exchange session keys for encryption.

        // Allocate the buffer for handshaking.
        ByteBuffer packet = ByteBuffer.allocate(1024);
        byte[] packetArr  = packet.array();

        // Control messages always start with zero.
        packet.put((byte) 0).put(mSharedSecret).flip();

        // Send the secret several times in case of packet loss.
        for (int i = 0; i < 3; ++i) {
            packet.position(0);
            ssl.write(packet.array(), 1024);
        }
        packet.clear();

        // Wait for the parameters within a limited time.
        for (int i = 0; i < MAX_HANDSHAKE_ATTEMPTS; ++i) {
            Thread.sleep(IDLE_INTERVAL_MS);

            // Normally we should not receive random packets. Check that the first
            // byte is 0 as expected.
            int length = ssl.read(packetArr, 1024);
            if (length > 0 && packet.get(0) == 0) {
                return configure(new String(packetArr, 1, length - 1, US_ASCII).trim());
            }
        }
        throw new IOException("Timed out");
    }

    /**
     * Method parses string for tunnel parameters.
     * @param parameters - {@link String} with parameters to parse;
     * @return - configured {@link ParcelFileDescriptor}
     * @throws IllegalArgumentException - thrown if error occured while parsing parameters.
     * @throws InterruptedException
     */
    private ParcelFileDescriptor configure(String parameters)
            throws IllegalArgumentException, InterruptedException {
        Log.i("VPN_CONNECTION_CONF", "Configure called.");
        // Configure a builder while parsing the parameters.
        android.net.VpnService.Builder builder = mService.new Builder();
        for (String parameter : parameters.split(" ")) {
            String[] fields = parameter.split(",");
            try {
                switch (fields[0].charAt(0)) {
                    case 'm':
                        builder.setMtu(Short.parseShort(fields[1]));
                        Log.i("MTU_SIZE", fields[1]);
                        break;
                    case 'a':
                        builder.addAddress(fields[1], Integer.parseInt(fields[2]));
                        break;
                    case 'r':
                        builder.addRoute(fields[1], Integer.parseInt(fields[2]));
                        break;
                    case 'd':
                        builder.addDnsServer(fields[1]);
                        break;
                    case 's':
                        builder.addSearchDomain(fields[1]);
                        break;
                }
            } catch (NumberFormatException e) {
                throw new IllegalArgumentException("Bad parameter: " + parameter);
            }
        }

        // Create a new interface using the builder and save the parameters.
        final ParcelFileDescriptor vpnInterface;
        synchronized (mService) {
            vpnInterface = builder
                    .setSession(mServerName)
                    .setConfigureIntent(mConfigureIntent)
                    .establish();
            if (mOnEstablishListener != null) {
                mOnEstablishListener.onEstablish(vpnInterface);
            }
        }

        Log.i(getTag(), "New interface: " + vpnInterface + " (" + parameters + ")");
        return vpnInterface;
    }

    private final String getTag() {
        return VpnConnection.class.getSimpleName() + "[" + mConnectionId + "]";
    }

    /**
     * The method shows peer cert information.
     * @param ssl - SSL Session instance
     */
    private void showPeer(WolfSSLSession ssl) {
        String altname;
        try {

            long peerCrtPtr = ssl.getPeerCertificate();

            if (peerCrtPtr != 0) {
                Log.i("SHOW_PEER","issuer : " +
                        ssl.getPeerX509Issuer(peerCrtPtr));
                Log.i("SHOW_PEER","subject : " +
                        ssl.getPeerX509Subject(peerCrtPtr));

                while( (altname = ssl.getPeerX509AltName(peerCrtPtr)) != null)
                    Log.i("SHOW_PEER","altname = " + altname);
            }

            Log.i("SHOW_PEER", "SSL version is " + ssl.getVersion());
            Log.i("SHOW_PEER", "SSL cipher suite is " + ssl.cipherGetName());

        } catch (WolfSSLJNIException e) {
            e.printStackTrace();
        }
    }

    /**
     * The {@link DgramReaderRunnable} class
     * Lisens for DTLS Socket income. Whet it gets some bytes, they will
     * be retranslated to tun file (iface).
     */
    class DgramReaderRunnable implements Runnable {
        DatagramSocket   ds;
        WolfSSLSession   ssl;
        FileOutputStream out;
        ByteBuffer       buf = ByteBuffer.allocate(MAX_PACKET_SIZE);

        DgramReaderRunnable(DatagramSocket sock, WolfSSLSession ssl, FileOutputStream out)
                throws SocketException {
            ds = sock;
            ds.setSoTimeout(0);
            this.ssl = ssl;
            this.out = out;
        }

        @Override
        public void run() {
            while(true) { // @todo: make thread to stop working after user-disconnect
                    int len = ssl.read(buf.array(), MAX_PACKET_SIZE);
                    if(len > 0) {
                        if(buf.get(0) != 0) {
                            try {
                                out.write(buf.array(), 0, len);
                            } catch (IOException e) {
                                e.printStackTrace();
                            }
                        } else {
                            Log.i("CONTROL_PKT", "Control zero packet received");
                        }
                        Log.i("SSL_READ_SUCCESS", "Read " + len + " bytes of data: ");
                        //Log.i("SSL_READ_SUCCESS", "Read " + len + " bytes of data: " +
                        //      new String(buf.array(), 0, len));
                        buf.clear();
                    }
                try {
                    Thread.sleep(IDLE_INTERVAL_MS);
                    Thread.yield();
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }

        }
    }
}