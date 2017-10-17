//#define DEBUG

#ifndef IP_MANAGER_HPP
#define IP_MANAGER_HPP

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdexcept>

using namespace std;

/**
 * @brief The IPManager class
 *        Contains IPv4 network info, such as\r\n
 *        network address, network mask and last\r\n
 *        generated client IP address.\r\n
 */
class IPManager {
private:
    in_addr_t networkAddress;
    in_addr_t ipaddr;
    in_addr_t netmask; // маска подсети
public:
    /* Forbid copy ctor and standart ctor: */
    IPManager() = delete;
    IPManager(IPManager& that) = delete;

    /**
     * @brief IPManager constructor
     * @param ipAndMask - contains ip address and mask bits
     *  like "x.x.x.x/y" where y is bit count (from 0 to 32)
     */
    explicit IPManager(std::string ipAndMask) {

        size_t slashPos = ipAndMask.find('/');

        if(slashPos == std::string::npos ||
           ipAndMask.at(slashPos) == ipAndMask.at(ipAndMask.length() - 1)) {
            cerr << "Bit mask was not found. Using default: 255.255.255.0" << endl;
            ipaddr = inet_addr(ipAndMask.c_str());
            networkAddress = ipaddr;
            netmask = inet_addr("255.255.255.0");
        } else {
            std::string ip = ipAndMask.substr(0, slashPos);
            uint32_t networkMaskBitCount = atoi(ipAndMask.substr(slashPos + 1).c_str());

#ifdef DEBUG
            cout << "slashpos is " << slashPos
                 << " ip is: " << ip.c_str()
                 << ", networkMaskBitCount is: " << networkMaskBitCount
                 << endl;
#endif // DEBUG

            ipaddr         = inet_addr(ip.c_str());
            networkAddress = ipaddr;

            in_addr_t tempmask = (0xffffffff >> (32 - networkMaskBitCount )) << (32 - networkMaskBitCount);
            netmask = htonl(tempmask);
        }

    }

    in_addr_t getSockaddrIn() {
        return ipaddr;
    }

    in_addr_t nextIp4Address() {
        in_addr_t temp = ipaddr;
        temp = ntohl(temp);
        temp = temp + 1;
        temp = htonl(temp);

        if(isInRange(temp))
            ipaddr = temp;
        else {
            cerr << "Error: no free ip addresses in the pool" << endl;
        }

        return ipaddr;
    }

    // ntohl
    in_addr_t ipToUint(std::string ip) {
        int a, b, c, d;
        in_addr_t addr = 0;

        if(sscanf(ip.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) != 4) {
            throw runtime_error("ipToUint: error");
        }

        addr =  a << 24;
        addr |= b << 16;
        addr |= c << 8;
        addr |= d;
        return addr;
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

#ifdef DEBUG
        cout << "LowerBound string: " << IPManager::getIpString(htonl(lowerBound)) << ", int: " << lowerBound
             << " UpperBound: " << IPManager::getIpString(htonl(upperBound)) << ", int: " << upperBound
             << " address to check: " << IPManager::getIpString(nextAddr) << ", int: " << nextAddr
             << endl;
#endif // DEBUG

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
