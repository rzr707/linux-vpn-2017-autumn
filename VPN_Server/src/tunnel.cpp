#include "tunnel.hpp"

#include "wolfssl/ssl.h"

Tunnel::Tunnel()
    : tunDescriptor_(-1)
    , wolfSsl_(nullptr)
{
}

Tunnel::Tunnel(int tunDescriptor, WOLFSSL* wolfSsl)
    : tunDescriptor_(tunDescriptor)
    , wolfSsl_(wolfSsl)
{
}

Tunnel& Tunnel::operator=(const Tunnel& that) {
    tunDescriptor_ = that.tunDescriptor_;
    wolfSsl_       = that.wolfSsl_;

    return *this;
}

WOLFSSL* Tunnel::getWolfSsl() {
    return wolfSsl_;
}

int Tunnel::getTunDescriptor() {
    return tunDescriptor_;
}




