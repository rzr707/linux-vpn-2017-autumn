#ifndef VPN_SERVER_HPP
#define VPN_SERVER_HPP

#include "client_parameters.hpp"
#include "tunnel_mgr.hpp"

#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <linux/if_tun.h>

#include <ifaddrs.h>

/**
 * @brief VPNServer class\r\n
 * How to run:\r\n
 * ./VPN_Server 8000 test -m 1400 -a 10.0.0.0 8 -d 8.8.8.8 -r 0.0.0.0 0 -i wlan0\r\n
 *  where: argv[i]\r\n
 * [1]      8000        - port to listen (mandatory)\r\n
 * [2]      test        - secret phrase  (mandatory)\r\n
 * [3, 4]   -m 1400     - packet mtu     (optional, default = 1400)\r\n
 * [5, 6]   -a 10.0.0.0 - virtual network address (optional, default = 10.0.0.0)\r\n
 * [7]      8           - virtual network mask    (optional, default = 8)\r\n
 * [8, 9]   -d 8.8.8.8  - DNS-server address      (optional, default = 8.8.8.8)\r\n
 * [10, 11] -r 0.0.0.0  - routing address         (optional, default = 0.0.0.0)\r\n
 * [12]     0           - routing address mask    (optional, default = 0)\r\n
 * [13, 14] -i wlan0    - physical network interface (opt., default = eth0)\r\n
 */
class VPNServer {
private:
    int                  argc;
    char**               argv;
    unsigned int         tunnelCounter;
    ClientParameters     cliParams;
    IPManager*           manager;
    std::string          port;
    TunnelManager*       tunMgr;
    std::recursive_mutex mutex;

public:
    explicit VPNServer
    (int argc, char** argv) {
        this->argc = argc;
        this->argv = argv;
        tunnelCounter = 0;
        parseArguments(argc, argv); // fill 'cliParams struct'

        manager = new IPManager(cliParams.virtualNetworkIp + '/' + cliParams.networkMask);
        tunMgr  = new TunnelManager("data/pid_list.dat",
                                    "data/tun_list.dat");

        // Enable IP forwarding
        tunMgr->execTerminalCommand("echo 1 > /proc/sys/net/ipv4/ip_forward");

        /* In case if program was terminated by error: */
        tunMgr->killAllProcesses(tunMgr->getInfoList(tunMgr->getPidListFilename()));
        tunMgr->closeAllTunnels(tunMgr->getInfoList(tunMgr->getTunListFilename()));

        // Pick a range of private addresses and perform NAT over chosen network interface.
        std::string virtualLanAddress = cliParams.virtualNetworkIp + '/' + cliParams.networkMask;
        std::string physInterfaceName = cliParams.physInterface;
        std::string postrouting
                = "iptables -t nat -A POSTROUTING -s " + virtualLanAddress +
                  " -o " + physInterfaceName + " -j MASQUERADE";
        tunMgr->execTerminalCommand(postrouting);

    }

    ~VPNServer() {
        // Disable IP Forwarding:
        tunMgr->execTerminalCommand("echo 0 > /proc/sys/net/ipv4/ip_forward");
        /* Clear unix settings: removing tunnels and closing child processes */
        tunMgr->closeAllTunnels(tunMgr->getInfoList(tunMgr->getTunListFilename()));
        tunMgr->killAllProcesses(tunMgr->getInfoList(tunMgr->getPidListFilename()));

        // remove NAT rule from iptables:
        std::string virtualLanAddress = cliParams.virtualNetworkIp + '/' + cliParams.networkMask;
        std::string physInterfaceName = cliParams.physInterface;
        std::string postrouting
                = "iptables -t nat -D POSTROUTING -s " + virtualLanAddress +
                  " -o " + physInterfaceName + " -j MASQUERADE";
        tunMgr->execTerminalCommand(postrouting);
        //tunMgr->execTerminalCommand("ifdown " + physInterfaceName + " && ifup " + physInterfaceName);
        //tunMgr->execTerminalCommand("service networking restart");
        delete manager;
        delete tunMgr;
    }

    /**
     * @brief initConsoleInput\r\n
     * Main method, waiting for user keyboard input.\r\n
     * If input string is 'exitpvn' - close all tunnels,\r\n
     * revert system settings, exit the application.\r\n
     */
    void initConsoleInput() {
        bool isExit = false;
        std::string input;
        std::thread t(&VPNServer::initServer, this);
        t.detach();

        std::cout << "Type 'exitvpn' in terminal to close VPN Server"
                  << std::endl;
        while(!isExit) {
            getline(std::cin, input);
            if(input == "exitvpn") {
                isExit = true;
            }
        }
        TunnelManager::log("Closing the VPN Server...");
    }

