# VPN Client & Server

####This is a VPN application that provides creating virtual private network between client and server. The server must be located in safe part of network. If so, it is guaranteed that between client and server will be established safe connection via DTLS 1.2

# Server side

The server application is runned like daemon (service). It has to be installed on UNIX-specified operating systems, such like Ubuntu, Debian etc.

## Server side installation

1. Fetch dependent wolfssl and wolfssljni libraries (run from root repo dir).

   * $ git submodule init
   * $ git submodule update

2. Compile dependent wolfssl library. To do this you need to have autoconf, automake and libtool installed.

   * $ cd wolfssl/
   * $ ./autogen.sh
   * $ make
   * $ make check
   * $ sudo make install

3. Compile server:
  
   * $ cd VPN_Server/
   * $ make

## Server usage:
Server must be runned from superuser account. You can run server with this parameters:
./VPN_Server 8000 test -a 10.0.0.0 8 -d 8.8.8.8 -r 0.0.0.0 0 -i wlan0
Where:
1. 8000 - port to listen
2. test - password
3. -a X.X.X.X Y (by default used 10.0.0.0 8)
   * X.X.X.X - tunnels network address
   * Y - tunnels netmask.
4. -d X.X.X.X
   * X.X.X.X - DNS Server address (used GoogleDNS 8.8.8.8 by default)
5. -r X.X.X.X Y (by default used 0.0.0.0 0)
   * X.X.X.X - route IP
   * Y - route netmask
6. -i xxxx (by default used eth0)
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


