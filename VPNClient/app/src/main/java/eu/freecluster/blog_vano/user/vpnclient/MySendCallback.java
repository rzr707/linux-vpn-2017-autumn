package eu.freecluster.blog_vano.user.vpnclient;

import java.io.*;
import java.net.*;
import java.nio.*;
import com.wolfssl.*;

class MySendCallback implements WolfSSLIOSendCallback
{
    public int sendCallback(WolfSSLSession ssl, byte[] buf, int sz,
                            Object ctx) {

        MyIOCtx ioctx = (MyIOCtx) ctx;
        int doDTLS = ioctx.isDTLS();

        if (doDTLS == 1) {

            DatagramSocket dsock = ioctx.getDatagramSocket();
            InetAddress hostAddr = ioctx.getHostAddress();
            int port = ioctx.getPort();
            DatagramPacket dp = new DatagramPacket(buf, sz, hostAddr, port);

            try {
                dsock.send(dp);

            } catch (IOException ioe) {
                ioe.printStackTrace();
                return WolfSSL.WOLFSSL_CBIO_ERR_GENERAL;
            } catch (Exception e) {
                e.printStackTrace();
                return WolfSSL.WOLFSSL_CBIO_ERR_GENERAL;
            }

            return dp.getLength();
        } else {
            DataOutputStream os = ioctx.getOutputStream();
            if (os == null) {
                System.out.println("DataOutputStream is null in sendCallback!");
                System.exit(1);
            }

            try {
                os.write(buf, 0, sz);
            } catch (IOException e) {
                e.printStackTrace();
                return WolfSSL.WOLFSSL_CBIO_ERR_GENERAL;
            }
        }

        return sz;
    }
}