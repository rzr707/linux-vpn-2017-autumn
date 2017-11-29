#include "tunnel_mgr_test.hpp"
#include "ip_manager_test.hpp"
#include "vpn_server_test.hpp"

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

