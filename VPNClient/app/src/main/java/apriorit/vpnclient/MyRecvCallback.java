package apriorit.vpnclient;

import java.io.*;
import java.net.*;

import com.wolfssl.*;

class MyRecvCallback implements WolfSSLIORecvCallback
{
    public int receiveCallback(WolfSSLSession ssl, byte[] buf, int sz,
                               Object ctx) {

        MyIOCtx ioctx = (MyIOCtx) ctx;
        int doDTLS = ioctx.isDTLS();

        if (doDTLS == 1) {

            int dtlsTimeout;
            DatagramSocket dsock;
            DatagramPacket recvPacket;

            try {
                dtlsTimeout = ssl.dtlsGetCurrentTimeout() * 1000;
                dsock = ioctx.getDatagramSocket();
                dsock.setSoTimeout(0);
                recvPacket = new DatagramPacket(buf, sz);

                dsock.receive(recvPacket);

                ioctx.setAddress(recvPacket.getAddress());
                ioctx.setPort(recvPacket.getPort());

            } catch (SocketTimeoutException ste) {
                return WolfSSL.WOLFSSL_CBIO_ERR_TIMEOUT;
            } catch (SocketException se) {
                se.printStackTrace();
                return WolfSSL.WOLFSSL_CBIO_ERR_GENERAL;
            } catch (IOException ioe) {
                ioe.printStackTrace();
                return WolfSSL.WOLFSSL_CBIO_ERR_GENERAL;
            } catch (Exception e) {
                e.printStackTrace();
                return WolfSSL.WOLFSSL_CBIO_ERR_GENERAL;
            }

            return recvPacket.getLength();

        } else {
            DataInputStream is = ioctx.getInputStream();
            if (is == null) {
                System.out.println("DataInputStream is null in recvCallback!");
                System.exit(1);
            }

            try {
                is.read(buf, 0, sz);
            } catch (IOException e) {
                e.printStackTrace();
                return WolfSSL.WOLFSSL_CBIO_ERR_GENERAL;
            }
        }

        return sz;
    }
}
