#include "tor_manager.h"

#include "netutil.h"

#include <filesystem>
#include <fstream>
#include <shlobj.h>

namespace {

constexpr int kTorPort = 9050;
constexpr int kStartTimeoutMs = 90000;

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
      << "DataDirectory \"" << fs(dataDir_) << "\"\n";
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
        WaitForSingleObject(startThread_, 3000);
        CloseHandle(startThread_);
        startThread_ = nullptr;
    }
    stopFlag_.store(false);
}

bool TorManager::StartBlocking() {
    Cleanup();
    state_.store(TorState::Starting);
    lastError_.clear();
    if (!SpawnTor()) {
        state_.store(TorState::Blocked);
        return false;
    }
    watchThread_ = CreateThread(nullptr, 0, RunWatchdog, this, 0, nullptr);
    bool ok = waitForPort("127.0.0.1", kTorPort, kStartTimeoutMs);
    state_.store(ok ? TorState::Connected : TorState::Blocked);
    if (!ok) lastError_ = "tor did not open SOCKS port 9050";
    return ok;
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
    HANDLE proc = self->process_.load();
    while (!self->stopFlag_.load()) {
        DWORD rc = WaitForSingleObject(proc, 1000);
        if (rc == WAIT_OBJECT_0) {
            TorState s = self->state_.load();
            if (s == TorState::Connected || s == TorState::Starting) {
                self->state_.store(TorState::Blocked);
                self->lastError_ = "tor process exited";
            }
            break;
        }
    }
    return 0;
}