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

#include <memory>

/**
 * @brief The VPNServer class<br>
 * Main application class that<br>
 * organizes a process of creating, removing and processing<br>
 * vpn tunnels, provides encrypting/decrypting of packets.<br>
 * To run the server loop call 'initServer' method.<br>
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
    const unsigned       default_values = 7;
    WOLFSSL_CTX*         ctx;

public:
    enum PacketType {
        ZERO_PACKET            = 0,
        CLIENT_WANT_CONNECT    = 1,
        CLIENT_WANT_DISCONNECT = 2
    };

    explicit VPNServer(int argc, char** argv);
    ~VPNServer();

    void initServer();
    void createNewConnection();
    void SetDefaultSettings(std::string *&in_param, const size_t& type);
    void parseArguments(int argc, char** argv);
    ClientParameters* buildParameters(const std::string& clientIp);
    int get_interface(const char *name);
    std::pair<int, WOLFSSL*> get_tunnel(const char *port);
    void initSsl();

};

#endif // VPN_SERVER_HPP
