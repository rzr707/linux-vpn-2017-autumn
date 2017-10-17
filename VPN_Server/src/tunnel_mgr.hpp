#ifndef TUNNEL_MGR_HPP
#define TUNNEL_MGR_HPP

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <list>
#include <chrono>   // std::chrono::system_clock::now()

#include <unistd.h> // pid_t
#include <signal.h> // SIGINT

/**
 * @brief The TunnelManager class
 * Writes/reads tunnels which were
 * created by VPN Server from .dat files
 * (needs to close opened tunnels in VPN Server
 * destructor and close previously opened tunnels after
 * crashes of VPN Server).
 * Also closes all forked processes
 * (processes.dat contains PIDs of them)
 */
class TunnelManager {
private:
    const char*  pidListFilename;
    const char*  tunListFilename;
    std::fstream file;
public:
    /* Forbid creating default ctor and copy ctor: */
    TunnelManager() = delete;
    TunnelManager(TunnelManager& that) = delete;

    TunnelManager
    (const char* pidListFilename, const char* tunListFilename)
        : pidListFilename(pidListFilename), tunListFilename(tunListFilename)
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

    void closeiftun(const std::string& tunStr) {
        std::string t = "ip link set dev " + tunStr + " down";
        execTerminalCommand(t.c_str());
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

};

#endif // TUNNEL_MGR_HPP