    /**
     * @brief initServer\r\n
     * Нужно форкать внутри initServer и\r\n
     * вызывать ТУТ build_parameters для каждого нового клиента,\r\n
     * ведь в build_parameters заполняется статический массив,\r\n
     * в котором хранится ip-адрес, назначенный клиенту\r\n
     */
    void initServer() {
        mutex.lock();
        // run commands via unix terminal (needs sudo)
        std::string serverIp = IPManager::getIpString(manager->nextIp4Address());
        std::string clientIp = IPManager::getIpString(manager->nextIp4Address());
        std::string tunStr   = "tun" + to_string(tunnelCounter);

        //TunnelManager::log("Working with tunnel [" + tunStr + "] now..");
        initUnixSettings(serverIp, clientIp);
        // Get TUN interface.
        int interface = get_interface(tunStr.c_str());

        // fill array with parameters to send:
        buildParameters(clientIp);
        mutex.unlock();

        // wait for a tunnel.
        int tunnel;
        while ((tunnel = get_tunnel(port.c_str(), cliParams.secretPassword.c_str())) != -1) {

            TunnelManager::log("New client connected to [" + tunStr + "]");

            /* execute this method again in another thread: */
            std::thread thr(&VPNServer::initServer, this);
            thr.detach();

            std::string tempTunStr = tunStr;

            // put the tunnel into non-blocking mode.
            fcntl(tunnel, F_SETFL, O_NONBLOCK);

            // send the parameters several times in case of packet loss.
            for (int i = 0; i < 3; ++i) {
                send(tunnel,
                     cliParams.parametersToSend,
                     sizeof(cliParams.parametersToSend),
                     MSG_NOSIGNAL);
            }

            // allocate the buffer for a single packet.
            char packet[32767];

            int timer = 0;

            // we keep forwarding packets till something goes wrong.
            while (true) {
                // assume that we did not make any progress in this iteration.
                bool idle = true;

                // read the outgoing packet from the input stream.
                int length = read(interface, packet, sizeof(packet));
                if (length > 0) {
                    // write the outgoing packet to the tunnel.
                    send(tunnel, packet, length, MSG_NOSIGNAL);

                    // there might be more outgoing packets.
                    idle = false;

                    // if we were receiving, switch to sending.
                    if (timer < 1) {
                        timer = 1;
                    }
                }

                // read the incoming packet from the tunnel.
                length = recv(tunnel, packet, sizeof(packet), 0);
                if (length == 0) {
                    TunnelManager::log(std::string() +
                                       "recv() length == " +
                                       to_string(length) +
                                       ". Breaking..",
                                       std::cerr);
                    break;
                }
                if (length > 0) {
                    // ignore control messages, which start with zero.
                    if (packet[0] != 0) {
                        // write the incoming packet to the output stream.
                        write(interface, packet, length);
                    }

                    // there might be more incoming packets.
                    idle = false;

                    // if we were sending, switch to receiving.
                    if (timer > 0) {
                        timer = 0;
                    }
                }

                // if we are idle or waiting for the network, sleep for a
                // fraction of time to avoid busy looping.
                if (idle) {
                    usleep(100000); // sleep for 10 seconds

                    // increase the timer. This is inaccurate but good enough,
                    // since everything is operated in non-blocking mode.
                    timer += (timer > 0) ? 100 : -100;

                    // we are receiving for a long time but not sending.
                    // can you figure out why we use a different value? :)
                    if (timer < -16000) {
                        // send empty control messages.
                        packet[0] = 0;
                        for (int i = 0; i < 3; ++i) {
                            send(tunnel, packet, 1, MSG_NOSIGNAL);
                        }

                        // switch to sending.
                        timer = 1;
                    }

                    // we are sending for a long time but not receiving.
                    if (timer > 20000) {
                        TunnelManager::log("[" + tempTunStr + "]" +
                                           "Sending for a long time but"
                                           " not receiving. Breaking...");
                        break;
                    }
                }
            }
            TunnelManager::log("Client has been disconnected from tunnel [" +
                               tempTunStr + "]");
            close(tunnel);
            /* @TODO: Close tunnel here via execTeminal*/
            return;
        }
        TunnelManager::log("Cannot create tunnels", std::cerr);
        exit(1);
    }

