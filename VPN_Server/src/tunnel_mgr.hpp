#ifndef TUNNEL_MGR_HPP
#define TUNNEL_MGR_HPP

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <list>
#include <chrono>   // std::chrono::system_clock::now()
#include <queue>
#include <set>

#include <unistd.h> // pid_t
#include <signal.h> // SIGINT

/**
 * @brief The TunnelManager class
 * Contains tunnel set of currently using tunnels
 * Generates number of next tunnel and
 * contains a queue of freed tunnels
 */
class TunnelManager {
private:
    const char*        pidListFilename;
    const char*        tunListFilename;
    std::fstream       file;
    std::queue<size_t> tunQueue;
    std::set<size_t>   tunSet;
    size_t             tunNumber;
public:
    /* Forbid creating default ctor and copy ctor: */
    TunnelManager() = delete;
    TunnelManager(TunnelManager& that) = delete;

    TunnelManager
    (const char* pidListFilename, const char* tunListFilename)
        : pidListFilename(pidListFilename),
          tunListFilename(tunListFilename),
          tunNumber(0)
    { }

    ~TunnelManager() {
        cleanFile(pidListFilename);
        cleanFile(tunListFilename);
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

    const char* getTunListFilename() const {
        return tunListFilename;
    }

    const char* getPidListFilename() const {
        return pidListFilename;
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
    }

    void closeTunNumber(const size_t& num) {
        closeiftun(std::string() + "tun" + std::to_string(num));
        tunQueue.push(num);
        tunSet.erase(num);
    }

    void closeAllTunnels() {
        for(const size_t& tunNum : tunSet) {
             closeiftun(std::string() + "tun" + std::to_string(tunNum));
        }
    }

    void closeAllTunnels(const std::list<std::string>& tunList) {
        for(std::string s : tunList) {
            TunnelManager::log("Closing tunnel '" + s + '\'');
            closeiftun(s);
        }
        cleanFile(tunListFilename);
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
        cleanFile(pidListFilename);
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
     * @brief currentTime
     * @return string with time in format "<WWW MMM DD hh:mm:ss yyyy>"
     */
    static std::string currentTime() {
        auto currTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::string time = std::string() +  "<" + ctime(&currTime);
        time = time.substr(0, time.length() - 1) + ">";
        return time;
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
        s << currentTime() << ' '
          << msg << std::endl;
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
        /*
        // write tunnel to created tunnels list:
        write(getTunListFilename(), tunName);
        // write process pid to created processes list:
        write(getPidListFilename(), to_string(getpid()));
        */
    }

};

#endif // TUNNEL_MGR_HPP
