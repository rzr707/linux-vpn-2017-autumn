/** \mainpage Virtual Private Server
 *
 * \section intro_sec Introduction
 *
 * This is documentation for VPN Server created as a task for
 * Apriorit company courses.
 *
 * \section install_sec Installation
 *
 * The application needs to be runned as superuser or by 'sudo'
 * because of network interfaces manupulating.\r\n
 * Here is example how to run the application from Sysem Terminal:\r\n
 * <pre>root @ ubuntu_server# ./VPN_Server 8000 test -m 1400 -a 10.0.0.0 8 -d 8.8.8.8 -r 0.0.0.0 0 -i wlan0</h2>
 *  where: argv[i]<br><pre>
 * [1]      8000        - port to listen (mandatory)
 * [2]      test        - secret phrase  (mandatory)
 * [3, 4]   -m 1400     - packet mtu     (optional, default = 1400)
 * [5, 6]   -a 10.0.0.0 - virtual network address (optional, default = 10.0.0.0)
 * [7]      8           - virtual network mask    (optional, default = 8)
 * [8, 9]   -d 8.8.8.8  - DNS-server address      (optional, default = 8.8.8.8)
 * [10, 11] -r 0.0.0.0  - routing address         (optional, default = 0.0.0.0)
 * [12]     0           - routing address mask    (optional, default = 0)
 * [13, 14] -i wlan0    - physical network interface (opt., default = eth0)<br></pre>
 * <h3>Enter 'exitvpn' in terminal to close the server.<h3><br>
 */

#include "vpn_server.hpp"
#include <iostream>

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cout <<
        "* How to:\n"
        "* " << argv[0] << " 8000 -m 1400 -a 10.0.0.0 8 -d 8.8.8.8 -r 0.0.0.0 0 -i wlan0\n"
        "*  where: argv[i]\n"
        "* [1]      8000        - port to listen (mandatory)\n"
        "* [2, 3]   -m 1400     - packet mtu     (optional, default = 1400)\n"
        "* [4, 5]   -a 10.0.0.0 - virtual network address (optional, default = 10.0.0.0)\n"
        "* [6]      8           - virtual network mask    (optional, default = 8)\n"
        "* [7, 8]   -d 8.8.8.8  - DNS-server address      (optional, default = 8.8.8.8)\n"
        "* [9, 10]  -r 0.0.0.0  - routing address         (optional, default = 0.0.0.0)\n"
        "* [11]     0           - routing address mask    (optional, default = 0)\n"
        "* [12, 13] -i wlan0    - physical network interface (opt., default = eth0)\n*\n";
        return EXIT_FAILURE;
    }

    try {
        VPNServer server(argc, argv);
        server.initServer();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return EXIT_SUCCESS;
}
