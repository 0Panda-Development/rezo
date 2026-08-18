#include "netutil.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <thread>

bool tryConnect(const char* host, unsigned short port) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        closesocket(s);
        return false;
    }
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
    int rc = connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    bool ok = rc == 0;
    if (!ok && WSAGetLastError() == WSAEWOULDBLOCK) {
        fd_set w;
        FD_ZERO(&w);
        FD_SET(s, &w);
        timeval tv{};
        tv.tv_usec = 250000;
        ok = select(0, nullptr, &w, nullptr, &tv) == 1;
    }
    closesocket(s);
    return ok;
}

bool waitForPort(const char* host, unsigned short port, int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    do {
        if (tryConnect(host, port)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}