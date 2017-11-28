#ifndef IP_MANAGER_HPP
#define IP_MANAGER_HPP

#include <iostream>
#include <queue>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <mutex>


/**
 * @brief The IPManager class
 *        Contains IPv4 network info, such as\r\n
 *        network address, network mask and last\r\n
 *        generated client IP address.\r\n
 */
class IPManager {
private:
    in_addr_t              networkAddress;
    in_addr_t              ipaddr;
    in_addr_t              netmask; // subnet mask
    std::mutex             mutex;
    std::queue<in_addr_t>* addrPoolPtr;
    size_t                 usedAddrCounter;

public:
    /* Forbid copy ctor and standart ctor: */
    IPManager() = delete;
    IPManager(IPManager& that) = delete;
    explicit IPManager(std::string ipAndMask, size_t poolInitSize = 100);
    ~IPManager();

    in_addr_t getAddrFromPool();
    void returnAddrToPool(in_addr_t ip);
    in_addr_t getSockaddrIn();
    in_addr_t genNextIp();
    uint32_t networkCapacity();
    bool isInRange(in_addr_t nextAddr);
    std::string getIpString();
    std::string maskString();
    std::string getNetworkString();

    static std::string getIpString(in_addr_t ip);

};

#endif // IP_MANAGER_HPP
