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
#include <sys/file.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <linux/if_tun.h>

#include <ifaddrs.h>
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <sys/time.h>

/**
 * @brief The VPNServer class<br>
 * Main application class that<br>
 * organizes a process of creating, removing and processing<br>
 * vpn tunnels, provides encrypting/decrypting of packets.<br>
 * To run the server loop call 'initConsolInput' method.<br>
 */
class VPNServer {
private:
    int                  argc;
    char**               argv;
    ClientParameters     cliParams;
    IPManager*           manager;
    std::string          port;
    TunnelManager*       tunMgr;
    std::recursive_mutex mutex;
    const int            TIMEOUT_LIMIT = 60000;
    WOLFSSL_CTX*         ctx;

public:
    explicit VPNServer
    (int argc, char** argv) {
        this->argc = argc;
        this->argv = argv;
        parseArguments(argc, argv); // fill 'cliParams struct'

        manager = new IPManager(cliParams.virtualNetworkIp + '/' + cliParams.networkMask,
                                6); // IP pool init size
        tunMgr  = new TunnelManager;

        // Enable IP forwarding
        tunMgr->execTerminalCommand("echo 1 > /proc/sys/net/ipv4/ip_forward");

        /* In case if program was terminated by error: */
        tunMgr->cleanupTunnels();

        // Pick a range of private addresses and perform NAT over chosen network interface.
        std::string virtualLanAddress = cliParams.virtualNetworkIp + '/' + cliParams.networkMask;
        std::string physInterfaceName = cliParams.physInterface;

        // Delete previous rule if server crashed:
        std::string delPrevPostrouting
                = "iptables -t nat -D POSTROUTING -s " + virtualLanAddress +
                  " -o " + physInterfaceName + " -j MASQUERADE";
        tunMgr->execTerminalCommand(delPrevPostrouting);

        std::string postrouting
                = "iptables -t nat -A POSTROUTING -s " + virtualLanAddress +
                  " -o " + physInterfaceName + " -j MASQUERADE";
        tunMgr->execTerminalCommand(postrouting);
        initSsl(); // initialize ssl context
    }

    ~VPNServer() {
        // Clean all tunnels with prefix "vpn_"
        tunMgr->cleanupTunnels();
        // Disable IP Forwarding:
        tunMgr->execTerminalCommand("echo 0 > /proc/sys/net/ipv4/ip_forward");
        // Remove NAT rule from iptables:
        std::string virtualLanAddress = cliParams.virtualNetworkIp + '/' + cliParams.networkMask;
        std::string physInterfaceName = cliParams.physInterface;
        std::string postrouting
                = "iptables -t nat -D POSTROUTING -s " + virtualLanAddress +
                  " -o " + physInterfaceName + " -j MASQUERADE";
        tunMgr->execTerminalCommand(postrouting);

        wolfSSL_CTX_free(ctx);
        wolfSSL_Cleanup();

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

        mutex.lock();
            std::cout << "\033[4;32mType 'exitvpn' in terminal to close VPN Server(DTLS ver.)\033[0m"
                      << std::endl;
        mutex.unlock();

        std::thread t(&VPNServer::createNewConnection, this);
        t.detach();
        while(!isExit) {
            getline(std::cin, input);
            if(input == "exitvpn") {
                isExit = true;
            }
        }
        TunnelManager::log("Closing the VPN Server...");
    }

