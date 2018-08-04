#ifndef CLIENT_PARAMETERS_HPP
#define CLIENT_PARAMETERS_HPP

#include <string>

/**
 * @brief The ClientParameters\r\n
 * Structure that contains info used\r\n
 * for creating 'parametersToSend[]' array\r\n
 * This array will form the package with client settings,\r\n
 * such as IP, DNS etc. and will be sent to client.\r\n
 */
struct ClientParameters {
    std::string mtu;
    std::string virtualNetworkIp;
    std::string networkMask;
    std::string dnsIp;
    std::string routeIp;
    std::string routeMask;
    std::string physInterface;   // eth0, wlan0 etc..
    char  parametersToSend[1024];
};

#endif // CLIENT_PARAMETERS_HPP
