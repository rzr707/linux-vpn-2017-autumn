#ifndef IP_MANAGER_TEST_HPP
#define IP_MANAGER_TEST_HPP

#include "../../VPN_Server/src/ip_manager.hpp"
#include <gtest/gtest.h>

class IPManagerTest : public testing::Test {
protected:
    void SetUp() {
        mgr = new IPManager("10.0.0.0/8");
    }
    void TearDown() {
        delete mgr;
    }
    IPManager* mgr;
};

TEST_F(IPManagerTest, TestGetNetworkIpString) {
    ASSERT_EQ("10.0.0.0", mgr->getNetworkString());
}

TEST_F(IPManagerTest, TestGetNextIpAddress) {
    mgr->nextIp4Address();
    ASSERT_EQ("10.0.0.1", mgr->getIpString());
}

TEST_F(IPManagerTest, TestNetworkCapacity) {
    ASSERT_EQ(16777215, mgr->networkCapacity());
}

#endif // IP_MANAGER_TEST_HPP
