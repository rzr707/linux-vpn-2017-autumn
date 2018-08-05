#ifndef TUNNEL_HPP
#define TUNNEL_HPP

class WOLFSSL;

class Tunnel {
public:
    Tunnel();
    Tunnel(int tunDescriptor, WOLFSSL* wolfSsl);
    Tunnel& operator=(const Tunnel& that);
    int getTunDescriptor();
    WOLFSSL* getWolfSsl();
private:
    int      tunDescriptor_;
    WOLFSSL* wolfSsl_;
};

#endif // TUNNEL_HPP
