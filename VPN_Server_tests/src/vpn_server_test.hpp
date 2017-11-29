#ifndef VPN_SERVER_TEST_HPP
#define VPN_SERVER_TEST_HPP

#include <gtest/gtest.h>
#include <../VPN_Server/src/tunnel_mgr.cpp>
#include <../VPN_Server/src/vpn_server.cpp>

TEST(VpnServerCorrectSubmask, CorrectSubmask) {
    int argc = 2;
    char* argv[] = { "", "8000" };
    VPNServer server(argc, argv);

    ASSERT_TRUE(server.correctSubmask("0"));
    ASSERT_TRUE(server.correctSubmask("5"));
    ASSERT_TRUE(server.correctSubmask("30"));
    ASSERT_TRUE(server.correctSubmask("32"));
    ASSERT_TRUE(server.correctSubmask("32"));
    ASSERT_TRUE(server.correctSubmask("16"));
}

TEST(VpnServerCorrectSubmask, IncorrectSubmask) {
    int argc = 2;
    char* argv[] = { "", "8000" };
    VPNServer server(argc, argv);

    ASSERT_FALSE(server.correctSubmask("-5"));
    ASSERT_FALSE(server.correctSubmask("500"));
    ASSERT_FALSE(server.correctSubmask("100"));
    ASSERT_FALSE(server.correctSubmask("250"));
    ASSERT_FALSE(server.correctSubmask("192.168.100.100"));
}

TEST(VpnServerIpCorrectness, CorrectIpv4address) {
    int argc = 2;
    char* argv[] = { "", "8000" };
    VPNServer server(argc, argv);

    ASSERT_TRUE(server.correctIp("192.168.0.0"));
    ASSERT_TRUE(server.correctIp("192.168.255.0"));
    ASSERT_TRUE(server.correctIp("10.0.0.0"));
    ASSERT_TRUE(server.correctIp("11.12.13.14"));
    ASSERT_TRUE(server.correctIp("14.13.12.11"));
    ASSERT_TRUE(server.correctIp("200.13.12.11"));
}

TEST(VpnServerIpCorrectness, IncorrectIpv4address) {
    int argc = 2;
    char* argv[] = { "", "8000" };
    VPNServer server(argc, argv);

    ASSERT_FALSE(server.correctIp("1920.168.0.0"));
    ASSERT_FALSE(server.correctIp("192.-1.255.0"));
    ASSERT_FALSE(server.correctIp("wrongip"));
    ASSERT_FALSE(server.correctIp(""));
    ASSERT_FALSE(server.correctIp("14.512.12.11"));
    ASSERT_FALSE(server.correctIp("0.13.12.257"));
}

TEST(VpnServerArguments, WrongPortException) {
    int argc = 2;
    char* argv[] = { "", "66666" };

    ASSERT_THROW(new VPNServer(argc, argv), std::invalid_argument);
}

TEST(VpnServerPortArgument, StandartArgumentNoExceptionThrown) {
    int argc = 2;
    char* argv[] = { "", "8000" };

    ASSERT_NO_THROW(new VPNServer(argc, argv));
}

TEST(VpnServerIpAddrArgument, InvalidIpExceptionThrown) {
    int argc = 5;
    char* argv[] = { "", "8000", "-a", "999.888.777.666", "8" };

    ASSERT_THROW(new VPNServer(argc, argv), std::invalid_argument);
}

TEST(VpnServerIpAddrArgument, ValidIpNoExceptionThrown) {
    int argc = 5;
    char* argv[] = { "", "8000", "-a", "192.168.1.0", "8" };

    ASSERT_NO_THROW(new VPNServer(argc, argv));
}

TEST(VpnServerIpMaskArgument, InvalidMaskExceptionThrown) {
    int argc = 5;
    char* argv[] = { "", "8000", "-a", "192.168.1.0", "333" };

    ASSERT_THROW(new VPNServer(argc, argv), std::invalid_argument);
}

TEST(VpnServerIpMaskArgument, ValidMaskNoExceptionThrown) {
    int argc = 5;
    char* argv[] = { "", "8000", "-a", "192.168.1.0", "16" };

    ASSERT_NO_THROW(new VPNServer(argc, argv));
}

TEST(VpnServerNetworkIfaceArgument, NoSuchIfaceException) {
    int argc = 3;
    char* argv[] = { "", "-i", "etj0" };

    ASSERT_THROW(new VPNServer(argc, argv), std::invalid_argument);
}


#endif // VPN_SERVER_TEST_HPP
