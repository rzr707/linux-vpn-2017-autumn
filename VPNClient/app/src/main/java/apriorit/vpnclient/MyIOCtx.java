package apriorit.vpnclient;

/**
 * Created by admin on 29.10.2017.
 */

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.net.DatagramSocket;
import java.net.InetAddress;

class MyIOCtx
{
    private DataOutputStream out;
    private DataInputStream in;
    private DatagramSocket dsock;
    private InetAddress hostAddress;
    private int port;

    /* if not using DTLS, sock and hostAddr may be null */
    public MyIOCtx(DataOutputStream outStr, DataInputStream inStr,
                   DatagramSocket s, InetAddress hostAddr, int port) {
        this.out = outStr;
        this.in = inStr;
        this.dsock = s;
        this.hostAddress = hostAddr;
        this.port = port;
    }

    public void test() {
        if (this.out == null) {
            System.out.println("out is NULL!");
            System.exit(1);
        }
        if (this.in == null) {
            System.out.println("in is NULL!");
            System.exit(1);
        }
    }

    public DataOutputStream getOutputStream() {
        return this.out;
    }

    public DataInputStream getInputStream() {
        return this.in;
    }

    public DatagramSocket getDatagramSocket() {
        return this.dsock;
    }

    public InetAddress getHostAddress() {
        return this.hostAddress;
    }

    public void setAddress(InetAddress addr) {
        this.hostAddress = addr;
    }

    public int getPort() {
        return this.port;
    }

    public void setPort(int port) {
        this.port = port;
    }

    public int isDTLS() {
        if (dsock != null)
            return 1;
        else
            return 0;
    }
}