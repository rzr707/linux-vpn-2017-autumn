#ifndef TUNNEL_MGR_HPP
#define TUNNEL_MGR_HPP

#include <string>
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
public:
    /* Forbid creating default copy ctor: */
    TunnelManager(TunnelManager& that) = delete;

    TunnelManager();
    ~TunnelManager();

    void execTerminalCommand(const std::string& cmd);
    void closeiftun(const std::string& tunStr);
    void closeTunNumber(const size_t num, const std::string& tunPrefix = "vpn_");
    void closeAllTunnels(const std::string tunPrefix = "vpn_");
    void closeAllTunnels(const std::list<std::string>& tunList);
    size_t getTunNumber();
    void removeTunFromSet(const size_t& tunNumber);
    void createUnixTunnel(const std::string& serverTunAddr
                         , const std::string& clientTunAddr
                         , const std::string& tunStr);
    void cleanupTunnels(const char* tunnelPrefix = "vpn_");

private:
    std::queue<size_t> tunQueue_;
    std::set<size_t>   tunSet_;
    size_t             tunNumber_;

};

#endif // TUNNEL_MGR_HPP
