#ifndef VPN_SERVER_HPP
#define VPN_SERVER_HPP

#include "client_parameters.hpp"

#include <mutex>
#include <memory>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

class TunnelManager;
class IPManager;
class Tunnel;

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

    /**
     * @brief initServer\r\n
     * Main method, creates first thread with vpn connection,\r\n
     * waiting for a client
     */
    void initServer();

private:
    /**
     * @brief createNewConnection\r\n
     * Method creates new connection (tunnel)
     * waiting for client. When client is connected,
     * the new instance of this method will be runned
     * in another thread
     */
    void createNewConnection();

    /**
     * @brief SetDefaultSettings
     * Set parameters if they were not set by
     * user via terminal arguments on startup
     * @param in_param - pointer on reference of parameter string to check
     * @param type     - index of parameters
     */
    void setDefaultSettings(std::string *&in_param, const size_t& type);

    /**
     * @brief parseArguments method
     * is parsing arguments from terminal
     * and fills ClientParameters structure
     * @param argc - arguments count
     * @param argv - arguments vector
     */
    void parseArguments(int argc, char **argv);

    /**
     * @brief VPNServer::correctSubmask checks netmask correctness
     * @param submaskString
     * @return true if submask is correct
     */
    bool correctSubmask(const std::string& submaskString);

    /**
     * @brief VPNServer::correctIp checks IPv4 address correctness
     * @param ipAddr
     * @return true if IPv4 address is correct
     */
    bool correctIp(const std::string& ipAddr);

    /**
     * @brief VPNServer::isNetIfaceExists checks existance of network interface
     * @param iface - system network interface name, e.g. 'eth0'
     * @return true if interface 'iface' exists
     */
    bool isNetIfaceExists(const std::string& iface);

    /**
     * @brief buildParameters
     * @param clientIp - Client's tunnel IP address
     * @return         - pointer to ClientParameters structure
     * with filled parameters to send to the client.
     */
    ClientParameters* buildParameters(const std::string& clientIp);

    /**
     * @brief get_interface
     * Tries to open dev/net/tun interface
     * @param name - tunnel interface name (e.g. "tun0")
     * @return descriptor of interface
     */
    int get_interface(const char *name);

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
    Tunnel getTunnel(const char *port);

    /**
     * @brief initSsl
     * Initialize SSL library, load certificates and keys,
     * set up DTLS 1.2 protection type.
     * Terminates the application if even one of steps is failed.
     */
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
