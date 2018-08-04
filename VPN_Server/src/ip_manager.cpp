#include "ip_manager.hpp"

#include <iostream>
#include <queue>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdexcept>

/**
 * @brief IPManager constructor
 * @param ipAndMask - contains ip address and mask bits
 *  like "x.x.x.x/y" where y is bit count (from 0 to 32)
 */
IPManager::IPManager(const std::__cxx11::string& ipAndMask, size_t poolInitSize) {
    size_t slashPos = ipAndMask.find('/');

    if(slashPos == std::string::npos ||
        ipAndMask.at(slashPos) == ipAndMask.at(ipAndMask.length() - 1)) {
        std::cerr << "Bit mask was not found. Using default: 255.255.255.0"
                  << std::endl;
        ipaddr_ = inet_addr(ipAndMask.c_str());
        networkAddress_ = ipaddr_;
        subnetMask_ = inet_addr("255.255.255.0");
    } else {
        std::string ip = ipAndMask.substr(0, slashPos);
        uint32_t networkMaskBitCount = atoi(ipAndMask.substr(slashPos + 1).c_str());

        ipaddr_         = inet_addr(ip.c_str());
        networkAddress_ = ipaddr_;

        in_addr_t tempmask = (0xffffffff >> (32 - networkMaskBitCount )) << (32 - networkMaskBitCount);
        subnetMask_ = htonl(tempmask);

        // address pool init:
        usedAddrCounter_ = 0;
        addrPoolPtr_.reset(new std::queue<in_addr_t>);
        for(size_t i = 0; i < poolInitSize; ++i) {
            addrPoolPtr_->push(genNextIp());
        }
    }
}


IPManager::~IPManager()
{
}

/**
 * @brief getAddrFromPool
 * @return IP address from the pool of addresses
 */
in_addr_t IPManager::getAddrFromPool() {

    mutex_.lock();

    if(addrPoolPtr_->empty()) {
        if(usedAddrCounter_ < networkCapacity()) {
            for(size_t i = 0; i < usedAddrCounter_; ++i) {
                addrPoolPtr_->push(genNextIp());
            }
        } else {
            return 0; // can't give IP address
            // throw std::runtime_error("usedAddrCounter > networkCapacity");
        }
    }
    in_addr_t result = addrPoolPtr_->front();
    addrPoolPtr_->pop();
    ++usedAddrCounter_; // counter of addresses that are already using by tunnels

    mutex_.unlock();
    return result;
}

/**
 * @brief returnAddrToPool
 * Pushes back freed IP-address back<br>
 * to the pool of IP addresses
 * @param ip - ip address to push into queue
 */
void IPManager::returnAddrToPool(in_addr_t ip) {
    mutex_.lock();
        addrPoolPtr_->push(ip);
        --usedAddrCounter_;
    mutex_.unlock();
}

in_addr_t IPManager::getSockaddrIn() {
    return ipaddr_;
}

in_addr_t IPManager::genNextIp() {
    in_addr_t temp = ipaddr_;
    temp = ntohl(temp);
    temp = temp + 1;
    temp = htonl(temp);

    if(isInRange(temp))
        ipaddr_ = temp;
    else {
        return 0;
        // throw std::runtime_error("Cannot allocate new IP address!");
    }

    return ipaddr_;
}

/**
 * @brief IPManager::networkCapacity
 * @return Network maximum end-nodes
 */
uint32_t IPManager::networkCapacity() {
    in_addr_t lowerBound = ntohl((networkAddress_ & subnetMask_));
    in_addr_t upperBound = ((lowerBound | ntohl((~subnetMask_))));

    return upperBound - lowerBound;
}

/**
 * @brief isInRange checks, is given IPv4 address is in\r\n
 *        given network 'networkAddress' range\r\n
 * @param nextAddr - IPv4 address to check
 * @return true if an address is in network address range, otherwise false
 */
bool IPManager::isInRange(in_addr_t nextAddr) {
    in_addr_t lowerBound = ntohl((networkAddress_ & subnetMask_));
    in_addr_t upperBound = ((lowerBound | ntohl((~subnetMask_))));

    if(ntohl(nextAddr) >= lowerBound &&
       ntohl(nextAddr) <= upperBound) {
        return true;
    }

    return false;
}

std::string IPManager::ipToString() {
    return IPManager::ipToString(ipaddr_);
}

std::string IPManager::maskString() {
    return IPManager::ipToString(subnetMask_);
}

std::string IPManager::getNetworkString() {
    return IPManager::ipToString(networkAddress_);
}

std::string IPManager::ipToString(in_addr_t ip) {
    char buffer[INET_ADDRSTRLEN];
    return inet_ntop(AF_INET, &ip, buffer, INET_ADDRSTRLEN);
}
