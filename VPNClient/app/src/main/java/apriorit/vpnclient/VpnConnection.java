package apriorit.vpnclient;

/**
 * Created by ivan on 01.10.2017.
 */

import static java.nio.charset.StandardCharsets.US_ASCII;

import android.app.PendingIntent;
import android.content.Context;
import android.os.ParcelFileDescriptor;
import android.os.PowerManager;
import android.util.Log;

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.PortUnreachableException;
import java.net.SocketAddress;
import java.net.SocketException;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.concurrent.TimeUnit;
import java.io.DataInputStream;
import java.io.DataOutputStream;

import com.wolfssl.*;

/**
 * @todo:
 * 1) Activate wakelock on connect to prevent enabling sleeping mode,
 *    When disconnected - deactivate it.
 * 2) On disconnect send special packet that signalize client manual disconnect
 * 3) Add receiving timeout. If reached - make force disconnect.
 */
public class VpnConnection implements Runnable {
    public enum SpecialPacket {
        ZERO_PACKET,
        WANT_CONNECT,
        WANT_DISCONNECT
    }

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

    /**
     * Time between polling the VPN interface for new traffic
     */
    private static final long IDLE_INTERVAL_MS = TimeUnit.MILLISECONDS.toMillis(4); // 20 by default
    private static final int MAX_HANDSHAKE_ATTEMPTS = 50;

    private final CustomVpnService mService;
    private final int mConnectionId;

    private final String mServerName;
    private final int mServerPort;

    private PendingIntent mConfigureIntent;
    private OnEstablishListener mOnEstablishListener;

    /**
     * This boolean controls recv-thread execution. If false, thread will be terminated.
     */
    private boolean connectedToServer = false;

    /** WakeLock object to prevent client from falling asleep when VPN is enabled */
    private PowerManager.WakeLock wakeLock = null;

