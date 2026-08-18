#include "netutil.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstring>
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

bool socksReady(const char* proxyHost, unsigned short proxyPort,
                const char* targetHost, unsigned short targetPort) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(proxyPort);
    bool fail = inet_pton(AF_INET, proxyHost, &addr.sin_addr) != 1 ||
                connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0;
    if (!fail) {
        // Circuit building can take seconds; don't let one attempt hang.
        DWORD timeout = 3000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
                   sizeof(timeout));
        // Greeting: version 5, one method (no auth).
        static const uint8_t hello[] = {0x05, 0x01, 0x00};
        uint8_t resp[2];
        fail = send(s, reinterpret_cast<const char*>(hello), sizeof(hello), 0) !=
                   static_cast<int>(sizeof(hello)) ||
               recv(s, reinterpret_cast<char*>(resp), 2, 0) != 2 ||
               resp[0] != 0x05 || resp[1] != 0x00;
        if (!fail) {
            // CONNECT via domain name (remote DNS, like browser traffic).
            size_t len = std::strlen(targetHost);
            fail = len > 255;
            if (!fail) {
                uint8_t req[4 + 1 + 255 + 2];
                req[0] = 0x05; req[1] = 0x01; req[2] = 0x00; req[3] = 0x03;
                req[4] = static_cast<uint8_t>(len);
                std::memcpy(req + 5, targetHost, len);
                uint16_t tp = htons(targetPort);
                std::memcpy(req + 5 + len, &tp, 2);
                uint8_t head[4];
                int reqLen = 5 + static_cast<int>(len) + 2;
                fail = send(s, reinterpret_cast<const char*>(req), reqLen, 0) != reqLen ||
                       recv(s, reinterpret_cast<char*>(head), 4, 0) != 4 ||
                       head[0] != 0x05 || head[1] != 0x00;
            }
        }
    }
    closesocket(s);
    return !fail;
}

bool waitForSocks(const char* proxyHost, unsigned short proxyPort,
                  const char* targetHost, unsigned short targetPort, int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    do {
        if (socksReady(proxyHost, proxyPort, targetHost, targetPort)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}