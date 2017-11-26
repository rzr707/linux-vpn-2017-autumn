#ifndef TUNNEL_MGR_TEST_HPP
#define TUNNEL_MGR_TEST_HPP

#include "../../VPN_Server/src/vpn_server.hpp"
#include <gtest/gtest.h>
#include <math.h>


class TunnelManagerTest : public testing::Test {
protected:
    void SetUp() {
        tunMgr = new TunnelManager;

    }
    void TearDown() {
        delete tunMgr;
    }
    TunnelManager* tunMgr;
};

TEST_F(TunnelManagerTest, TestCreateUnixTunnel) {
}


#endif // TUNNEL_MGR_TEST_HPP