    /**
     * @brief parseArguments method
     * is parsing arguments from terminal
     * and fills ClientParameters structure
     * @param argc - arguments count
     * @param argv - arguments vector
     */
    void parseArguments(int argc, char** argv) {
        if(argc < 3) {
            TunnelManager::log("Arguments list is too small!",
                               std::cerr);
            exit(1);
        }

        const unsigned int EQUALS = 0;
        port = argv[1]; // port to listen

        if(atoi(port.c_str()) < 1 || atoi(port.c_str()) > 0xFFFF) {
            TunnelManager::log("Error: invalid number of port (" +
                               port +
                               "). Terminating . . .",
                               std::cerr);
            exit(1);
        }

        cliParams.secretPassword  = argv[2];

        for(int i = 4; i < argc; ++i) {
            if(strcmp("-m", argv[i]) == EQUALS) {
                cliParams.mtu = argv[i + 1];
            }
            if(strcmp("-a", argv[i]) == EQUALS) {
                cliParams.virtualNetworkIp = argv[i + 1];
                cliParams.networkMask = argv[i + 2];
            }
            if(strcmp("-d", argv[i]) == EQUALS) {
                cliParams.dnsIp = argv[i + 1];
            }
            if(strcmp("-r", argv[i]) == EQUALS) {
                cliParams.routeIp = argv[i + 1];
                cliParams.routeMask = argv[i + 2];
            }
            if(strcmp("-i", argv[i]) == EQUALS) {
                cliParams.physInterface = argv[i + 1];
            }
            // just for testing:
            if(strcmp("-tn", argv[i]) == EQUALS) {
                tunnelCounter = atoi(argv[i + 1]);
            }
        }

        /* if there was no specific arguments,
         *  default settings will be set up
         */
        if(cliParams.mtu.empty()) {
            cliParams.mtu = "1400";
        }
        if(cliParams.virtualNetworkIp.empty()) {
            cliParams.virtualNetworkIp = "10.0.0.0";
        }
        if(cliParams.networkMask.empty()) {
            cliParams.networkMask = "8";
        }
        if(cliParams.dnsIp.empty()) {
            cliParams.dnsIp = "8.8.8.8";
        }
        if(cliParams.routeIp.empty()) {
            cliParams.routeIp = "0.0.0.0";
        }
        if(cliParams.routeMask.empty()) {
            cliParams.routeMask = "0";
        }
        if(cliParams.physInterface.empty()) {
            cliParams.physInterface = "eth0";
        }
    }

    /**
     * @brief buildParameters fills
     * 'parametersToSend' array with info
     *  which will be sent to client.
     * @param clientIp - IP-address string to send to client
     */
    void buildParameters(const std::string& clientIp) {

        int size     = sizeof(cliParams.parametersToSend);
        // Here is parameters string formed:
        std::string paramStr = std::string() + "m," + cliParams.mtu +
                " a," + clientIp + ",32 d," + cliParams.dnsIp +
                " r," + cliParams.routeIp + "," + cliParams.routeMask;

        // fill parameters array:
        cliParams.parametersToSend[0] = 0; // control messages always start with zero
        memcpy(&cliParams.parametersToSend[1], paramStr.c_str(), paramStr.length());
        memset(&cliParams.parametersToSend[paramStr.length() + 1], ' ', size - (paramStr.length() + 1));
    }

    /**
     * @brief get_interface
     * Tries to open dev/net/tun interface
     * @param name - tunnel interface name (e.g. "tun0")
     * @return descriptor of interface
     */
    int get_interface(const char *name) {
        int interface = open("/dev/net/tun", O_RDWR | O_NONBLOCK);

        ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
        strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

        if (int status = ioctl(interface, TUNSETIFF, &ifr)) {
            TunnelManager::log("Cannot get TUN interface\nStatus is: " +
                               status,
                               std::cerr);
            exit(1);
        }

        return interface;
    }

    int get_tunnel(const char *port, const char *secret) {
        // we use an IPv6 socket to cover both IPv4 and IPv6.
        int tunnel = socket(AF_INET6, SOCK_DGRAM, 0);
        int flag = 1;
        setsockopt(tunnel, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
        flag = 0;
        setsockopt(tunnel, IPPROTO_IPV6, IPV6_V6ONLY, &flag, sizeof(flag));

        // accept packets received on any local address.
        sockaddr_in6 addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(atoi(port));

        // call bind(2) in a loop since Linux does not have SO_REUSEPORT.
        while (bind(tunnel, (sockaddr *)&addr, sizeof(addr))) {
            if (errno != EADDRINUSE) {
                return -1;
            }
            usleep(100000);
        }

        // receive packets till the secret matches.
        char packet[1024];
        socklen_t addrlen;
        do {
            addrlen = sizeof(addr);
            int n = recvfrom(tunnel, packet, sizeof(packet), 0,
                    (sockaddr *)&addr, &addrlen);
            if (n <= 0) {
                return -1;
            }
            packet[n] = 0;
        } while (packet[0] != 0 || strcmp(secret, &packet[1]));

        // connect to the client
        connect(tunnel, (sockaddr *)&addr, addrlen);
        return tunnel;
    }

    /**
     * @brief initUnixSettings - uplink new p2p tunnel
     *                           (server must be running with root permissions)
     * @param serverTunAddr    - server tunnel ip
     * @param clientTunAddr    - client tunnel ip
     */
    void initUnixSettings
    (const std::string& serverTunAddr,
     const std::string& clientTunAddr) {

        std::string tunName = "tun" + std::to_string(tunnelCounter);
        std::string tunInterfaceSetup = "ip tuntap add dev " + tunName +  " mode tun";
        tunMgr->execTerminalCommand(tunInterfaceSetup);

        string ifconfig = "ifconfig " + tunName + " " + serverTunAddr +
                          " dstaddr " + clientTunAddr + " up";
        tunMgr->execTerminalCommand(ifconfig);
        // write tunnel to created tunnels list:
        tunMgr->write(tunMgr->getTunListFilename(), tunName);
        // write process pid to created processes list:
        tunMgr->write(tunMgr->getPidListFilename(), to_string(getpid()));

        ++tunnelCounter; // increment tunnel number for next connection
    }

};

#endif // VPN_SERVER_HPP