    /**
     * The main class where secure connection with server will be established and packets
     * will be forwarded from android device and into it.
     *
     * @param service - an instance of android VPN Service
     * @param connectionId - connection ID
     * @param serverName   - server name
     * @param serverPort   - port to connect
     * @param appContext   - application context, needed for loading android assets
     * @throws WolfSSLException - DTLS-specified exceptions
     */
    public VpnConnection(final CustomVpnService service, final int connectionId,
                         final String serverName, final int serverPort,
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

        sslLib = new WolfSSL();

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

        PowerManager powerManager = (PowerManager) appContext.getSystemService(Context.POWER_SERVICE);
        wakeLock = powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "VpnWakeLock");
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
            final InetSocketAddress serverAddress
                    = new InetSocketAddress(InetAddress.getByName(mServerName), mServerPort);
            run(serverAddress);
        } catch (IOException | InterruptedException | IllegalArgumentException | NullPointerException e) {
            Log.e(getTag(), "Connection failed, exiting", e);
            try {
                Thread.sleep(1000);
            } catch (InterruptedException e1) {
                e1.printStackTrace();
            }
            mService.Reconnect();
        }
    }

    private boolean run(InetSocketAddress server)
            throws IOException, InterruptedException, IllegalArgumentException {
        // Create SSL Session instance:
        try {
            ssl = new WolfSSLSession(sslCtx);
        } catch (WolfSSLException e) {
            throw new IOException("Can't create WolfSSLSession");
        }

        ParcelFileDescriptor iface = null;
        connectedToServer = false;
        // Create a DatagramSocket as the VPN tunnel.
        try  {
            DatagramSocket dgramSock = new DatagramSocket();
            // Protect the tunnel before connecting to avoid loopback.
            if (!mService.protect(dgramSock)) {
                throw new IOException("Cannot protect the tunnel");
            }

            int status;

            status = ssl.dtlsSetPeer(server);
            if (status != WolfSSL.SSL_SUCCESS) {
                Log.e("WOLFSSL_DTLS_SET_PEER",
                        "Failed to set DTLS peer");
                throw new IOException("dtlsSetPeerException");
            }

            /* register callbacks: */
            ConnectRecvCallback connCallback = new ConnectRecvCallback();
            MyRecvCallback rcb = new MyRecvCallback();
            MySendCallback scb = new MySendCallback();
            MyIOCtx ioctx = new MyIOCtx(dtlsOutputStream, dtlsInputStream, dgramSock,
                    server.getAddress(), mServerPort);

            try {
                sslCtx.setIORecv(connCallback);
                sslCtx.setIOSend(scb);
                ssl.setIOReadCtx(ioctx);
                ssl.setIOWriteCtx(ioctx);
            } catch (WolfSSLJNIException e) {
                throw new IOException("Can't register callbacks in VPN Connection");
            }
            Log.i("IO_CALLBACKS_DTLS", "Registered I/O callbacks");

            wakeLock.acquire();
            ByteBuffer bb = ByteBuffer.allocate(1024);
            DatagramPacket dpacket = new DatagramPacket(bb.array(), 2);
            dgramSock.connect(server.getAddress(), mServerPort);

            // Send initial packets several times in case of packets loss to
            // init the DTLS connection with server:
            for(int i = 0; i < 4; ++i) {
                bb.put((byte)0).put((byte)SpecialPacket.WANT_CONNECT.ordinal()).flip();
                bb.position(0);
                dgramSock.send(dpacket);
                bb.clear();
                Thread.sleep(100);
            }

            /* call wolfSSL_connect */
            status = ssl.connect();
            Log.i("IO_CALLBACKS_DTLS", "Registered I/O callbacks end");
            if (status != WolfSSL.SSL_SUCCESS) {
                int err = ssl.getError(status);
                String errString = sslLib.getErrorString(err);
                Log.e("WOLFSSL_CONNECT", "Connect failed. Code: " + err +
                        ", description: " + errString);
                throw new IOException("Can't connect to server");
            }
            showPeer(ssl);

            connectedToServer = true;


            iface = handshake(ssl);
            mService.SetDefaultRecCount();
            if(mService.old_vpn_interface!=null){
                mService.old_vpn_interface.close();
            }
            mService.old_vpn_interface = iface;
            FileInputStream  in = new FileInputStream(iface.getFileDescriptor());
            FileOutputStream out = new FileOutputStream(iface.getFileDescriptor());

            try {
                sslCtx.setIORecv(rcb);
            } catch (WolfSSLJNIException e) {
                throw new IOException("Can't set sslCtx.setIORecv()");
            }

            // Allocate the buffer for a single packet.
            ByteBuffer packet = ByteBuffer.allocate(MAX_PACKET_SIZE);

            /* Here Runnable instance will be created.
             * It will be listening for incoming packets
             * from outside (dtls) in a new thread
             */

            //After all test can be uncommented
            DgramReaderRunnable r = new DgramReaderRunnable(dgramSock, ssl, out);
            Thread receiver = new Thread(r);
            receiver.start();

            // dgramSock.setSoTimeout(0);

            // Here packets are forwarding from tunnel to secured socket:
            int ctrlPktCounter = 0;
            // Calculate when control messages will be sent (after 'maxLimit' times of 'read')
            int maxLimit = (int) (3000 / IDLE_INTERVAL_MS);

            while (true) {
                int len = in.read(packet.array());
                if (len > 0) {
                    packet.limit(len);
                    len = ssl.write(packet.array(), len);
                    packet.clear();
                    if(len==-1)
                        throw new IOException("Can't write to the tunnel!");
                    Log.i("SSL_WRITE_SUCCESS", "Written " + len + " bytes of data.");
                }
                if(++ctrlPktCounter > maxLimit) {
                    ctrlPktCounter = 0;
                    packet.put((byte) 0).limit(1);
                    for (int i = 0; i < 3; ++i) {
                        packet.position(0);
                        ssl.write(packet.array(), 1);
                        if(len==-1)
                            throw new IOException("Can't write to the tunnel!");
                        Log.i("CTRL_MSG_SENT", "Control message sent to server");
                    }
                    packet.clear();
                }
                Thread.sleep(IDLE_INTERVAL_MS);
            }
        } catch (PortUnreachableException e) {
            e.printStackTrace();
            Log.e(getTag(), "Lost connection with server");

        } catch (SocketException e) {
            Log.e(getTag(), "Cannot use socket", e);
        } catch (InterruptedException e) {
            ByteBuffer packet = ByteBuffer.allocate(1024);
            for(int i = 0; i < 4; ++i) {
                packet.clear();
                packet.position(0);
                packet.put((byte)0).put((byte)SpecialPacket.WANT_DISCONNECT.ordinal()).flip();
                ssl.write(packet.array(), 2);
                packet.clear();
            }
            if(mService.old_vpn_interface!=null) {
                try {
                    mService.old_vpn_interface.close();
                } catch (IOException e3) {
                    Log.d(getTag(), "Unable to close interface", e3);
                }
            }

            if (iface != null) {
                try {
                    iface.close();
                } catch (IOException e2) {
                    Log.d(getTag(), "Unable to close interface", e2);
                }
            }

        } finally {

            connectedToServer = false;

            // free wakeLock to prevent battery draining:
            wakeLock.release();
        }
        return connectedToServer;
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
     */
    private ParcelFileDescriptor handshake(WolfSSLSession ssl)
            throws IOException {
        // To build a secured tunnel, we should perform mutual authentication
        // and exchange session keys for encryption.

        // Allocate the buffer for handshaking.
        ByteBuffer packet = ByteBuffer.allocate(1024);
        byte[] packetArr  = packet.array();

        // Control messages always start with zero.
        packet.put((byte) 0).flip();

        // Send the 'init-connection' packet several times in case of packet loss.
        for (int i = 0; i < 4; ++i) {
            packet.position(0);
            ssl.write(packet.array(), 1024);
        }
        packet.clear();

        // Wait for the parameters within a limited time.
        for (int i = 0; i < MAX_HANDSHAKE_ATTEMPTS; ++i) {
            try {
                Thread.sleep(IDLE_INTERVAL_MS);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }

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
     */
    private ParcelFileDescriptor configure(String parameters)
            throws IllegalArgumentException {
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
            while(connectedToServer) {
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
                    buf.clear();
                }
                try {
                    Thread.sleep(IDLE_INTERVAL_MS);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }

        }
    }
}