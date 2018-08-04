#ifndef IP_MANAGER_HPP
#define IP_MANAGER_HPP

#include <queue>
#include <string>
#include <mutex>

/**
 * @brief The IPManager class
 *        Contains IPv4 network info, such as\r\n
 *        network address, network mask and last\r\n
 *        generated client IP address.\r\n
 */
class IPManager {

public:
    typedef unsigned int uint32_t;
    typedef uint32_t in_addr_t;

public:
    /* Forbid copy ctor and standart ctor: */
    IPManager() = delete;
    IPManager(IPManager& that) = delete;
    IPManager(const std::string& ipAndMask, size_t poolInitSize = 100);
    ~IPManager();

    in_addr_t getAddrFromPool();
    void returnAddrToPool(in_addr_t ip);
    in_addr_t getSockaddrIn();
    in_addr_t genNextIp();
    uint32_t networkCapacity();
    bool isInRange(in_addr_t nextAddr);
    std::string ipToString();
    std::string maskString();
    std::string getNetworkString();

    static std::string ipToString(in_addr_t ip);

private:
    in_addr_t              networkAddress_, ipaddr_, subnetMask_;
    std::mutex             mutex_;
    std::queue<in_addr_t>* addrPoolPtr_;
    size_t                 usedAddrCounter_;
};

#endif // IP_MANAGER_HPP
