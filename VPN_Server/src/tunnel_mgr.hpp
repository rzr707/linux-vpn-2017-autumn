#ifndef TUNNEL_MGR_HPP
#define TUNNEL_MGR_HPP

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <list>
#include <chrono>   // std::chrono::system_clock::now()
#include <queue>
#include <set>
#include <thread>
#include <iomanip> // std::put_time
#include <string.h>

#include <unistd.h> // pid_t
#include <signal.h> // SIGINT

#include <sys/types.h>
#include <ifaddrs.h>

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

    explicit TunnelManager() : tunNumber(0) {
        // DelTunFromFile("", true);
    }

    ~TunnelManager() {
        cleanupTunnels("vpn_tun");
    }

    void write(const std::string& filename,
               const std::string& infoToWrite) {
        file.open(filename, std::ios_base::out | std::ios_base::app);
        if(!file.is_open()) {
            throw std::runtime_error(std::string() +
                                     "Could not open file to write: " +
                                     filename);
        }
        file << infoToWrite << std::endl;
        file.close();
    }

    std::list<std::string> getInfoList
    (const char* filename) {
        file.open(filename, std::ios_base::in);
        if(!file.is_open()) {
            throw std::runtime_error(std::string() +
                                     "Could not open file to read:" +
                                     filename);
        }
        std::list<std::string> list;
        std::string line;
        while(getline(file, line)) {
            list.push_back(line);
        }
        file.close();

        return list;
    }

    void cleanFile(const char* filename) {
        file.open(filename,
                  std::ios_base::out | std::ios_base::trunc);
        file.close();
    }

    /**
     * @brief execTerminalCommand - runs command from terminal (linux console)
     * @param cmd
     */
    void execTerminalCommand(const std::string& cmd) {
        std::string status = ": (OK: EXECUTED)";
        if(system(cmd.c_str()) != 0)
            status = ": (ERROR: NOT EXECUTED)";
        else
            TunnelManager::log(cmd + status);
    }

    /**
     * @brief closeiftun
     * delete tunnel interface
     * @param tunStr - tunnel interface name (e.g. 'tun3')
     */
    void closeiftun(const std::string& tunStr) {
        std::string t = "ip link delete " + tunStr;
                // "ip link set dev " + tunStr + " down";
        execTerminalCommand(t.c_str());
        //DelTunFromFile(tunStr);
    }

    void closeTunNumber(const size_t& num, const std::string tunPrefix = "vpn_") {
        closeiftun(std::string() + tunPrefix + "tun" + std::to_string(num));
        tunQueue.push(num);
        tunSet.erase(num);
    }

    void closeAllTunnels(const std::string tunPrefix = "vpn_") {
        for(const size_t& tunNum : tunSet) {
             closeiftun(std::string() + tunPrefix +  "tun" + std::to_string(tunNum));
        }
    }

    void closeAllTunnels(const std::list<std::string>& tunList) {
        for(std::string s : tunList) {
            TunnelManager::log("Closing tunnel '" + s + '\'');
            closeiftun(s);
        }
    }

    void killProcess(const std::string& procId) {
        kill(atoi(procId.c_str()), SIGINT);
    }

    void killAllProcesses(const std::list<std::string>& procList) {
        for(const std::string& s : procList) {
            TunnelManager::log("Killing process with PID " + s);
            if(s != std::to_string(getpid())) {
                killProcess(s);
            } else {
                TunnelManager::log("No need to kill parent process");
            }
        }
    }

    /**
     * @brief getTunNumber
     * @return the number of tunnel to create it
     */
    size_t getTunNumber() {
        if(tunQueue.empty()) {
            tunSet.insert(tunNumber);
            return tunNumber++;
        }
        size_t result = tunQueue.front();
        tunQueue.pop();
        tunSet.insert(result);
        return result;
    }

    void removeTunFromSet(const size_t& tunNumber) {
        tunSet.erase(tunNumber);
    }

    /**
     * @brief initUnixSettings - uplink new p2p tunnel
     *                           (server must be running with root permissions)
     * @param serverTunAddr    - server tunnel ip
     * @param clientTunAddr    - client tunnel ip
     */
    void createUnixTunnel
    (const std::string& serverTunAddr,
     const std::string& clientTunAddr,
     const std::string&      tunStr) {

        std::string tunName = tunStr;
        std::string tunInterfaceSetup = "ip tuntap add dev " + tunName +  " mode tun";
        execTerminalCommand(tunInterfaceSetup);

        std::string ifconfig = "ifconfig " + tunName + " " + serverTunAddr +
                          " dstaddr " + clientTunAddr + " up";
        execTerminalCommand(ifconfig);
        // SaveTunToFile(tunName);
    }
    
    /*
    void SaveTunToFile(const std::string& TunName)
    {
        save_tun_file.open(SaveTunListFilename, std::ios_base::out | std::ios_base::app);
        if(!save_tun_file.is_open()) {
            throw std::runtime_error(std::string() + "TunnelManager::SaveTunToFile - Could not open file to save tun information");
        }
        
        save_tun_file << TunName << std::endl;
        save_tun_file.close();
    }
    
    void DelTunFromFile(const std::string& TunName, bool ClearAllData = false)
    {
        int tun_length = 0;
        int position = 0;
        std::string line;
        
        save_tun_file.open(SaveTunListFilename, std::ios::in | std::ios::out);
        if(!save_tun_file.is_open()) {
            throw std::runtime_error(std::string() + "TunnelManager::SaveTunToFile - Could not open file to save tun information");
        }
        
        if(!ClearAllData)
        {
            while(getline(save_tun_file, line)) {  
                if(line[0] != ' ' && !strcmp(line.c_str(), TunName.c_str()))
                {
                    save_tun_file.seekg(position);
                    tun_length = line.length();
                    while(tun_length--)
                        save_tun_file << ' ';
                    break;
                }
                else
                    position+=line.length()+1;
            }
        }
        else
        {
            while(getline(save_tun_file, line)) 
                if(line[0] != ' ')
                    execTerminalCommand("ip link delete " + line);
                
            cleanFile(SaveTunListFilename);
        }
        
        save_tun_file.close();
    } */

    /**
     * @brief cleanupTunnels method cleans uplinked tunnels previous<br>
     * VPN Server work (if server crashed and didn't delete interfaces<br>
     * from the system.
     * @param tunnelPrefix - tunnel prefix to compare. e.g. "vpn_tun"<br>
     */
    void cleanupTunnels(const char* tunnelPrefix = "vpn_") {
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

    /**
     * @brief log - log out the message to output stream 's'
     * (needs to be reimplemented as separate
     *  class in log.hpp or utils.hpp)
     * @param msg - message to log out
     * @param s   - output stream
     */
    static void log(const std::string& msg,
                    std::ostream& s = std::cout) {

        auto timeNow = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    timeNow.time_since_epoch()
                    ) % 1000;

        std::time_t currTime
                = std::chrono::system_clock::to_time_t(timeNow);


        s << std::put_time(std::localtime(&currTime), "<%d.%m.%y %H:%M:%S.")
          << ms.count() << '>'
          << " <THREAD ID: " << std::this_thread::get_id()  << '>'
          << ' '
          << msg << std::endl;
    }
};

#endif // TUNNEL_MGR_HPP
