#ifndef VPN_SERVER_HPP
#define VPN_SERVER_HPP

#include "client_parameters.hpp"
#include "tunnel_mgr.hpp"
#include "ip_manager.hpp"

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

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

    VPNServer(int argc, char** argv);
    ~VPNServer();

    void initServer();
    void createNewConnection();
    void SetDefaultSettings(std::string *&in_param, const size_t& type);
    void parseArguments(int argc, char** argv);
    bool correctSubmask(const std::string& submaskString);
    bool correctIp(const std::string& ipAddr);
    bool isNetIfaceExists(const std::string& iface);
    ClientParameters* buildParameters(const std::string& clientIp);
    int get_interface(const char *name);
    std::pair<int, WOLFSSL*> get_tunnel(const char *port);
    void initSsl();
    void certError(const char* filename);

};

#endif // VPN_SERVER_HPP
