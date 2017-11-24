# VPN Client & Server

This is an VPN application that provides creating virtual private network between client and server. The server must be located in safe part of network. If so, it is guaranteed that between client and server will be established safe connection via DTLS 1.2

# Server side

The server application is runned like daemon (service). It has to be installed on UNIX-specified operating systems, such like Ubuntu, Debian etc. You can install precompiled VPN server (package is stored at VPN_Server/debpackage/ directory) and run it like a daemon or compile it by yourself and run it from terminal manager such like 'tmux'.

## .deb package installation

To install precompiled debian package you need to execute this command in terminal (with root privileges in directory with .deb package):

  * apt-get install -y systemd && dpkg -i apriorit-vpnservice_1.0-1_amd64.deb

After this VPN server will be runned like a daemon. To stop daemon use this command:

  * systemctl stop apriorit-vpnservice.service

If you need to reconfigure server run parameters, edit file "/etc/systemd/system/apriorit-vpnservice.service" . The parameters and default values are listed in this readme below. After edit you have to restart service or reboot the system.

## Server side building

1. Fetch dependent wolfssl and wolfssljni libraries (run from root repo dir).

   * $ git submodule init
   * $ git submodule update

2. Compile dependent wolfssl library. To do this you need to have autoconf, automake and libtool installed.

   * $ cd wolfssl/
   * $ ./autogen.sh
   * $ ./configure --enable-dtls
   * $ make
   * $ make check
   * $ sudo make install
   * $ mv /usr/local/lib/libwolfssl.la /usr/lib/libwolfssl.la 
   * $ mv /usr/local/lib/libwolfssl.so /usr/lib/libwolfssl.so
   * $ mv /usr/local/lib/libwolfssl.so.14 /usr/lib/libwolfssl.so.14
   * $ mv /usr/local/lib/libwolfssl.so.14.0.0 /usr/lib/libwolfssl.so.14.0.0

3. Compile server:
  
   * $ cd VPN_Server/
   * $ make
   * or:
   * $ g++ -std=c++11 main.cpp -lwolfssl -o ../VPN_Server

4. (Optional) You can generate your own certificates and keys. Use openssl for this. When generated, put your new files to VPN_Server/certs/ directory, replacing the old ones. Also you need replace ca_cert.pem in client application the path is VPNClient/app/src/main/assets/

## Server usage:
Server must be runned from superuser account. You can run server with this parameters:
./VPN_Server 8000 test -a 10.0.0.0 8 -d 8.8.8.8 -r 0.0.0.0 0 -i wlan0
Where:
1. 8000 - port to listen
2. -a X.X.X.X Y (by default used 10.0.0.0 8)
   * X.X.X.X - tunnels network address
   * Y - tunnels netmask.
3. -d X.X.X.X
   * X.X.X.X - DNS Server address (used GoogleDNS 8.8.8.8 by default)
4. -r X.X.X.X Y (by default used 0.0.0.0 0)
   * X.X.X.X - route IP
   * Y - route netmask
5. -i xxxx (by default used eth0)
   * xxxx - physical network adapter to use (ethX, wlan1 etc.)

# Android Client

## Client building (installing from IDE Android Studio)

1. First of all you need to have NDK, CMake and LLDB installed in Android studio. To do that, go to "Tools->Android->SDK Manager", choose SDK Tools tab and install these tools.
2. Go to wolfssl/wolfssl/ and make copy of options.h.in and rename it to options.h
3. In Android studio: Build->Refresh Lincked C++ projects
4. In Android studio: Build->Clean Project
5. In Android studio: Build->Rebuild Project
6. Or use gradlew.bat or gradlew file to build project without Android Studio IDE (in VPNClient/ directory)
7. Use "Build->Generate signed APK" to generate Android .apk file.
8. Install APK and run it.

## Client usage:

1. Choose VPN server to connect from list;
2. Tap lock button to connect to the vpn server or disconnect from it. 
3. You will be informed about connection status via android notification and toasts.


