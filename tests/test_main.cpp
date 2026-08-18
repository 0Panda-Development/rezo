#include <cstdio>
#include <filesystem>
#include <winsock2.h>

#include "netutil.h"
#include "tor_manager.h"
#include "tor_state.h"

static int failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FAIL: %s (line %d)\n", #cond, __LINE__);          \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

static void TestStateLogic() {
    CHECK(shouldBlockRequest(TorState::Disconnected));
    CHECK(shouldBlockRequest(TorState::Starting));
    CHECK(!shouldBlockRequest(TorState::Connected));
    CHECK(shouldBlockRequest(TorState::Blocked));
    CHECK(torStateName(TorState::Connected)[0] != '\0');
    CHECK(torStateName(TorState::Blocked)[0] != '\0');
}

static void TestWaitForPort() {
    WSADATA wsa;
    CHECK(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);

    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    CHECK(srv != INVALID_SOCKET);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    CHECK(bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    CHECK(listen(srv, 1) == 0);
    int len = sizeof(addr);
    CHECK(getsockname(srv, reinterpret_cast<sockaddr*>(&addr), &len) == 0);
    unsigned short port = ntohs(addr.sin_port);

    CHECK(waitForPort("127.0.0.1", port, 3000));      // open port -> true
    CHECK(!waitForPort("127.0.0.1", port + 1, 500));  // closed port -> false
    closesocket(srv);
}

static void TestTorManager() {
    TorManager tm;
    if (std::filesystem::exists(tm.TorDir() + "\\tor.exe")) {
        CHECK(tm.StartBlocking());
        CHECK(tm.State() == TorState::Connected);
        tm.Stop();
        CHECK(tm.State() == TorState::Disconnected);
    } else {
        std::printf("SKIP: tor\\tor.exe not present\n");
    }
}

int main() {
    TestStateLogic();
    TestWaitForPort();
    TestTorManager();
    if (failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d FAILURE(S)\n", failures);
    return 1;
}