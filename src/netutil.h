#pragma once

bool tryConnect(const char* host, unsigned short port);
bool waitForPort(const char* host, unsigned short port, int timeoutMs);