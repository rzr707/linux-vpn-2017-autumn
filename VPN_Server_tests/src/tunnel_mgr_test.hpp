#ifndef TUNNEL_MGR_TEST_HPP
#define TUNNEL_MGR_TEST_HPP

#include "../../VPN_Server/src/tunnel_mgr.hpp"
#include <gtest/gtest.h>
#include <math.h>

class TunnelManagerTest : public testing::Test {
protected:
    void SetUp() {
        mgr = new TunnelManager("../TunnelManager/data/pid_list.dat",
                                "../TunnelManager/data/tun_list.dat");
    }
    void TearDown() {
        delete mgr;
    }
    TunnelManager* mgr;
};

TEST_F(TunnelManagerTest, TestGetFilename1) {
    ASSERT_EQ("../TunnelManager/data/tun_list.dat", mgr->getTunListFilename());
}

TEST_F(TunnelManagerTest, TestGetFilename2) {
    ASSERT_EQ("../TunnelManager/data/pid_list.dat", mgr->getPidListFilename());
}

TEST_F(TunnelManagerTest, TestWriteToFile) {
    ASSERT_NO_THROW(mgr->write(mgr->getTunListFilename(), "tun0"));
    ASSERT_THROW(mgr->write("../VPN_Server/data/readonly_test.dat", "tun0"), std::runtime_error);
}

TEST_F(TunnelManagerTest, TestReadFromFile) {
    ASSERT_NO_THROW(mgr->write(mgr->getTunListFilename(), "tun0"));
    ASSERT_NO_THROW(mgr->write(mgr->getTunListFilename(), "tun1"));
    ASSERT_NO_THROW(mgr->write(mgr->getTunListFilename(), "tun2"));

    std::list<std::string> list =
            mgr->getInfoList(mgr->getTunListFilename());
    ASSERT_EQ("tun0", list.front());
    list.pop_front();
    ASSERT_EQ("tun1", list.front());
    list.pop_front();
    ASSERT_EQ("tun2", list.front());
}

#endif // TUNNEL_MGR_TEST_HPP
