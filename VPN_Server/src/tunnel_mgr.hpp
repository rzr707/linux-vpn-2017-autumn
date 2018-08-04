#ifndef TUNNEL_MGR_HPP
#define TUNNEL_MGR_HPP

#include <iostream>
#include <fstream>
#include <queue>
#include <set>
#include <list>

#define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__ * 10)

/**
 * @brief The TunnelManager class
 * Contains tunnel set of currently using tunnels
 * Generates number of next tunnel and
 * contains a queue of freed tunnels
 */
class TunnelManager {
private:
    std::fstream       file;
    std::fstream       save_tun_file;
    std::queue<size_t> tunQueue;
    std::set<size_t>   tunSet;
    size_t             tunNumber;
public:
    /* Forbid creating default copy ctor: */
    TunnelManager(TunnelManager& that) = delete;

    explicit TunnelManager();
    ~TunnelManager();

    void execTerminalCommand(const std::string& cmd);
    void closeiftun(const std::string& tunStr);
    void closeTunNumber(const size_t num, const std::string& tunPrefix = "vpn_");
    void closeAllTunnels(const std::string tunPrefix = "vpn_");
    void closeAllTunnels(const std::list<std::string>& tunList);
    size_t getTunNumber();
    void removeTunFromSet(const size_t& tunNumber);
    void createUnixTunnel
    (const std::string& serverTunAddr,
     const std::string& clientTunAddr,
     const std::string&      tunStr);

    void cleanupTunnels(const char* tunnelPrefix = "vpn_");

    static std::string currentTime();
    static void log(const std::string& msg,
                    std::ostream& s = std::cout);

};

#endif // TUNNEL_MGR_HPP
