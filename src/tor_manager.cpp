#include "tor_manager.h"

#include "netutil.h"

#include <filesystem>
#include <fstream>
#include <shlobj.h>

namespace {

constexpr int kTorPort = 9050;
// First bootstrap downloads the microdesc consensus; on slow networks that
// can take minutes, so a hard fail must be generous. Later starts use the
// cache and are fast.
constexpr int kStartTimeoutMs = 900000;
// Target used to prove the proxy is genuinely usable (a full SOCKS5 CONNECT,
// which forces a circuit) rather than merely accepting connections.
constexpr const char* kProbeHost = "check.torproject.org";
constexpr unsigned short kProbePort = 80;

std::string Utf8(const std::wstring& w) {
    if (w.empty()) return {};
    // Pass w.size() (not -1): -1 includes the terminating NUL, which would
    // truncate every path built from this string.
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

std::string ExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return Utf8(std::filesystem::path(buf).parent_path().wstring());
}

std::string FindTorDir() {
    std::filesystem::path p = ExeDir();
    for (int i = 0; i < 6 && !p.empty(); ++i) {
        std::filesystem::path candidate = p / "tor";
        if (std::filesystem::exists(candidate / "tor.exe")) return candidate.string();
        p = p.parent_path();
    }
    return "";
}

}  // namespace

TorManager::TorManager() : torDir_(FindTorDir()) {
    wchar_t appdata[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appdata) == S_OK) {
        dataDir_ = Utf8(appdata) + "\\Rezo\\tor";
    } else {
        dataDir_ = ExeDir() + "\\tor-data";
    }
    std::filesystem::create_directories(dataDir_);
}

TorManager::~TorManager() { Stop(); }

bool TorManager::WriteTorrc() {
    if (torDir_.empty()) {
        lastError_ = "tor\\tor.exe not found";
        return false;
    }
    // torrc treats '\' as an escape inside quoted strings; forward slashes
    // avoid that entirely. GeoIP6File was removed in tor 0.4.9 (merged into
    // geoip). Log notice file is omitted: tor 0.4.9.11 fails to open any log
    // file path on Windows (EINVAL), which aborts startup.
    auto fs = [](const std::string& p) {
        std::string s = p;
        for (char& c : s) if (c == '\\') c = '/';
        return s;
    };
    std::ofstream f(dataDir_ + "\\torrc");
    if (!f) {
        lastError_ = "cannot write torrc";
        return false;
    }
    f << "SocksPort 9050\n"
      << "DataDirectory \"" << fs(dataDir_) << "\"\n"
      // This VM's disk is slow; keep tor's state in RAM.
      << "AvoidDiskWrites 1\n";
    if (std::filesystem::exists(torDir_ + "\\geoip"))
        f << "GeoIPFile \"" << fs(torDir_) << "/geoip\"\n";
    return true;
}

bool TorManager::SpawnTor() {
    if (!WriteTorrc()) return false;
    std::string cmdline = "\"" + torDir_ + "\\tor.exe\" -f \"" + dataDir_ + "\\torrc\"";
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        lastError_ = "CreateProcess failed: " + std::to_string(GetLastError());
        return false;
    }
    CloseHandle(pi.hThread);
    process_.store(pi.hProcess);
    return true;
}

void TorManager::Cleanup() {
    stopFlag_.store(true);
    HANDLE proc = process_.exchange(nullptr);
    if (proc) {
        TerminateProcess(proc, 0);
        CloseHandle(proc);
    }
    if (watchThread_) {
        WaitForSingleObject(watchThread_, 3000);
        CloseHandle(watchThread_);
        watchThread_ = nullptr;
    }
    if (startThread_) {
        // Skip the join when called from the start thread itself: waiting on
        // your own thread handle stalls the full timeout every start/retry.
        if (GetCurrentThreadId() != startThreadId_) {
            WaitForSingleObject(startThread_, 3000);
            CloseHandle(startThread_);
            startThread_ = nullptr;
        }
    }
    stopFlag_.store(false);
}

