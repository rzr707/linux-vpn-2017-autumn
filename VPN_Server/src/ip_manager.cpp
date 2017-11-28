#include "ip_manager.hpp"

/**
 * @brief IPManager constructor
 * @param ipAndMask - contains ip address and mask bits
 *  like "x.x.x.x/y" where y is bit count (from 0 to 32)
 */
IPManager::IPManager(std::string ipAndMask, size_t poolInitSize) {
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


IPManager::~IPManager() {
    delete addrPoolPtr;
}

/**
 * @brief getAddrFromPool
 * @return IP address from the pool of addresses
 */
in_addr_t IPManager::getAddrFromPool() {

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
void IPManager::returnAddrToPool(in_addr_t ip) {
    mutex.lock();
        addrPoolPtr->push(ip);
        --usedAddrCounter;
    mutex.unlock();
}

in_addr_t IPManager::getSockaddrIn() {
    return ipaddr;
}

in_addr_t IPManager::genNextIp() {
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

/**
 * @brief IPManager::networkCapacity
 * @return Network maximum end-nodes
 */
uint32_t IPManager::networkCapacity() {
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
bool IPManager::isInRange(in_addr_t nextAddr) {
    in_addr_t lowerBound = ntohl((networkAddress & netmask));
    in_addr_t upperBound = ((lowerBound | ntohl((~netmask))));

    if(ntohl(nextAddr) >= lowerBound &&
       ntohl(nextAddr) <= upperBound) {
        return true;
    }

    return false;
}

std::string IPManager::getIpString() {
    return IPManager::getIpString(ipaddr);
}

std::string IPManager::maskString() {
    return IPManager::getIpString(netmask);
}

std::string IPManager::getNetworkString() {
    return IPManager::getIpString(networkAddress);
}

std::string IPManager::getIpString(in_addr_t ip) {
    char buffer[INET_ADDRSTRLEN];
    return inet_ntop(AF_INET, &ip, buffer, INET_ADDRSTRLEN);
}
