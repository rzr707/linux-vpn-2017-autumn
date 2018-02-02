#include "vpn_server.hpp"

VPNServer::VPNServer (int argc, char** argv) {
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

VPNServer::~VPNServer() {
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
 * @brief initServer\r\n
 * Main method, creates first thread with vpn connection,\r\n
 * waiting for a client
 */
void VPNServer::initServer() {
    mutex.lock();
        std::cout << "\033[4;32mVPN Service is started (DTLS, ver.29.11.17)\033[0m"
                  << std::endl;
    mutex.unlock();

    std::thread t(&VPNServer::createNewConnection, this);
    t.detach();

    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(100));
    }
}

/**
 * @brief createNewConnection\r\n
 * Method creates new connection (tunnel)
 * waiting for client. When client is connected,
 * the new instance of this method will be runned
 * in another thread
 */
void VPNServer::createNewConnection() {
    mutex.lock();
    // create ssl from sslContext:
    // run commands via unix terminal (needs sudo)
    in_addr_t serTunAddr    = manager->getAddrFromPool();
    in_addr_t cliTunAddr    = manager->getAddrFromPool();
    std::string serverIpStr = IPManager::getIpString(serTunAddr);
    std::string clientIpStr = IPManager::getIpString(cliTunAddr);
    size_t tunNumber        = tunMgr->getTunNumber();
    std::string tunStr      = "vpn_tun" + std::to_string(tunNumber);
    std::string tempTunStr = tunStr;
    int interface = 0; // Tun interface
    int sentParameters = -12345;
    int e = 0;
    // allocate the buffer for a single packet.
    char packet[32767];
    int timer = 0;
    bool isClientConnected = true;
    bool idle = true;
    int length = 0;
    int sentData = 0;
    std::pair<int, WOLFSSL*> tunnel;

    if(serTunAddr == 0 || cliTunAddr == 0) {
        TunnelManager::log("No free IP addresses. Tunnel will not be created.",
                           std::cerr);
        return;
    }

    tunMgr->createUnixTunnel(serverIpStr,
                             clientIpStr,
                             tunStr);
    // Get TUN interface.
    interface = get_interface(tunStr.c_str());

    mutex.unlock();

    // fill array with parameters to send:
    std::unique_ptr<ClientParameters> cliParams(buildParameters(clientIpStr));

    // wait for a tunnel.
    while ((tunnel = get_tunnel(port.c_str())).first != -1
           &&
           tunnel.second != nullptr) {

        TunnelManager::log("New client connected to [" + tunStr + "]");

        // send the parameters several times in case of packet loss.
        for (int i = 0; i < 3; ++i) {
            sentParameters =
                wolfSSL_send(tunnel.second, cliParams->parametersToSend,
                             sizeof(cliParams->parametersToSend),
                             MSG_NOSIGNAL);

            if(sentParameters < 0) {
                TunnelManager::log("Error sending parameters: " +
                                std::to_string(sentParameters));
                e = wolfSSL_get_error(tunnel.second, 0);
                printf("error = %d, %s\n", e, wolfSSL_ERR_reason_error_string(e));
            }
        }

        // we keep forwarding packets till something goes wrong.
        while (isClientConnected) {
            // assume that we did not make any progress in this iteration.
            idle = true;

            // read the outgoing packet from the input stream.
            length = read(interface, packet, sizeof(packet));
            if (length > 0) {
                // write the outgoing packet to the tunnel.
                sentData = wolfSSL_send(tunnel.second, packet, length, MSG_NOSIGNAL);
                if(sentData < 0) {
                    TunnelManager::log("sentData < 0");
                    e = wolfSSL_get_error(tunnel.second, 0);
                    printf("error = %d, %s\n", e, wolfSSL_ERR_reason_error_string(e));
                }

                // there might be more outgoing packets.
                idle = false;

                // if we were receiving, switch to sending.
                if (timer < 1)
                    timer = 1;
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
                    sentData = write(interface, packet, length);
                    if(sentData < 0) {
                        TunnelManager::log("write(interface, packet, length) < 0");
                    }
                } else {
                    TunnelManager::log("Recieved empty control msg from client");
                    if(packet[1] == CLIENT_WANT_DISCONNECT && length == 2) {
                        TunnelManager::log("WANT_DISCONNECT from client");
                        isClientConnected = false;
                    }
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
                        sentData = wolfSSL_send(tunnel.second, packet, 1, MSG_NOSIGNAL);
                        if(sentData < 0) {
                            TunnelManager::log("sentData < 0");
                            e = wolfSSL_get_error(tunnel.second, 0);
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

        break;
    }
    TunnelManager::log("Tunnel closed.");
    wolfSSL_shutdown(tunnel.second);
    wolfSSL_free(tunnel.second);
    //
    manager->returnAddrToPool(serTunAddr);
    manager->returnAddrToPool(cliTunAddr);
    tunMgr->closeTunNumber(tunNumber);
}

/**
 * @brief SetDefaultSettings
 * Set parameters if they were not set by
 * user via terminal arguments on startup
 * @param in_param - pointer on reference of parameter string to check
 * @param type     - index of parameters
 */
void VPNServer::SetDefaultSettings(std::string *&in_param, const size_t& type) {
    if(!in_param->empty())
        return;

    std::string default_values[] = {
        "1400",
        "10.0.0.0", "8",
        "8.8.8.8",
        "0.0.0.0", "0",
        "eth0"
    };

    *in_param = default_values[type];
}

/**
 * @brief parseArguments method
 * is parsing arguments from terminal
 * and fills ClientParameters structure
 * @param argc - arguments count
 * @param argv - arguments vector
 */
void VPNServer::parseArguments(int argc, char** argv) {
    std::string* std_params[default_values] = {
        &cliParams.mtu,
        &cliParams.virtualNetworkIp,
        &cliParams.networkMask,
        &cliParams.dnsIp,
        &cliParams.routeIp,
        &cliParams.routeMask,
        &cliParams.physInterface
    };

    port = argv[1]; // port to listen

    if(atoi(port.c_str()) < 1 || atoi(port.c_str()) > 0xFFFF) {
        throw std::invalid_argument(
                    "Error: invalid number of port " + port);
    }

    for(int i = 2; i < argc; ++i) {
        if(strlen(argv[i]) == 2) {
            switch (argv[i][1]) {
                case 'm':
                    if((i + 1) < argc) {
                        cliParams.mtu = argv[i + 1];
                    }
                    if(atoi(cliParams.mtu.c_str()) > 2000
                       || atoi(cliParams.mtu.c_str()) < 1000) {
                        throw std::invalid_argument("Invalid mtu");
                    }
                    break;
                case 'a':
                    if((i + 1) < argc) {
                            cliParams.virtualNetworkIp = argv[i + 1];
                            if(!correctIp(cliParams.virtualNetworkIp)) {
                                throw std::invalid_argument("Invalid network ip");
                            }
                    }
                    if((i + 2) < argc) {
                        cliParams.networkMask = argv[i + 2];
                        if(!correctSubmask(cliParams.networkMask)) {
                           throw std::invalid_argument("Invalid mask");
                        }
                    }
                    break;
                case 'd':
                    if((i + 1) < argc) {
                        cliParams.dnsIp = argv[i + 1];
                    }
                    if(!correctIp(cliParams.dnsIp)) {
                        throw std::invalid_argument("Invalid dns IP");
                    }
                    break;
                case 'r':
                    if((i + 1) < argc) {
                        cliParams.routeIp = argv[i + 1];
                    }
                    if(!correctIp(cliParams.routeIp)) {
                        throw std::invalid_argument("Invalid route IP");
                    }
                    if((i + 2) < argc) {
                        cliParams.routeMask = argv[i + 2];
                        if(!correctSubmask(cliParams.routeMask)) {
                            throw std::invalid_argument("Invalid route mask");
                        }
                    }
                    break;
                case 'i':
                    cliParams.physInterface = argv[i + 1];
                    if(!isNetIfaceExists(cliParams.physInterface)) {
                        throw std::invalid_argument("No such network interface");
                    }
                    break;
            }
        }
    }

    /* if there was no specific arguments,
     *  default settings will be set up
     */
    for(size_t i = 0; i < default_values; ++i)
        SetDefaultSettings(std_params[i], i);
}

/**
 * @brief VPNServer::correctSubmask checks netmask correctness
 * @param submaskString
 * @return true if submask is correct
 */
bool VPNServer::correctSubmask(const std::string& submaskString) {
    int submask = std::atoi(submaskString.c_str());
    return (submask >= 0) && (submask <= 32);
}

/**
 * @brief VPNServer::correctIp checks IPv4 address correctness
 * @param ipAddr
 * @return true if IPv4 address is correct
 */
bool VPNServer::correctIp(const std::string& ipAddr) {
    in_addr stub;
    return inet_pton(AF_INET, ipAddr.c_str(), &stub) == 1;
}

/**
 * @brief VPNServer::isNetIfaceExists checks existance of network interface
 * @param iface - system network interface name, e.g. 'eth0'
 * @return true if interface 'iface' exists
 */
bool VPNServer::isNetIfaceExists(const std::string& iface) {
    ifaddrs *addrs, *tmp;

    getifaddrs(&addrs);
    tmp = addrs;

    while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET)
            if(strcmp(iface.c_str(), tmp->ifa_name) == 0)
                return true;

        tmp = tmp->ifa_next;
    }

    freeifaddrs(addrs);

    return false;
}

/**
 * @brief buildParameters
 * @param clientIp - Client's tunnel IP address
 * @return         - pointer to ClientParameters structure
 * with filled parameters to send to the client.
 */
ClientParameters* VPNServer::buildParameters(const std::string& clientIp) {
    ClientParameters* cliParams = new ClientParameters;
    int size = sizeof(cliParams->parametersToSend);
    // Here is parameters string formed:
    std::string paramStr = std::string() + "m," + this->cliParams.mtu +
            " a," + clientIp + ",32 d," + this->cliParams.dnsIp +
            " r," + this->cliParams.routeIp + "," + this->cliParams.routeMask;

    // fill parameters array:
    cliParams->parametersToSend[0] = 0; // control messages always start with zero
    memcpy(&cliParams->parametersToSend[1], paramStr.c_str(), paramStr.length());
    memset(&cliParams->parametersToSend[paramStr.length() + 1], ' ', size - (paramStr.length() + 1));

    return cliParams;
}

/**
 * @brief get_interface
 * Tries to open dev/net/tun interface
 * @param name - tunnel interface name (e.g. "tun0")
 * @return descriptor of interface
 */
int VPNServer::get_interface(const char *name) {
    int interface = open("/dev/net/tun", O_RDWR | O_NONBLOCK);

    ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

    if (int status = ioctl(interface, TUNSETIFF, &ifr)) {
        throw std::runtime_error("Cannot get TUN interface\nStatus is: " +
                                 status);
    }

    return interface;
}

/**
 * @brief get_tunnel
 * Method creates listening datagram socket for income connection,
 * binds it to IP address and waiting for connection.
 * When client is connected, method initialize SSL session and
 * set socket to nonblocking mode.
 * @param port - port to listen
 * @return If success, pair with socket descriptor and
 * SSL session object pointer will be returned, otherwise
 * negative SD and nullptr.
 */
std::pair<int, WOLFSSL*> VPNServer::get_tunnel(const char *port) {
    // we use an IPv6 socket to cover both IPv4 and IPv6.
    int tunnel = socket(AF_INET6, SOCK_DGRAM, 0);
    int flag = 1;
     // receive packets till the secret matches.
    char packet[1024];
    memset(packet, 0, sizeof(packet[0] * 1024));

    socklen_t addrlen;
    WOLFSSL* ssl;

    /* Create the WOLFSSL Object */
    if ((ssl = wolfSSL_new(ctx)) == NULL) {
        throw std::runtime_error("wolfSSL_new error.");
    }
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

    addrlen = sizeof(addr);
    int recievedLen = 0;
    do {
        recievedLen = 0;
        recievedLen = recvfrom(tunnel, packet, sizeof(packet), 0,
                              (sockaddr *)&addr, &addrlen);
        /*
        TunnelManager::log("packet[0] == " +
                           std::to_string(packet[0]) +
                           ", packet[1] == " +
                           std::to_string(packet[1]) +
                           ", receivecLen == " + std::to_string(recievedLen));
        */
        if(recievedLen == 2
           && packet[0] == ZERO_PACKET
           && packet[1] == CLIENT_WANT_CONNECT)
              break;

    } while (true);

    /* if client is connected then run another instance of connection
     * in a new thread: */
    std::thread thr(&VPNServer::createNewConnection, this);
    thr.detach();

    // connect to the client
    connect(tunnel, (sockaddr *)&addr, addrlen);

    // put the tunnel into non-blocking mode.
    fcntl(tunnel, F_SETFL, O_NONBLOCK);

    // set the session ssl to client connection port
    wolfSSL_set_fd(ssl, tunnel);
    wolfSSL_set_using_nonblock(ssl, 1);

    int acceptStatus = SSL_FAILURE;
    int tryCounter   = 1;

    // Try to accept ssl connection for 50 times:
    while( (acceptStatus = wolfSSL_accept(ssl)) != SSL_SUCCESS
          && tryCounter++ <= 50) {
        TunnelManager::log("wolfSSL_accept(ssl) != SSL_SUCCESS. Sleeping..");
        std::this_thread::sleep_for(std::chrono::microseconds(200000));
    }

    if(tryCounter >= 50) {
        wolfSSL_free(ssl);
        return std::pair<int, WOLFSSL*>(-1, nullptr);
    }

    return std::pair<int, WOLFSSL*>(tunnel, ssl);
}

/**
 * @brief initSsl
 * Initialize SSL library, load certificates and keys,
 * set up DTLS 1.2 protection type.
 * Terminates the application if even one of steps is failed.
 */
void VPNServer::initSsl() {
    char caCertLoc[]   = "certs/ca_cert.crt";
    char servCertLoc[] = "certs/server-cert.pem";
    char servKeyLoc[]  = "certs/server-key.pem";
    /* Initialize wolfSSL */
    wolfSSL_Init();

    /* Set ctx to DTLS 1.2 */
    if ((ctx = wolfSSL_CTX_new(wolfDTLSv1_2_server_method())) == NULL) {
        throw std::runtime_error("wolfSSL_CTX_new error.");
    }
    /* Load CA certificates */
    if (wolfSSL_CTX_load_verify_locations(ctx, caCertLoc, 0) !=
            SSL_SUCCESS)
        certError(caCertLoc);

    /* Load server certificates */
    if (wolfSSL_CTX_use_certificate_file(ctx, servCertLoc, SSL_FILETYPE_PEM) !=
                                                                 SSL_SUCCESS)
        certError(servKeyLoc);

    /* Load server Keys */
    if (wolfSSL_CTX_use_PrivateKey_file(ctx, servKeyLoc,
                SSL_FILETYPE_PEM) != SSL_SUCCESS)
        certError(servKeyLoc);

}

void VPNServer::certError(const char * filename) {
    throw std::runtime_error(std::string() +
                             "Error loading '" +
                             filename +
                             "'. Please check if the file exists");
}