    /**
     * @brief createNewConnection\r\n
     * Method creates new connection (tunnel)
     * waiting for client. When client is connected,
     * the new instance of this method will be runned
     * in another thread
     */
    void createNewConnection() {
        mutex.lock();
        // create ssl from sslContext:
        // run commands via unix terminal (needs sudo)
        in_addr_t serTunAddr    = manager->getAddrFromPool();
        in_addr_t cliTunAddr    = manager->getAddrFromPool();
        std::string serverIpStr = IPManager::getIpString(serTunAddr);
        std::string clientIpStr = IPManager::getIpString(cliTunAddr);
        size_t tunNumber        = tunMgr->getTunNumber();
        std::string tunStr      = "vpn_tun" + std::to_string(tunNumber);

        if(serTunAddr == 0 || cliTunAddr == 0) {
            TunnelManager::log("No free IP addresses. Tunnel will not be created.",
                               std::cerr);
            return;
        }

        //TunnelManager::log("Working with tunnel [" + tunStr + "] now..");
        tunMgr->createUnixTunnel(serverIpStr,
                                 clientIpStr,
                                 tunStr);
        // Get TUN interface.
        int interface = get_interface(tunStr.c_str());

        // fill array with parameters to send:
        buildParameters(clientIpStr);
        mutex.unlock();

        // wait for a tunnel.
        std::pair<int, WOLFSSL*> tunnel;
        while ((tunnel = get_tunnel(port.c_str())).first != -1
               &&
               tunnel.second != nullptr) {

            TunnelManager::log("New client connected to [" + tunStr + "]");
            std::string tempTunStr = tunStr;

            /* if client is connected then run another instance of connection
             * in a new thread: */
            std::thread thr(&VPNServer::createNewConnection, this);
            thr.detach();

            // put the tunnel into non-blocking mode.
            fcntl(tunnel.first, F_SETFL, O_NONBLOCK);

            int sentParameters = -12345;
            // send the parameters several times in case of packet loss.
            for (int i = 0; i < 3; ++i) {
                sentParameters =
                    wolfSSL_send(tunnel.second, cliParams.parametersToSend,
                                 sizeof(cliParams.parametersToSend),
                                 MSG_NOSIGNAL);

                    if(sentParameters < 0) {
                    TunnelManager::log("Error sending parameters: " +
                                       std::to_string(sentParameters));
                    int e = wolfSSL_get_error(tunnel.second, 0);
                    printf("error = %d, %s\n", e, wolfSSL_ERR_reason_error_string(e));
                }
            }

            // allocate the buffer for a single packet.
            char packet[32767];

            int timer = 0;

            /** @todo:
             * When received zero packet from client (packet[0] == 0)
             * and packet[1] == CLIENT_DISCONNECTED,
             * then set isClientConnected to false
             * */
            bool isClientConnected = true;

            // we keep forwarding packets till something goes wrong.
            while (isClientConnected) {
                // assume that we did not make any progress in this iteration.
                bool idle = true;

                // read the outgoing packet from the input stream.
                int length = read(interface, packet, sizeof(packet));
                if (length > 0) {
                    // write the outgoing packet to the tunnel.
                    int sentData = wolfSSL_send(tunnel.second, packet, length, MSG_NOSIGNAL);
                    if(sentData < 0) {
                        TunnelManager::log("sentData < 0");
                        int e = wolfSSL_get_error(tunnel.second, 0);
                        printf("error = %d, %s\n", e, wolfSSL_ERR_reason_error_string(e));
                    } else {
                        // TunnelManager::log("outgoing packet from interface to the tunnel.");
                    }

                    // there might be more outgoing packets.
                    idle = false;

                    // if we were receiving, switch to sending.
                    if (timer < 1) {
                        timer = 1;
                    }
                }

                // read the incoming packet from the tunnel.
                length = wolfSSL_recv(tunnel.second, packet, sizeof(packet), 0);
                if (length == 0) {
                    TunnelManager::log(std::string() +
                                       "recv() length == " +
                                       std::to_string(length) +
                                       ". Breaking..",
                                       std::cerr);
                    break;
                }
                if (length > 0) {
                    // ignore control messages, which start with zero.
                    if (packet[0] != 0) {
                        // write the incoming packet to the output stream.
                        int sentData = write(interface, packet, length);
                        if(sentData < 0) {
                            TunnelManager::log("write(interface, packet, length) < 0");
                        } else {
                            // TunnelManager::log("written the incoming packet to the output stream..");
                        }

                    } else {
                        /// if(packet[1] == CLIENT_DISCONNECTED) {
                        /// @todo: Here check if client sent
                        /// 'disconnect'-packet and disconnect from client
                        /// }
                        TunnelManager::log("Recieved empty control msg from client");
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
                    std::this_thread::sleep_for(std::chrono::microseconds(100000));

                    // increase the timer. This is inaccurate but good enough,
                    // since everything is operated in non-blocking mode.
                    timer += (timer > 0) ? 100 : -100;

                    // we are receiving for a long time but not sending
                    if (timer < -10000) {  // -16000
                        // send empty control messages.
                        packet[0] = 0;
                        for (int i = 0; i < 3; ++i) {
                            int sentData = wolfSSL_send(tunnel.second, packet, 1, MSG_NOSIGNAL);
                            if(sentData < 0) {
                                TunnelManager::log("sentData < 0");
                                int e = wolfSSL_get_error(tunnel.second, 0);
                                printf("error = %d, %s\n", e, wolfSSL_ERR_reason_error_string(e));
                            } else {
                                TunnelManager::log("sent empty control packet");
                            }
                        }

                        // switch to sending.
                        timer = 1;
                    }

                    // we are sending for a long time but not receiving.
                    if (timer > TIMEOUT_LIMIT) {
                        TunnelManager::log("[" + tempTunStr + "]" +
                                           "Sending for a long time but"
                                           " not receiving. Breaking...");
                        break;
                    }
                }
            }
            TunnelManager::log("Client has been disconnected from tunnel [" +
                               tempTunStr + "]");

            //close(tunnel.first);
            // wolfSSL_set_fd(tunnel.second, 0);
            wolfSSL_shutdown(tunnel.second);
            wolfSSL_free(tunnel.second);
            //
            manager->returnAddrToPool(serTunAddr);
            manager->returnAddrToPool(cliTunAddr);
            tunMgr->closeTunNumber(tunNumber);
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
        if(argc < 2) {
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

    std::pair<int, WOLFSSL*>
    get_tunnel(const char *port) {
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
                return std::pair<int, WOLFSSL*>(-1, nullptr);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100000));
        }

        // receive packets till the secret matches.
        char packet[1024];
        socklen_t addrlen;

            addrlen = sizeof(addr);
            int n = recvfrom(tunnel, packet, sizeof(packet), 0,
                    (sockaddr *)&addr, &addrlen);
            if (n <= 0) {
                return std::pair<int, WOLFSSL*>(-1, nullptr);
            }
            packet[n] = 0;


        // connect to the client
        connect(tunnel, (sockaddr *)&addr, addrlen);

        WOLFSSL* ssl;

        /* Create the WOLFSSL Object */
        if ((ssl = wolfSSL_new(ctx)) == NULL) {
            printf("wolfSSL_new error.\n");
            exit(1);
        }

        /* set the session ssl to client connection port */
        wolfSSL_set_fd(ssl, tunnel);
        wolfSSL_set_using_nonblock(ssl, 1);

        if(wolfSSL_accept(ssl) != WOLFSSL_SUCCESS) {
            wolfSSL_free(ssl);
            std::pair<int, WOLFSSL*>(-1, nullptr);
        }

        return std::pair<int, WOLFSSL*>(tunnel, ssl);
    }

    void initSsl() {
        char caCertLoc[]   = "certs/ca_cert.pem";
        char servCertLoc[] = "certs/server-cert.pem";
        char servKeyLoc[]  = "certs/server-key.pem";
        /* Initialize wolfSSL */
        wolfSSL_Init();

        /* Set ctx to DTLS 1.2 */
        if ((ctx = wolfSSL_CTX_new(wolfDTLSv1_2_server_method())) == NULL) {
            printf("wolfSSL_CTX_new error.\n");
            exit(1);
        }
        /* Load CA certificates */
        if (wolfSSL_CTX_load_verify_locations(ctx,caCertLoc,0) !=
                SSL_SUCCESS) {
            printf("Error loading %s, please check the file.\n", caCertLoc);
            exit(1);
        }
        /* Load server certificates */
        if (wolfSSL_CTX_use_certificate_file(ctx, servCertLoc, SSL_FILETYPE_PEM) !=
                                                                     SSL_SUCCESS) {
            printf("Error loading %s, please check the file.\n", servCertLoc);
            exit(1);
        }
        /* Load server Keys */
        if (wolfSSL_CTX_use_PrivateKey_file(ctx, servKeyLoc,
                    SSL_FILETYPE_PEM) != SSL_SUCCESS) {
            printf("Error loading %s, please check the file.\n", servKeyLoc);
            exit(1);
        }
    }
};

#endif // VPN_SERVER_HPP
