#pragma once

#include <atomic>
#include <string>
#include <windows.h>

#include "tor_state.h"

class TorManager {
public:
    TorManager();
    ~TorManager();

    bool StartBlocking();   // spawn tor, wait for port, start watchdog; true if Connected
    void StartAsync();      // StartBlocking on a background thread
    void Stop();            // terminate tor, join threads, state -> Disconnected
    TorState State() const { return state_.load(); }
    std::string LastError() const { return lastError_; }
    std::string TorDir() const { return torDir_; }

private:
    static DWORD WINAPI RunStart(LPVOID param);
    static DWORD WINAPI RunWatchdog(LPVOID param);
    bool SpawnTor();
    bool WriteTorrc();
    void Cleanup();  // kill process, join watchdog/start threads

    std::atomic<TorState> state_{TorState::Disconnected};
    std::atomic<HANDLE> process_{nullptr};
    std::atomic<bool> stopFlag_{false};
    std::string lastError_;
    std::string torDir_;
    std::string dataDir_;
    HANDLE startThread_ = nullptr;
    HANDLE watchThread_ = nullptr;
};