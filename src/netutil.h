#pragma once

#include <cstddef>

bool tryConnect(const char* host, unsigned short port);
bool waitForPort(const char* host, unsigned short port, int timeoutMs);

// Full SOCKS5 CONNECT through (proxyHost, proxyPort) to targetHost:targetPort
// (remote DNS, like the browser's own routing). True only when a circuit to
// the target actually exists — i.e. the proxy is genuinely usable, not just
// accepting connections.
bool socksReady(const char* proxyHost, unsigned short proxyPort,
                const char* targetHost, unsigned short targetPort);
bool waitForSocks(const char* proxyHost, unsigned short proxyPort,
                  const char* targetHost, unsigned short targetPort, int timeoutMs);