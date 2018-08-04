#include "tunnel_mgr.hpp"
#include "utils/utils.hpp"

#include <stdexcept>
#include <chrono>   // std::chrono::system_clock::now()
#include <thread>
#include <iomanip> // std::put_time
#include <string.h>

#include <unistd.h> // pid_t
#include <signal.h> // SIGINT

#include <sys/types.h>
#include <ifaddrs.h>

TunnelManager::TunnelManager() : tunNumber_(0) { }

TunnelManager::~TunnelManager() {
    cleanupTunnels("vpn_tun");
}

/**
 * @brief execTerminalCommand - runs command from terminal (linux console)
 * @param cmd
 */
void TunnelManager::execTerminalCommand(const std::string& cmd) {
    std::string status = ": (OK: EXECUTED)";
    if(system(cmd.c_str()) != 0) {
        status = ": (ERROR: NOT EXECUTED)";
    } else {
        utils::Logger::log(cmd + status);
    }
}

/**
 * @brief closeiftun
 * delete tunnel interface
 * @param tunStr - tunnel interface name (e.g. 'tun3')
 */
void TunnelManager::closeiftun(const std::string& tunStr) {
    std::string t = "ip link delete " + tunStr;
    execTerminalCommand(t.c_str());
}

void TunnelManager::closeTunNumber(const size_t num,
                                   const std::__cxx11::string& tunPrefix) {
    closeiftun(std::string() + tunPrefix + "tun" + std::to_string(num));
    tunQueue_.push(num);
    tunSet_.erase(num);
}

void TunnelManager::closeAllTunnels(const std::string tunPrefix) {
    for(const size_t& tunNum : tunSet_) {
         closeiftun(std::string() + tunPrefix +  "tun" + std::to_string(tunNum));
    }
}

void TunnelManager::closeAllTunnels(const std::list<std::string>& tunList) {
    for(std::string s : tunList) {
        utils::Logger::log("Closing tunnel '" + s + '\'');
        closeiftun(s);
    }
}

/**
 * @brief getTunNumber
 * @return the number of tunnel to create it
 */
size_t TunnelManager::getTunNumber() {
    if(tunQueue_.empty()) {
        tunSet_.insert(tunNumber_);
        return tunNumber_++;
    }
    size_t result = tunQueue_.front();
    tunQueue_.pop();
    tunSet_.insert(result);
    return result;
}

void TunnelManager::removeTunFromSet(const size_t& tunNumber) {
    tunSet_.erase(tunNumber);
}

/**
 * @brief initUnixSettings - uplink new p2p tunnel
 *                           (server must be running with root permissions)
 * @param serverTunAddr    - server tunnel ip
 * @param clientTunAddr    - client tunnel ip
 */
void TunnelManager::createUnixTunnel
(const std::string& serverTunAddr,
 const std::string& clientTunAddr,
 const std::string&      tunStr) {

    std::string tunName = tunStr;
    std::string tunInterfaceSetup = "ip tuntap add dev " + tunName +  " mode tun";
    execTerminalCommand(tunInterfaceSetup);

    std::string ifconfig = "ifconfig " + tunName + " " + serverTunAddr +
                      " dstaddr " + clientTunAddr + " up";
    execTerminalCommand(ifconfig);
}

/**
 * @brief cleanupTunnels method cleans uplinked tunnels previous<br>
 * VPN Server work (if server crashed and didn't delete interfaces<br>
 * from the system.
 * @param tunnelPrefix - tunnel prefix to compare. e.g. "vpn_tun"<br>
 */
void TunnelManager::cleanupTunnels(const char* tunnelPrefix) {
    ifaddrs *iface, *temp;
    getifaddrs(&iface);
    temp = iface;

    while(temp) {
        if(strstr(temp->ifa_name, tunnelPrefix)) {
            closeiftun(temp->ifa_name);
        }
        temp = temp->ifa_next;
    }
    freeifaddrs(iface);
}
