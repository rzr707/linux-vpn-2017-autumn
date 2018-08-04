#ifndef VPN_SERVER_HPP
#define VPN_SERVER_HPP

#include "client_parameters.hpp"

#include <mutex>
#include <memory>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

class TunnelManager;
class IPManager;

/**
 * @brief The VPNServer class<br>
 * Main application class that<br>
 * organizes a process of creating, removing and processing<br>
 * vpn tunnels, provides encrypting/decrypting of packets.<br>
 * To run the server loop call 'initServer' method.<br>
 */
class VPNServer {
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
    void setDefaultSettings(std::string *&in_param, const size_t& type);
    void parseArguments(int argc, char **argv);
    bool correctSubmask(const std::string& submaskString);
    bool correctIp(const std::string& ipAddr);
    bool isNetIfaceExists(const std::string& iface);
    ClientParameters* buildParameters(const std::string& clientIp);
    int get_interface(const char *name);
    std::pair<int, WOLFSSL*> get_tunnel(const char *port_);
    void initSsl();
    void certError(const char* filename);

private:
    ClientParameters               cliParams_;
    std::unique_ptr<IPManager>     manager_;
    std::string                    port_;
    std::unique_ptr<TunnelManager> tunMgr_;
    std::recursive_mutex           mutex_;
    WOLFSSL_CTX*                   ctxPtr_;

    const int                      TIMEOUT_LIMIT_MS = 60000;
    const unsigned                 DEFAULT_ARG_COUNT = 7;
};

#endif // VPN_SERVER_HPP