bool TorManager::StartBlocking() {
    Cleanup();
    state_.store(TorState::Starting);
    lastError_.clear();
    // Monitor runs in every path (spawn and adoption): with an adopted
    // daemon there is no process handle, so periodic probing is the only way
    // to notice it dying.
    watchThread_ = CreateThread(nullptr, 0, RunWatchdog, this, 0, nullptr);
    // A daemon left over from a hard-killed session (same data dir, same
    // port) may already be serving — adopt it instead of spawning a second
    // tor that dies on the data-dir lock. The lock file fingerprints a daemon
    // running our config; a foreign SOCKS on 9050 is not adopted. Readiness
    // (circuit to the probe host) is required, not just a listening port.
    if (socksReady("127.0.0.1", kTorPort, kProbeHost, kProbePort) &&
        std::filesystem::exists(dataDir_ + "\\lock")) {
        state_.store(TorState::Connected);
        return true;
    }
    if (!SpawnTor()) {
        state_.store(TorState::Blocked);
        return false;
    }
    // Wait for a real circuit, not just the port: first page load pays the
    // circuit build anyway, so front-load it into startup.
    bool ok = waitForSocks("127.0.0.1", kTorPort, kProbeHost, kProbePort, kStartTimeoutMs);
    // ok can be stale: our spawn may have died right after the probe, or lost
    // the data-dir lock to a slow stale daemon that only now became usable.
    // Probe before claiming Connected.
    bool up = ok && socksReady("127.0.0.1", kTorPort, kProbeHost, kProbePort);
    state_.store(up ? TorState::Connected : TorState::Blocked);
    if (!up) {
        // If our spawn already exited, the watchdog wrote the accurate reason.
        HANDLE proc = process_.load();
        if (!proc || WaitForSingleObject(proc, 0) != WAIT_OBJECT_0)
            lastError_ = "tor SOCKS proxy not ready";
    }
    return up;
}

void TorManager::StartAsync() {
    TorState s = state_.load();
    if (s == TorState::Starting || s == TorState::Connected) return;
    if (startThread_) {
        WaitForSingleObject(startThread_, 3000);
        CloseHandle(startThread_);
        startThread_ = nullptr;
    }
    startThread_ = CreateThread(nullptr, 0, RunStart, this, 0, nullptr);
    startThreadId_ = GetThreadId(startThread_);
}

DWORD WINAPI TorManager::RunStart(LPVOID param) {
    reinterpret_cast<TorManager*>(param)->StartBlocking();
    return 0;
}

void TorManager::Stop() {
    Cleanup();
    state_.store(TorState::Disconnected);
}

DWORD WINAPI TorManager::RunWatchdog(LPVOID param) {
    TorManager* self = reinterpret_cast<TorManager*>(param);
    int ticks = 0;
    while (!self->stopFlag_.load()) {
        // Reload each iteration: the thread starts before SpawnTor stores the
        // handle, so loading once could miss it and leave the process-exit
        // kill switch permanently disengaged.
        HANDLE proc = self->process_.load();
        if (proc) {
            DWORD rc = WaitForSingleObject(proc, 1000);
            if (rc == WAIT_OBJECT_0) {
                TorState s = self->state_.load();
                if (s == TorState::Connected || s == TorState::Starting) {
                    self->state_.store(TorState::Blocked);
                    self->lastError_ = "tor process exited";
                }
                break;
            }
        } else {
            Sleep(1000);
        }
        // Every ~5s, re-probe the proxy: catches an adopted daemon dying (no
        // process handle to wait on) and recovers Connected when Tor returns.
        if (++ticks % 5 == 0 && !self->stopFlag_.load()) {
            TorState s = self->state_.load();
            if (s == TorState::Starting && proc &&
                WaitForSingleObject(proc, 0) == WAIT_OBJECT_0) {
                // Spawn died during bootstrap: don't make the UI wait out the
                // full start timeout before showing Blocked.
                self->state_.store(TorState::Blocked);
                self->lastError_ = "tor process exited";
            } else if (s == TorState::Connected) {
                // Full circuit probe, not a bare connect: "something listens
                // on 9050" (a foreign SOCKS, a dead-socket leftover) must not
                // count as Tor being up.
                if (!socksReady("127.0.0.1", kTorPort, kProbeHost, kProbePort)) {
                    self->state_.store(TorState::Blocked);
                    self->lastError_ = "tor unreachable";
                }
            } else if (s == TorState::Blocked) {
                if (socksReady("127.0.0.1", kTorPort, kProbeHost, kProbePort)) {
                    self->state_.store(TorState::Connected);
                    self->lastError_.clear();
                }
            }
        }
    }
    return 0;
}