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
    in_addr_t              netmask; // маска подсети
    std::mutex             mutex;
    std::queue<in_addr_t>* addrPoolPtr;
    size_t                 usedAddrCounter;

public:
    /* Forbid copy ctor and standart ctor: */
    IPManager() = delete;
    IPManager(IPManager& that) = delete;

    /**
     * @brief IPManager constructor
     * @param ipAndMask - contains ip address and mask bits
     *  like "x.x.x.x/y" where y is bit count (from 0 to 32)
     */
    explicit IPManager(std::string ipAndMask, size_t poolInitSize = 100) {

        size_t slashPos = ipAndMask.find('/');

        if(slashPos == std::string::npos ||
            ipAndMask.at(slashPos) == ipAndMask.at(ipAndMask.length() - 1)) {
            std::cerr << "Bit mask was not found. Using default: 255.255.255.0"
                      << std::endl;
            ipaddr = inet_addr(ipAndMask.c_str());
            networkAddress = ipaddr;
            netmask = inet_addr("255.255.255.0");
        } else {
            std::string ip = ipAndMask.substr(0, slashPos);
            uint32_t networkMaskBitCount = atoi(ipAndMask.substr(slashPos + 1).c_str());

            ipaddr         = inet_addr(ip.c_str());
            networkAddress = ipaddr;

            in_addr_t tempmask = (0xffffffff >> (32 - networkMaskBitCount )) << (32 - networkMaskBitCount);
            netmask = htonl(tempmask);

            // address pool init:
            usedAddrCounter = 0;
            addrPoolPtr = new std::queue<in_addr_t>();
            for(size_t i = 0; i < poolInitSize; ++i) {
                addrPoolPtr->push(genNextIp());
            }
        }
    }

    ~IPManager() {
        delete addrPoolPtr;
    }

    /**
     * @brief getAddrFromPool
     * @return IP address from the pool of addresses
     */
    in_addr_t getAddrFromPool() {

        mutex.lock();

        if(addrPoolPtr->empty()) {
            if(usedAddrCounter < networkCapacity()) {
                for(size_t i = 0; i < usedAddrCounter; ++i) {
                    addrPoolPtr->push(genNextIp());
                }
            } else {
                return 0; // can't give IP address
                // throw std::runtime_error("usedAddrCounter > networkCapacity");
            }
        }
        in_addr_t result = addrPoolPtr->front();
        addrPoolPtr->pop();
        ++usedAddrCounter; // counter of addresses that are already using by tunnels

        mutex.unlock();
        return result;
    }

    /**
     * @brief returnAddrToPool
     * Pushes back freed IP-address back<br>
     * to the pool of IP addresses
     * @param ip - ip address to push into queue
     */
    void returnAddrToPool(in_addr_t ip) {
        mutex.lock();
            addrPoolPtr->push(ip);
            --usedAddrCounter;
        mutex.unlock();
    }

    in_addr_t getSockaddrIn() {
        return ipaddr;
    }

    in_addr_t genNextIp() {
        in_addr_t temp = ipaddr;
        temp = ntohl(temp);
        temp = temp + 1;
        temp = htonl(temp);

        if(isInRange(temp))
            ipaddr = temp;
        else {
            return 0;
            // throw std::runtime_error("Cannot allocate new IP address!");
        }

        return ipaddr;
    }

    // wrong capacity if the mask is "255.255.255.255"
    uint32_t networkCapacity() {
        in_addr_t lowerBound = ntohl((networkAddress & netmask));
        in_addr_t upperBound = ((lowerBound | ntohl((~netmask))));

        return upperBound - lowerBound;
    }

    /**
     * @brief isInRange checks, is given IPv4 address is in\r\n
     *        given network 'networkAddress' range\r\n
     * @param nextAddr - IPv4 address to check
     * @return true if an address is in network address range, otherwise false
     */
    bool isInRange(in_addr_t nextAddr) {
        in_addr_t lowerBound = ntohl((networkAddress & netmask));
        in_addr_t upperBound = ((lowerBound | ntohl((~netmask))));

        if(ntohl(nextAddr) >= lowerBound &&
           ntohl(nextAddr) <= upperBound) {
            return true;
        }

        return false;
    }

    std::string getIpString() {
        return IPManager::getIpString(ipaddr);
    }

    std::string maskString() {
        return IPManager::getIpString(netmask);
    }

    std::string getNetworkString() {
        return IPManager::getIpString(networkAddress);
    }


    static std::string getIpString(in_addr_t ip) {
        char buffer[INET_ADDRSTRLEN];
        return inet_ntop(AF_INET, &ip, buffer, INET_ADDRSTRLEN);
    }

};

#endif // IP_MANAGER_HPP
