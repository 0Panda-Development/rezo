# Rezo Browser Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Rezo — a Windows x64 CEF browser that routes every request through a bundled Tor daemon with a kill switch, plus a minimal gaming-themed UI (tabs, address bar, quick-access new-tab page).

**Architecture:** One executable (Rezo.exe, C++17 + CEF Views API) spawns tor.exe (official Tor expert bundle) as a child process with a generated torrc (SOCKS5 on 127.0.0.1:9050). All Chromium traffic goes through the SOCKS5 proxy; a watchdog detects Tor death and a kill switch in `OnBeforeResourceLoad`/`OnBeforeBrowse` blocks navigation until Tor is back. Privacy hardening via command-line switches. New-tab page and blocked page are served by a custom `rezo://newtab` scheme handler that reads a plain-text config (`quickaccess.txt`) next to the exe.

**Tech Stack:** C++17, CEF (Chromium Embedded Framework, latest stable win64 standard distribution), CMake >= 3.21, Visual Studio 2022 (C++ Desktop workload), Win32 API, Tor expert bundle (win64). No other dependencies.

## Global Constraints

- Windows 10/11 x64 only. Build with VS 2022 + CMake. C++17 required.
- CEF distribution lives in-tree at `third_party/cef/` — **downloaded, never committed**.
- Tor expert bundle lives at `tor/` (tor.exe, geoip, geoip6) — **downloaded, never committed**.
- Every internet request must exit via Tor. **Never** fall back to direct internet.
- Kill-switch rule: traffic is allowed only when Tor state == `Connected`; local URLs (`rezo://`, `file:`, `data:`) are always allowed.
- Tests: plain C++ `assert`-based `main()` test exe, no framework, run via `ctest`.
- All user-facing copy in English.
- New-tab URL scheme is `rezo://newtab/`; blocked page is `rezo://newtab/blocked`.

## File Structure

```
E:\Rezo\
├── CMakeLists.txt            # CEF target + rezo_tests target
├── README.md                 # build/run instructions
├── .gitignore                # build/, dist/, third_party/, tor/, .vs/
├── src\
│   ├── main.cpp              # entry: CEF init, global g_tor, message loop
│   ├── app.h / app.cpp       # RezoApp (CefApp): command-line switches, OnContextInitialized
│   ├── client.h / client.cpp # RezoClient: kill switch, popup policy, titles/address
│   ├── window.h / window.cpp # RezoWindow + Chrome delegate: tabs, bar, address, status
│   ├── tor_state.h           # TorState enum + shouldBlockRequest() (pure, header-only)
│   ├── tor_manager.h/.cpp    # spawn tor, watchdog, state machine (no CEF deps)
│   ├── netutil.h/.cpp        # waitForPort() (WinSock)
│   ├── newtab.h / newtab.cpp # rezo://newtab scheme handler + HTML builders
│   └── resources\
│       ├── newtab.html       # template with {{TILES}} and {{STATUS}} placeholders
│       ├── blocked.html      # Tor-down page with retry link
│       └── quickaccess.txt   # tile config: "Name<TAB>URL" per line, # comments
├── tests\
│   └── test_main.cpp         # rezo_tests: tor_state + netutil + TorManager integration
├── scripts\
│   └── package.ps1           # assemble dist\Rezo\ (portable)
└── docs\superpowers\specs\2026-08-18-rezo-browser-design.md
```

Key interfaces (defined once here, used by later tasks):

- `enum class TorState { Disconnected, Starting, Connected, Blocked };` — `src/tor_state.h`
- `bool shouldBlockRequest(TorState s);` — `true` unless `Connected`
- `const char* torStateName(TorState s);`
- `bool waitForPort(const char* host, unsigned short port, int timeoutMs);` — `src/netutil.h`
- `class TorManager` — `bool StartBlocking(); void StartAsync(); void Stop(); TorState State() const; std::string LastError() const; std::string TorDir() const;` — `src/tor_manager.h`
- `extern TorManager g_tor;` — defined in `src/main.cpp`
- `class RezoClient` — `CefClient` + `CefRequestHandler` + `CefLifeSpanHandler` + `CefLoadHandler` + `CefDisplayHandler`; constructor takes `RezoWindow*`
- `class RezoWindow` — `void Create(const std::string& startUrl); void NavigateTo(const std::string& input); void OpenTab(const std::string& url); void CloseTab(CefRefPtr<CefBrowser> browser); void SelectTab(int index); void SetTabTitle(CefRefPtr<CefBrowser> browser, const CefString& title); void SetAddress(const std::string& url); void UpdateTorStatus(); bool IsClosing() const; void DestroyWindow();` — `src/window.h`
- `class NewTabFactory : public CefSchemeHandlerFactory` — registered for scheme `"rezo"`, domain `"newtab"`

---

### Task 1: Toolchain check + project skeleton

**Files:**
- Create: `CMakeLists.txt`, `src/main.cpp`, `tests/test_main.cpp`, `.gitignore`, `README.md`

**Interfaces:**
- Consumes: nothing.
- Produces: buildable project; `rezo_tests` ctest target; conventions all later tasks follow.

- [ ] **Step 1: Verify toolchain**

Open a **VS 2022 x64 developer terminal** (start menu: "x64 Native Tools Command Prompt for VS 2022", or any PowerShell after running `VsDevCmd.bat`). Run:

```powershell
cmake --version    # need >= 3.21
cl                  # should print "Microsoft (R) C/C++ Optimizing Compiler"
```

If `cmake` is missing: `winget install Kitware.CMake`. If `cl` is missing: install the "Desktop development with C++" workload in Visual Studio Installer.

- [ ] **Step 2: Write the skeleton files**

`.gitignore`:

```gitignore
build/
dist/
third_party/
tor/
.vs/
CMakeFiles/
cmake_install.cmake
CMakeCache.txt
```

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.21)
project(Rezo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(rezo src/main.cpp)

enable_testing()
add_executable(rezo_tests tests/test_main.cpp)
add_test(NAME rezo_tests COMMAND rezo_tests)
```

`src/main.cpp`:

```cpp
#include <cstdio>

int main() {
    std::printf("Rezo skeleton OK\n");
    return 0;
}
```

`tests/test_main.cpp`:

```cpp
#include <cstdio>

int main() {
    std::printf("rezo_tests skeleton OK\n");
    return 0;
}
```

`README.md`:

```markdown
# Rezo

Privacy-focused gaming browser for Windows. All traffic routed through the Tor
network. Built with CEF (Chromium) in C++.

## Build

1. Install Visual Studio 2022 with the "Desktop development with C++" workload,
   and CMake >= 3.21.
2. Open a VS 2022 x64 developer terminal in this folder.
3. `cmake -S . -B build -G "Visual Studio 17 2022" -A x64`
4. `cmake --build build --config Release`

The exe lands in `build\bin\Release\` (CEF may use `build\Release\` — check
both). See the spec in `docs\superpowers\specs\` for the design.
```

- [ ] **Step 3: Build and run**

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Expected: both targets build; `ctest` passes; running the exe prints `Rezo skeleton OK`.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt src/main.cpp tests/test_main.cpp .gitignore README.md
git commit -m "task1: project skeleton with ctest harness"
```

---

### Task 2: Download CEF and open a window

**Files:**
- Create: `third_party/cef/` (downloaded, gitignored)
- Modify: `CMakeLists.txt`, `src/main.cpp`

**Interfaces:**
- Consumes: toolchain from Task 1.
- Produces: `Rezo.exe` opens a 1280x800 window rendering a local `data:` page. Establishes the in-tree CEF integration pattern every later task builds on.

- [ ] **Step 1: Download and extract CEF**

Go to https://cef-builds.spotifycdn.com/index.html#windows64 and note the
latest **stable** version (e.g. `138.0.xxxx`). Download its **Standard
Distribution** for Windows 64-bit (tar.bz2):

```powershell
# example; substitute the actual version from the index page
Invoke-WebRequest "https://cef-builds.spotifycdn.com/cef_builds/cef_binary_138.0.xxxx_windows64.tar.bz2" -OutFile "$env:TEMP\cef.tar.bz2"
tar -xf "$env:TEMP\cef.tar.bz2" -C "$env:TEMP"
Move-Item "$env:TEMP\cef_binary_138.0.xxxx_windows64" third_party\cef
```

Verify the extraction contains all of: `cmake/CMakeLists.txt`,
`include/cef_app.h`, `libcef_dll_wrapper/`, `Release/libcef.dll`,
`Release/resources.pak`. If `libcef_dll_wrapper/` is missing, download the
**Source Distribution** instead (it contains the wrapper too).

- [ ] **Step 2: Read the CEF CMake usage notes**

Read `third_party/cef/README.md` (or `third_party/cef/cmake/CMakeLists.txt`)
and confirm the `cef_add_target` invocation form in that version (older
versions take the target name positionally, newer versions use
`cef_add_target(TARGET name ...)`). The code below uses the modern form —
adjust if your version differs.

- [ ] **Step 3: Rewrite CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.21)
project(Rezo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef" CACHE PATH "CEF root")
add_subdirectory(${CEF_ROOT})

cef_add_target(TARGET rezo
    SOURCES
        src/main.cpp
    DEFINES
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

enable_testing()
add_executable(rezo_tests tests/test_main.cpp)
add_test(NAME rezo_tests COMMAND rezo_tests)
```

- [ ] **Step 4: Replace src/main.cpp with the CEF hello window**

```cpp
#include "include/cef_app.h"
#include "include/cef_base.h"

class RezoApp : public CefApp, public CefBrowserProcessHandler {
public:
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }

    void OnContextInitialized() override {
        CefBrowserHost::CreateBrowserSync(
            CefWindowInfo(), new DummyClient(), "data:text/html,<h1 style='font-family:sans-serif'>Rezo</h1>",
            CefBrowserSettings(), nullptr, nullptr);
    }
    IMPLEMENT_REFCOUNTING(RezoApp);

private:
    class DummyClient : public CefClient {
    public:
        IMPLEMENT_REFCOUNTING(DummyClient);
    };
};

int main(int argc, char* argv[]) {
    CefMainArgs args(argc, argv);
    CefRefPtr<RezoApp> app(new RezoApp);
    int code = CefExecuteProcess(args, app.get(), nullptr);
    if (code >= 0) return code;

    CefSettings settings;
    settings.no_sandbox = true;
    settings.log_severity = LOGSEVERITY_WARNING;
    if (!CefInitialize(args, settings, app.get(), nullptr)) return 1;

    CefRunMessageLoop();
    CefShutdown();
    return 0;
}
```

Note: default-constructed `CefWindowInfo()` creates a plain top-level native
window — fine for this milestone; the Views-based chrome replaces it in Task 6.

- [ ] **Step 5: Build and run**

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Run the exe from `build\bin\Release\rezo.exe` (or `build\Release\`).
Expected: a window showing "Rezo" in large text. Close it with Alt+F4; the
process should exit (if it lingers, kill it in Task Manager and note it —
Task 6 fixes exit behavior).

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src/main.cpp
git commit -m "task2: CEF integrated, hello window opens"
```

---

### Task 3: TorManager — spawn, wait, watchdog + unit tests

**Files:**
- Create: `src/tor_state.h`, `src/netutil.h`, `src/netutil.cpp`, `src/tor_manager.h`, `src/tor_manager.cpp`, `tor/` (downloaded, gitignored)
- Modify: `tests/test_main.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from earlier tasks (independent of CEF).
- Produces: `TorState`, `shouldBlockRequest()`, `torStateName()`, `waitForPort()`, `TorManager` (`StartBlocking`, `StartAsync`, `Stop`, `State`, `LastError`, `TorDir`). All later tasks consume these.

- [ ] **Step 1: Download the Tor expert bundle**

Go to https://www.torproject.org/download/tor/ → **Expert Bundle** → Windows
64-bit (a `tor-win64-<ver>.tar.gz` on `dist.torproject.org`). Extract and
copy into the repo:

```powershell
tar -xf "$env:TEMP\tor-win64-<ver>.tar.gz" -C "$env:TEMP"
mkdir tor
Copy-Item "$env:TEMP\tor-win64-<ver>\tor.exe" tor\
Copy-Item "$env:TEMP\tor-win64-<ver>\geoip" tor\
Copy-Item "$env:TEMP\tor-win64-<ver>\geoip6" tor\
```

Verify: `tor\tor.exe` exists.

- [ ] **Step 2: Write the failing test**

`tests/test_main.cpp` (full replacement):

```cpp
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
```

- [ ] **Step 3: Run test to verify it fails to compile**

```powershell
cmake --build build --config Release --target rezo_tests
```

Expected: compile error — `netutil.h`, `tor_manager.h`, `tor_state.h` don't exist yet.

- [ ] **Step 4: Write the implementation**

`src/tor_state.h`:

```cpp
#pragma once

enum class TorState { Disconnected, Starting, Connected, Blocked };

inline bool shouldBlockRequest(TorState s) {
    return s != TorState::Connected;
}

inline const char* torStateName(TorState s) {
    switch (s) {
        case TorState::Disconnected: return "Disconnected";
        case TorState::Starting:     return "Starting";
        case TorState::Connected:    return "Connected";
        case TorState::Blocked:      return "Blocked";
    }
    return "Unknown";
}
```

`src/netutil.h`:

```cpp
#pragma once

bool waitForPort(const char* host, unsigned short port, int timeoutMs);
```

`src/netutil.cpp`:

```cpp
#include "netutil.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <thread>

static bool tryConnect(const char* host, unsigned short port) {
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
    closesocket(s);
    return rc == 0;
}

bool waitForPort(const char* host, unsigned short port, int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    do {
        if (tryConnect(host, port)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}
```

`src/tor_manager.h`:

```cpp
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
```

`src/tor_manager.cpp`:

```cpp
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
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
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
    std::ofstream f(dataDir_ + "\\torrc");
    if (!f) {
        lastError_ = "cannot write torrc";
        return false;
    }
    f << "SocksPort 9050\n"
      << "DataDirectory \"" << dataDir_ << "\"\n"
      << "Log notice file \"" << dataDir_ << "\\tor.log\"\n";
    if (std::filesystem::exists(torDir_ + "\\geoip"))
        f << "GeoIPFile \"" << torDir_ << "\\geoip\"\n";
    if (std::filesystem::exists(torDir_ + "\\geoip6"))
        f << "GeoIP6File \"" << torDir_ << "\\geoip6\"\n";
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
```

- [ ] **Step 5: Wire CMake**

Update `CMakeLists.txt` to add the new sources and link the test with the
Tor/WinSock pieces:

```cmake
cmake_minimum_required(VERSION 3.21)
project(Rezo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef" CACHE PATH "CEF root")
add_subdirectory(${CEF_ROOT})

cef_add_target(TARGET rezo
    SOURCES
        src/main.cpp
        src/app.cpp
        src/client.cpp
        src/window.cpp
        src/tor_manager.cpp
        src/netutil.cpp
        src/newtab.cpp
    DEFINES
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

enable_testing()
add_executable(rezo_tests tests/test_main.cpp src/tor_manager.cpp src/netutil.cpp)
target_include_directories(rezo_tests PRIVATE src)
target_link_libraries(rezo_tests PRIVATE ws2_32)
add_test(NAME rezo_tests COMMAND rezo_tests)
```

(Task 3 doesn't create app/client/window/newtab yet, so the CMakeLists above
is forward-looking for Tasks 4-7. To keep the build green NOW, comment out
`src/app.cpp`, `src/client.cpp`, `src/window.cpp`, `src/newtab.cpp` in the
SOURCES list and uncomment them in their tasks.)

- [ ] **Step 6: Build and run tests**

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target rezo_tests
ctest --test-dir build -C Release --output-on-failure
```

Expected: ALL TESTS PASSED (the TorManager test spawns real tor and waits for
the circuit — allow up to ~90 s the first time).

- [ ] **Step 7: Commit**

```bash
git add src/tor_state.h src/netutil.h src/netutil.cpp src/tor_manager.h src/tor_manager.cpp tests/test_main.cpp CMakeLists.txt
git commit -m "task3: TorManager spawn/watchdog with tests"
```

---

### Task 4: Proxy routing + kill switch

**Files:**
- Create: `src/app.h`, `src/app.cpp`, `src/client.h`, `src/client.cpp`, `src/window.h`, `src/window.cpp`, `src/newtab.h`, `src/newtab.cpp`, `src/resources/newtab.html`, `src/resources/blocked.html`
- Modify: `src/main.cpp` (full CEF wiring, `g_tor`), `CMakeLists.txt` (uncomment app/client/window/newtab sources)

**Interfaces:**
- Consumes: `TorManager`/`g_tor` (Task 3), `CefSettings` pattern (Task 2).
- Produces: `RezoApp`, `RezoClient`, `RezoWindow` (minimal: browser view only, no chrome yet), `NewTabFactory` + blocked page. Traffic proxied through Tor; navigation blocked when Tor is down.

- [ ] **Step 1: Write src/app.h**

```cpp
#pragma once

#include "include/cef_app.h"
#include <string>

class RezoWindow;

class RezoApp : public CefApp, public CefBrowserProcessHandler {
public:
    RezoApp();
    ~RezoApp();

    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
    void OnBeforeCommandLineProcessing(const CefString& process_type,
                                       CefRefPtr<CefCommandLine> command_line) override;
    void OnContextInitialized() override;

    RezoWindow* window() { return window_; }
    void SetWindow(RezoWindow* w) { window_ = w; }

    IMPLEMENT_REFCOUNTING(RezoApp);

private:
    std::string UserAgent() const;
    RezoWindow* window_ = nullptr;
};
```

- [ ] **Step 2: Write src/app.cpp**

```cpp
#include "app.h"

#include "include/cef_version.h"
#include "newtab.h"
#include "window.h"

RezoApp::RezoApp() = default;
RezoApp::~RezoApp() = default;

std::string RezoApp::UserAgent() const {
    // Generic Windows UA; Chrome major version matches the bundled CEF build.
    return "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
           "(KHTML, like Gecko) Chrome/" + std::to_string(cef_version_info(4)) +
           " Safari/537.36";
}

void RezoApp::OnBeforeCommandLineProcessing(const CefString& process_type,
                                            CefRefPtr<CefCommandLine> command_line) {
    command_line->AppendSwitchWithValue("proxy-server", "socks5://127.0.0.1:9050");
    command_line->AppendSwitchWithValue("proxy-bypass-list", "<-loopback>");
    command_line->AppendSwitchWithValue(
        "host-resolver-rules", "MAP * ~NOTFOUND, EXCLUDE localhost, EXCLUDE 127.0.0.1");
    command_line->AppendSwitchWithValue("force-webrtc-ip-handling-policy",
                                        "disable_non_proxied_udp");
    command_line->AppendSwitchWithValue("user-agent", UserAgent());
    command_line->AppendSwitch("block-third-party-cookies");
    command_line->AppendSwitch("disable-breakpad");
    command_line->AppendSwitch("disable-component-update");
    command_line->AppendSwitch("disable-domain-reliability");
    command_line->AppendSwitch("disable-features=Translate,MediaRouter,OptimizationHints");
}

void RezoApp::OnContextInitialized() {
    CefRegisterSchemeHandlerFactory("rezo", "newtab", new NewTabFactory());
    SetWindow(new RezoWindow());
    window()->Create("rezo://newtab/");
}
```

- [ ] **Step 3: Write src/window.h and src/window.cpp (minimal for Task 4)**

`src/window.h`:

```cpp
#pragma once

#include <string>

class RezoWindow {
public:
    void Create(const std::string& startUrl);
    void DestroyWindow();
    bool IsClosing() const { return closing_; }

private:
    bool closing_ = false;
};
```

`src/window.cpp`:

```cpp
#include "window.h"

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/views/cef_window.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_box_layout.h"

#include "app.h"
#include "client.h"

namespace {

class WindowDelegate : public CefWindowDelegate {
public:
    explicit WindowDelegate(RezoWindow* owner) : owner_(owner) {}

    void OnWindowCreated(CefRefPtr<CefWindow> window) override {
        CefBrowserSettings settings;
        CefRefPtr<CefBrowserView> view = CefBrowserView::CreateBrowserView(
            new RezoClient(owner_), startUrl_, settings, nullptr, nullptr);
        window->AddChildView(view);
        window->Show();
    }

    void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
        owner_->DestroyWindow();
        CefQuitMessageLoop();
    }

    CefSize GetInitialSize(const CefRect& bounds) override { return CefSize(1280, 800); }
    IMPLEMENT_REFCOUNTING(WindowDelegate);

    std::string startUrl_;
    RezoWindow* owner_;
};

}  // namespace

void RezoWindow::Create(const std::string& startUrl) {
    CefRefPtr<WindowDelegate> delegate(new WindowDelegate(this));
    delegate->startUrl_ = startUrl;
    CefWindow::CreateTopLevelWindow(delegate);
}

void RezoWindow::DestroyWindow() {
    closing_ = true;
    RezoApp* app = static_cast<RezoApp*>(CefApp::GetInstance());  // null-safe guard below
    (void)app;
}
```

Note: `CefApp::GetInstance()` is a CEF 128+ helper. If it does not exist in
your version, expose the app pointer via a global instead:

```cpp
// main.cpp
RezoApp* g_app = nullptr;
// app.cpp OnContextInitialized etc. use g_app; DestroyWindow calls:
// g_app->SetWindow(nullptr);
```

- [ ] **Step 4: Write src/client.h and src/client.cpp**

`src/client.h`:

```cpp
#pragma once

#include "include/cef_client.h"

class RezoWindow;

class RezoClient : public CefClient,
                   public CefRequestHandler,
                   public CefLifeSpanHandler,
                   public CefLoadHandler {
public:
    explicit RezoClient(RezoWindow* window) : window_(window) {}

    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

    // CefRequestHandler
    bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefRequest> request, bool user_gesture,
                        bool is_redirect) override;
    bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              CefRefPtr<CefCallback> callback) override;
    bool OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                       const CefPopupFeatures& popup_features, CefWindowInfo& window_info,
                       CefRefPtr<CefClient>& client, CefBrowserSettings& settings,
                       CefRefPtr<CefDictionaryValue>& extra_info,
                       bool* no_javascript_access) override;

    // CefLifeSpanHandler
    bool DoClose(CefRefPtr<CefBrowser> browser) override;

    // CefLoadHandler
    void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                     ErrorCode errorCode, const CefString& errorText,
                     const CefString& failedUrl) override;

    IMPLEMENT_REFCOUNTING(RezoClient);

private:
    RezoWindow* window_;
};
```

`src/client.cpp`:

```cpp
#include "client.h"

#include "app.h"
#include "tor_manager.h"
#include "tor_state.h"

namespace {

bool IsLocalUrl(const CefString& url) {
    return url.ToString().rfind("rezo://", 0) == 0 ||
           url.ToString().rfind("file:", 0) == 0 ||
           url.ToString().rfind("data:", 0) == 0;
}

void ShowBlockedPage(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) {
    if (frame->IsMain()) frame->LoadURL("rezo://newtab/blocked");
}

}  // namespace

bool RezoClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefRequest> request, bool user_gesture,
                                bool is_redirect) {
    if (!frame->IsMain()) return false;
    if (IsLocalUrl(request->GetURL())) return false;
    if (shouldBlockRequest(g_tor.State())) {
        ShowBlockedPage(browser, frame);
        return true;  // cancel navigation
    }
    return false;
}

bool RezoClient::OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                      CefRefPtr<CefRequest> request,
                                      CefRefPtr<CefCallback> callback) {
    if (IsLocalUrl(request->GetURL())) return false;
    if (shouldBlockRequest(g_tor.State())) {
        callback->Cancel();
        ShowBlockedPage(browser, frame);
        return true;
    }
    return false;
}

bool RezoClient::OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                               const CefPopupFeatures& popup_features,
                               CefWindowInfo& window_info, CefRefPtr<CefClient>& client,
                               CefBrowserSettings& settings,
                               CefRefPtr<CefDictionaryValue>& extra_info,
                               bool* no_javascript_access) {
    // Keep it simple: allow popups. They open as plain native windows but
    // still go through this client, so proxy + kill switch apply. Tab-ifying
    // popups is a v2 improvement.
    return false;
}

bool RezoClient::DoClose(CefRefPtr<CefBrowser> browser) {
    return false;  // default close; Task 6 handles per-tab close
}

void RezoClient::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                             ErrorCode errorCode, const CefString& errorText,
                             const CefString& failedUrl) {
    if (errorCode != ERR_ABORTED && !IsLocalUrl(failedUrl) && frame->IsMain()) {
        // Proxy/Tor is down: show the blocked page instead of the error.
        ShowBlockedPage(browser, frame);
    }
}
```

`src/newtab.h`:

```cpp
#pragma once

#include "include/cef_scheme.h"
#include <string>

class NewTabFactory : public CefSchemeHandlerFactory {
public:
    CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         const CefString& scheme_name,
                                         CefRefPtr<CefRequest> request) override;
    IMPLEMENT_REFCOUNTING(NewTabFactory);
};
```

`src/newtab.cpp`:

```cpp
#include "newtab.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "tor_manager.h"
#include "tor_state.h"

namespace {

std::string ResourceDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::filesystem::path p(buf);
    return p.parent_path().string();
}

std::string ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string EscapeHtml(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

std::string BuildTiles(const std::string& cfgPath) {
    std::ifstream f(cfgPath);
    std::string out;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string name = line.substr(0, tab);
        std::string url = line.substr(tab + 1);
        if (name.empty() || url.empty()) continue;
        out += "<a href=\"" + EscapeHtml(url) + "\">" + EscapeHtml(name) + "</a>\n";
    }
    return out;
}

}  // namespace

class NewTabHandler : public CefResourceHandler {
public:
    bool ProcessRequest(CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback) override {
        url_ = request->GetURL();
        callback->Continue();
        return true;
    }

    void GetResponseHeaders(CefRefPtr<CefResponse> response, int64& response_length,
                            CefString& redirect_url) override {
        response->SetMimeType("text/html");
        response->SetStatus(200);
        response->SetHeaderByName("Cache-Control", "no-store", true);
        body_ = BuildPage(url_);
        response_length = static_cast<int64>(body_.size());
    }

    bool ReadResponse(void* data_out, int bytes_to_read, int& bytes_read,
                      CefRefPtr<CefCallback> callback) override {
        if (offset_ >= body_.size()) {
            bytes_read = 0;
            return false;
        }
        size_t n = std::min<size_t>(bytes_to_read, body_.size() - offset_);
        memcpy(data_out, body_.data() + offset_, n);
        offset_ += n;
        bytes_read = static_cast<int>(n);
        return true;
    }

    void Cancel() override {}

    IMPLEMENT_REFCOUNTING(NewTabHandler);

private:
    std::string BuildPage(const std::string& url) {
        std::string dir = ResourceDir();
        std::string status = torStateName(g_tor.State());
        std::string statusText = std::string("Tor: ") + status;
        if (url.find("blocked") != std::string::npos) {
            std::string html = ReadFile(dir + "\\blocked.html");
            // {{STATUS}} placeholder is fine to leave if absent.
            return ReplaceAll(html, "{{STATUS}}", statusText);
        }
        std::string html = ReadFile(dir + "\\newtab.html");
        html = ReplaceAll(html, "{{TILES}}", BuildTiles(dir + "\\quickaccess.txt"));
        html = ReplaceAll(html, "{{STATUS}}", statusText);
        return html;
    }

    static std::string ReplaceAll(std::string s, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
        return s;
    }

    std::string url_;
    std::string body_;
    size_t offset_ = 0;
};

CefRefPtr<CefResourceHandler> NewTabFactory::Create(CefRefPtr<CefBrowser> browser,
                                                    CefRefPtr<CefFrame> frame,
                                                    const CefString& scheme_name,
                                                    CefRefPtr<CefRequest> request) {
    return new NewTabHandler();
}
```

Note: `newtab.cpp` needs `#include <windows.h>` (for `GetModuleFileNameW`),
`<filesystem>`, `<cstring>` (memcpy). Add them.

- [ ] **Step 6: Write the resource files**

`src/resources/newtab.html`:

```html
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>New Tab</title>
<style>
  body { background:#0f0f14; color:#e8e8f0; font-family:'Segoe UI',sans-serif; text-align:center; padding-top:60px; }
  h1 { color:#7c5cff; letter-spacing:4px; }
  .tiles { display:flex; flex-wrap:wrap; gap:16px; justify-content:center; margin-top:40px; }
  a { display:block; width:160px; padding:20px 10px; background:#1a1a24; border:1px solid #2a2a3a;
      border-radius:8px; color:#c8c8d8; text-decoration:none; font-size:14px; }
  a:hover { border-color:#7c5cff; }
  .status { margin-top:30px; color:#8a8a9a; font-size:13px; }
</style>
</head>
<body>
<h1>REZO</h1>
<div class="tiles">{{TILES}}</div>
<p class="status">{{STATUS}}</p>
</body>
</html>
```

`src/resources/blocked.html`:

```html
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Tor Offline</title>
<style>
  body { background:#0f0f14; color:#e8e8f0; font-family:'Segoe UI',sans-serif; text-align:center; padding-top:120px; }
  h1 { color:#ff5c5c; letter-spacing:3px; }
  a { display:inline-block; margin-top:30px; padding:12px 28px; background:#7c5cff; color:#fff;
      border-radius:6px; text-decoration:none; }
  p { color:#8a8a9a; margin-top:20px; }
</style>
</head>
<body>
<h1>TOR OFFLINE</h1>
<p>Rezo cannot load this page: the Tor connection is down.</p>
<p>Your IP was never exposed.</p>
<p>Status: {{STATUS}}</p>
<a href="rezo://newtab/">Retry</a>
</body>
</html>
```

- [ ] **Step 7: Replace src/main.cpp**

```cpp
#include "include/cef_app.h"

#include "app.h"
#include "tor_manager.h"

TorManager g_tor;
RezoApp* g_app = nullptr;

int main(int argc, char* argv[]) {
    CefMainArgs args(argc, argv);
    CefRefPtr<RezoApp> app(new RezoApp);
    g_app = app.get();
    int code = CefExecuteProcess(args, app.get(), nullptr);
    if (code >= 0) return code;

    CefSettings settings;
    settings.no_sandbox = true;
    settings.log_severity = LOGSEVERITY_WARNING;
    if (!CefInitialize(args, settings, app.get(), nullptr)) return 1;

    g_tor.StartAsync();  // spawn tor in the background; UI shows Starting
    CefRunMessageLoop();

    g_tor.Stop();
    CefShutdown();
    return 0;
}
```

And in `app.cpp` replace the `CefApp::GetInstance()` usage from window.cpp's
`DestroyWindow()` with the global `g_app` (update `src/window.cpp`):

```cpp
void RezoWindow::DestroyWindow() {
    closing_ = true;
    g_app->SetWindow(nullptr);
}
```

Uncomment `src/app.cpp`, `src/client.cpp`, `src/window.cpp`, `src/newtab.cpp`
in CMakeLists SOURCES. Also update `RezoApp` ctor to start the window only
when g_app is set (already the case via OnContextInitialized).

- [ ] **Step 8: Build, run tests, manual smoke test**

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Manual smoke test (Tor will take ~30-90 s to bootstrap the first time):

1. Copy `tor\` and `src\resources\*` next to the built exe:
   `Copy-Item tor build\bin\Release\tor -Recurse` and
   `Copy-Item src\resources build\bin\Release\resources -Recurse`
   (resource files must sit next to the exe — the scheme handler reads
   `newtab.html`, `blocked.html`, `quickaccess.txt` from there).
2. Launch Rezo.exe. New tab shows the REZO grid with "Tor: Starting" that
   becomes "Tor: Connected" once the circuit is up.
3. In the address bar... (no address bar yet — Task 6). Verify routing by
   temporarily loading `https://check.torproject.org` as the start URL:
   change `OnContextInitialized` start URL to
   `"https://check.torproject.org"`, rebuild, launch. Page should say
   "Congratulations. This browser is configured to use Tor."
4. Kill `tor.exe` in Task Manager while Rezo is open. Navigate → blocked page
   appears ("TOR OFFLINE"). Click Retry after relaunching tor manually —
   in v1 the retry re-runs `StartAsync`; ensure `OnContextInitialized`
   blocked page link points at `rezo://newtab/` (it does).
5. Restore start URL to `"rezo://newtab/"`.

- [ ] **Step 9: Commit**

```bash
git add src/app.h src/app.cpp src/client.h src/client.cpp src/window.h src/window.cpp src/newtab.h src/newtab.cpp src/main.cpp CMakeLists.txt src/resources/
git commit -m "task4: Tor proxy routing + kill switch + blocked page"
```

---

### Task 5: Privacy hardening flags

**Files:**
- Modify: `src/app.cpp` (switch set already present from Task 4 — this task verifies and tunes)

**Interfaces:**
- Consumes: Task 4's `OnBeforeCommandLineProcessing`.
- Produces: verified privacy posture; documented tuning knobs.

- [ ] **Step 1: Confirm the switch set**

The switches from Task 4 are: proxy-server (SOCKS5 9050), proxy-bypass-list
(loopback only), host-resolver-rules (all DNS via proxy → no local DNS leaks),
force-webrtc-ip-handling-policy (WebRTC proxy-only → effectively off over
SOCKS5), user-agent (generic Windows), block-third-party-cookies,
disable-breakpad/component-update/domain-reliability,
disable-features=Translate,MediaRouter,OptimizationHints.

- [ ] **Step 2: Verification checklist (manual, in order)**

With Tor connected, visit each and record results:

1. **IP check** — https://check.torproject.org → "Congratulations… configured to use Tor".
2. **UA check** — https://www.whatismybrowser.com/detect/what-is-my-user-agent
   → generic Windows Chrome string, no build/arch specifics beyond Win64.
3. **WebRTC leak check** — https://browserleaks.com/webrtc → no local IPs
   listed (public IP shown is the Tor exit, not yours).
4. **DNS leak check** — https://www.dnsleaktest.com → results should show
   Tor exit locations only.
5. **Cookie check** — https://browserleaks.com/third-party → 3rd-party cookies
   blocked.
6. **Disk hygiene** — after closing Rezo, confirm no
   `%LOCALAPPDATA%\Rezo\...\Cache` or `History` files exist (empty cache_path
   = memory-only profile). Tor data is expected in `%LOCALAPPDATA%\Rezo\tor`.

If any check fails, adjust the corresponding switch in
`OnBeforeCommandLineProcessing` (e.g. WebRTC still leaking → append
`--disable-features=WebRtcHideLocalIpsWithMdns` or `--disable-webrtc` if your
CEF build supports it), rebuild, re-run the checklist. Document the final
switch set as a comment block in app.cpp.

- [ ] **Step 3: Commit**

```bash
git add src/app.cpp
git commit -m "task5: verified privacy hardening switches"
```

---

### Task 6: Browser chrome — tabs, address bar, status

**Files:**
- Modify: `src/window.h`, `src/window.cpp`, `src/client.h`, `src/client.cpp` (title/address/status hooks)

**Interfaces:**
- Consumes: `RezoClient` (Task 4), `g_tor` (Task 3).
- Produces: full `RezoWindow` with tab strip, top bar (back/forward/reload/address), Tor status ticker; client callbacks `SetTabTitle`, `SetAddress`, `OnLoadStart`.

- [ ] **Step 1: Write the new src/window.h**

```cpp
#pragma once

#include "include/cef_browser.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_button.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_window.h"

#include <string>
#include <vector>

class RezoWindow;

// Single delegate for the window, its buttons and the address textfield.
class Chrome : public CefWindowDelegate,
               public CefButtonDelegate,
               public CefTextfieldDelegate {
public:
    explicit Chrome(RezoWindow* owner) : owner_(owner) {}

    // CefWindowDelegate
    void OnWindowCreated(CefRefPtr<CefWindow> window) override;
    void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
    CefSize GetInitialSize(const CefRect& bounds) override { return CefSize(1280, 800); }

    // CefButtonDelegate
    void OnButtonPressed(CefRefPtr<CefButton> button, const CefButtonEvent& event) override;

    // CefTextfieldDelegate
    bool OnKeyEvent(CefRefPtr<CefTextfield> textfield, const CefKeyEvent& event) override;

    IMPLEMENT_REFCOUNTING(Chrome);

private:
    RezoWindow* owner_;
};

class StatusTicker : public CefTask {
public:
    explicit StatusTicker(RezoWindow* owner) : owner_(owner) {}
    void Execute() override;
    IMPLEMENT_REFCOUNTING(StatusTicker);
private:
    RezoWindow* owner_;
};

class RezoWindow {
public:
    RezoWindow();
    ~RezoWindow();

    void Create(const std::string& startUrl);
    void NavigateTo(const std::string& input);
    void OpenTab(const std::string& url);
    void CloseTab(CefRefPtr<CefBrowser> browser);
    void SelectTab(int index);
    void SetTabTitle(CefRefPtr<CefBrowser> browser, const CefString& title);
    void SetAddress(const std::string& url);
    void UpdateTorStatus();
    bool IsClosing() const { return closing_; }
    void DestroyWindow();
    CefRefPtr<CefBrowser> ActiveBrowser() const;

private:
    struct Tab {
        CefRefPtr<CefBrowserView> view;
        CefRefPtr<CefLabelButton> button;
    };
    int TabIndex(CefRefPtr<CefBrowser> browser) const;
    void RebuildTabButtons();

    CefRefPtr<CefWindow> window_;
    CefRefPtr<CefPanel> root_;
    CefRefPtr<CefPanel> tabBar_;
    CefRefPtr<CefLabelButton> plus_;
    CefRefPtr<CefTextfield> address_;
    CefRefPtr<CefLabelButton> status_;
    CefRefPtr<Chrome> chrome_;
    std::vector<Tab> tabs_;
    int selected_ = -1;
    std::string startUrl_;
    bool closing_ = false;
};
```

- [ ] **Step 2: Write the new src/window.cpp**

```cpp
#include "window.h"

#include "include/cef_parser.h"
#include "include/cef_task.h"

#include "app.h"
#include "client.h"
#include "tor_manager.h"
#include "tor_state.h"

namespace {

enum {
    ID_BACK = 1,
    ID_FORWARD,
    ID_RELOAD,
    ID_NEWTAB,
    ID_TAB_FIRST = 100,
};

std::string TrimTitle(const std::string& s) {
    return s.size() > 24 ? s.substr(0, 24) + "..." : s;
}

}  // namespace

RezoWindow::RezoWindow() : chrome_(new Chrome(this)) {}
RezoWindow::~RezoWindow() = default;

void Chrome::OnWindowCreated(CefRefPtr<CefWindow> window) {
    RezoWindow* w = owner_;

    w->window_ = window;
    window->SetTitle("Rezo");
    window->SetBackgroundColor(0xFF101014);

    // Root: vertical [tab bar, top bar, browser area]
    CefRefPtr<CefPanel> root = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings vs;
    vs.orientation = CEF_ORIENTATION_VERTICAL;
    vs.between_child_spacing = 2;
    root->SetLayout(CefBoxLayout::Create(vs, root));
    w->root_ = root;

    // Tab strip
    CefRefPtr<CefPanel> tabBar = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings ts;
    ts.orientation = CEF_ORIENTATION_HORIZONTAL;
    ts.between_child_spacing = 2;
    tabBar->SetLayout(CefBoxLayout::Create(ts, tabBar));
    w->tabBar_ = tabBar;
    w->plus_ = CefLabelButton::CreateButton(this, "+");
    w->plus_->SetID(ID_NEWTAB);
    tabBar->AddChildView(w->plus_);

    // Top bar: back / forward / reload / address / status
    CefRefPtr<CefPanel> bar = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings hs;
    hs.orientation = CEF_ORIENTATION_HORIZONTAL;
    hs.between_child_spacing = 4;
    bar->SetLayout(CefBoxLayout::Create(hs, bar));

    CefRefPtr<CefLabelButton> back = CefLabelButton::CreateButton(this, "◀");
    back->SetID(ID_BACK);
    back->SetTooltipText("Back");
    CefRefPtr<CefLabelButton> fwd = CefLabelButton::CreateButton(this, "▶");
    fwd->SetID(ID_FORWARD);
    fwd->SetTooltipText("Forward");
    CefRefPtr<CefLabelButton> reload = CefLabelButton::CreateButton(this, "⟳");
    reload->SetID(ID_RELOAD);
    reload->SetTooltipText("Reload");
    w->address_ = CefTextfield::CreateTextfield(this);
    w->address_->SetPlaceholderText("Enter address or search");
    w->status_ = CefLabelButton::CreateButton(nullptr, "");
    w->status_->SetEnabled(false);

    bar->AddChildView(back);
    bar->AddChildView(fwd);
    bar->AddChildView(reload);
    bar->AddChildView(w->address_);
    bar->GetLayout()->AsBoxLayout()->SetFlexForView(w->address_, 1);
    bar->AddChildView(w->status_);

    // First tab
    CefBrowserSettings settings;
    CefRefPtr<CefBrowserView> view = CefBrowserView::CreateBrowserView(
        new RezoClient(w), w->startUrl_, settings, nullptr, nullptr);

    root->AddChildView(tabBar);
    root->AddChildView(bar);
    root->AddChildView(view);
    root->GetLayout()->AsBoxLayout()->SetFlexForView(view, 1);

    window->AddChildView(root);
    w->tabs_.push_back({view, nullptr});
    w->SelectTab(0);
    window->Show();
    CefPostDelayedTask(TID_UI, new StatusTicker(w), 1000);
}

void Chrome::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
    owner_->DestroyWindow();
    CefQuitMessageLoop();
}

void Chrome::OnButtonPressed(CefRefPtr<CefButton> button, const CefButtonEvent& event) {
    RezoWindow* w = owner_;
    int id = button->GetID();
    CefRefPtr<CefBrowser> browser = w->ActiveBrowser();
    if (id == ID_BACK && browser) browser->GoBack();
    else if (id == ID_FORWARD && browser) browser->GoForward();
    else if (id == ID_RELOAD && browser) browser->Reload();
    else if (id == ID_NEWTAB) w->OpenTab("");
    else if (id >= ID_TAB_FIRST) w->SelectTab(id - ID_TAB_FIRST);
}

bool Chrome::OnKeyEvent(CefRefPtr<CefTextfield> textfield, const CefKeyEvent& event) {
    if (event.type == KEYEVENT_RAWKEYDOWN && event.windows_key_code == VK_RETURN) {
        owner_->NavigateTo(textfield->GetText().ToString());
        return true;
    }
    return false;
}

void StatusTicker::Execute() {
    if (owner_->IsClosing()) return;
    owner_->UpdateTorStatus();
    CefPostDelayedTask(TID_UI, this, 1000);
}

void RezoWindow::Create(const std::string& startUrl) {
    startUrl_ = startUrl;
    CefWindow::CreateTopLevelWindow(chrome_);
}

void RezoWindow::DestroyWindow() {
    closing_ = true;
    g_app->SetWindow(nullptr);
    window_->RemoveAllChildViews();
    window_ = nullptr;
    tabs_.clear();
}

CefRefPtr<CefBrowser> RezoWindow::ActiveBrowser() const {
    if (selected_ < 0 || selected_ >= static_cast<int>(tabs_.size())) return nullptr;
    return tabs_[selected_].view->GetBrowser();
}

int RezoWindow::TabIndex(CefRefPtr<CefBrowser> browser) const {
    for (size_t i = 0; i < tabs_.size(); ++i) {
        if (tabs_[i].view->GetBrowser() == browser) return static_cast<int>(i);
    }
    return -1;
}

void RezoWindow::SelectTab(int index) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return;
    selected_ = index;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        tabs_[i].view->SetVisible(i == static_cast<size_t>(index));
    }
    RebuildTabButtons();
    CefRefPtr<CefBrowser> b = ActiveBrowser();
    if (b) SetAddress(b->GetMainFrame()->GetURL().ToString());
}

void RezoWindow::RebuildTabButtons() {
    // Remove old tab buttons, keep the "+" button.
    if (!tabBar_) return;
    std::vector<CefRefPtr<CefView>> old;
    for (size_t i = 0; i < tabBar_->GetChildViewCount(); ++i) {
        CefRefPtr<CefView> v = tabBar_->GetChildViewAt(static_cast<int>(i));
        if (v != plus_) old.push_back(v);
    }
    for (CefRefPtr<CefView>& v : old) tabBar_->RemoveChildView(v);

    for (size_t i = 0; i < tabs_.size(); ++i) {
        std::string label = TrimTitle(tabs_[i].view->GetBrowser()->GetMainFrame()->GetURL().ToString());
        if (label.empty()) label = "New Tab";
        if (static_cast<int>(i) == selected_) label = "● " + label;
        CefRefPtr<CefLabelButton> b = CefLabelButton::CreateButton(chrome_, label);
        b->SetID(static_cast<int>(ID_TAB_FIRST + i));
        tabBar_->AddChildView(b);
        tabs_[i].button = b;
    }
}

void RezoWindow::OpenTab(const std::string& url) {
    std::string target = url.empty() ? "rezo://newtab/" : url;
    CefRefPtr<CefBrowserView> view = CefBrowserView::CreateBrowserView(
        new RezoClient(this), target, CefBrowserSettings(), nullptr, nullptr);
    root_->AddChildView(view);
    root_->GetLayout()->AsBoxLayout()->SetFlexForView(view, 1);
    tabs_.push_back({view, nullptr});
    SelectTab(static_cast<int>(tabs_.size()) - 1);
}

void RezoWindow::CloseTab(CefRefPtr<CefBrowser> browser) {
    int idx = TabIndex(browser);
    if (idx < 0) return;
    if (tabs_.size() <= 1) {
        // Last tab: close the window.
        if (window_) window_->Close();
        return;
    }
    CefRefPtr<CefBrowserView> view = tabs_[idx].view;
    tabs_.erase(tabs_.begin() + idx);
    root_->RemoveChildView(view);
    view->GetBrowser()->GetHost()->CloseBrowser(true);
    SelectTab(std::min(selected_, static_cast<int>(tabs_.size()) - 1));
    RebuildTabButtons();
}

void RezoWindow::NavigateTo(const std::string& input) {
    std::string url = input;
    if (url.find("://") == std::string::npos) {
        if (url.find('.') == std::string::npos) {
            // Looks like a search query.
            url = "https://html.duckduckgo.com/html/?q=" + CefURIEncode(url, false).ToString();
        } else {
            url = "http://" + url;
        }
    }
    if (CefRefPtr<CefBrowser> b = ActiveBrowser()) b->GetMainFrame()->LoadURL(url);
    SetAddress(url);
}

void RezoWindow::SetTabTitle(CefRefPtr<CefBrowser> browser, const CefString& title) {
    int idx = TabIndex(browser);
    if (idx < 0) return;
    tabs_[idx].button->SetText(TrimTitle(title.ToString()));
}

void RezoWindow::SetAddress(const std::string& url) {
    if (address_) address_->SetText(url);
}

void RezoWindow::UpdateTorStatus() {
    if (status_) status_->SetText(std::string("Tor: ") + torStateName(g_tor.State()));
}
```

- [ ] **Step 3: Update client.h/client.cpp with display hooks**

`src/client.h` — add `CefDisplayHandler` to the base list and
`GetDisplayHandler()` override, plus these methods:

```cpp
    // CefDisplayHandler
    void OnTitleChanged(CefRefPtr<CefBrowser> browser, const CefString& title) override;
    void OnAddressChanged(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                          const CefString& url) override;
```

`src/client.cpp` — add:

```cpp
void RezoClient::OnTitleChanged(CefRefPtr<CefBrowser> browser, const CefString& title) {
    window_->SetTabTitle(browser, title);
}

void RezoClient::OnAddressChanged(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                  const CefString& url) {
    if (frame->IsMain()) window_->SetAddress(url.ToString());
}
```

Update `RezoClient::DoClose` for per-tab closing (Ctrl+W triggers close of the
browser view):

```cpp
bool RezoClient::DoClose(CefRefPtr<CefBrowser> browser) {
    window_->CloseTab(browser);
    return true;
}
```

- [ ] **Step 4: Build and manual test**

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Copy `tor\` and `src\resources\*` next to the exe again if needed. Manual
checklist:

1. Window shows tab strip ("● New Tab"), top bar, address field, "Tor: Connected".
2. Address bar: type `https://check.torproject.org` + Enter → Tor check page
   (loading may take a few seconds through Tor).
3. Type `cats` + Enter → DuckDuckGo HTML search results.
4. "+" opens a new tab; clicking tab buttons switches; Ctrl+W closes the
   current tab; closing the last tab closes the window and exits the process.
5. Back/forward/reload work per tab.
6. Tor status label updates every second.
7. Kill tor.exe → next navigation shows "TOR OFFLINE" blocked page.

- [ ] **Step 5: Commit**

```bash
git add src/window.h src/window.cpp src/client.h src/client.cpp
git commit -m "task6: tab strip, address bar, tor status chrome"
```

---

### Task 7: Quick-access tiles + retry wiring

**Files:**
- Create: `src/resources/quickaccess.txt` (defaults; created at runtime if missing)
- Modify: `src/newtab.cpp` (write defaults on first run, retry button behavior), `src/window.cpp` (Ctrl+W handled by CEF default; nothing extra)

**Interfaces:**
- Consumes: `NewTabHandler::BuildPage` (Task 4).
- Produces: user-editable tile config with sensible defaults; retry flow re-runs `g_tor.StartAsync()`.

- [ ] **Step 1: Ensure default config is created on first run**

In `src/newtab.cpp`, add a helper called from `BuildPage`:

```cpp
std::string EnsureConfig(const std::string& cfgPath) {
    if (std::filesystem::exists(cfgPath)) return cfgPath;
    std::ofstream f(cfgPath);
    if (f) {
        f << "# Rezo quick access tiles\n"
          << "# One tile per line: Name<TAB>URL\n\n"
          << "Steam\thttps://store.steampowered.com\n"
          << "Discord\thttps://discord.com/app\n"
          << "Roblox\thttps://www.roblox.com\n"
          << "Reddit\thttps://www.reddit.com\n"
          << "YouTube\thttps://www.youtube.com\n";
    }
    return cfgPath;
}
```

Call `EnsureConfig` before `BuildTiles` in `BuildPage`. (If the resources dir
isn't writable, `BuildTiles` just yields an empty grid — acceptable.)

- [ ] **Step 2: Retry button re-runs Tor**

The blocked page's Retry link (`rezo://newtab/`) already reloads the new tab,
but does not restart Tor. Add a tiny script to `blocked.html` that asks the
scheme handler to restart via a dedicated URL:

In `src/newtab.cpp`, in `BuildPage`, handle a `retry` URL before the generic
newtab branch:

```cpp
        if (url.find("retry") != std::string::npos) {
            g_tor.StartAsync();
            return "<html><body style='background:#0f0f14;color:#e8e8f0;font-family:sans-serif;text-align:center;padding-top:120px'>"
                   "<h1>TOR STARTING</h1>"
                   "<p>Waiting for the Tor circuit...</p>"
                   "<a href='rezo://newtab/' style='color:#7c5cff'>Back</a></body></html>";
        }
```

And in `blocked.html` change the Retry link to:

```html
<a href="rezo://newtab/retry">Retry</a>
```

- [ ] **Step 3: Build and manual test**

```powershell
cmake --build build --config Release
```

1. Delete `build\bin\Release\quickaccess.txt` if present. Launch → grid shows
   the five defaults (Steam, Discord, Roblox, Reddit, YouTube); a
   `quickaccess.txt` file now exists next to the exe.
2. Edit `quickaccess.txt` — add `MyWiki<TAB>https://example.com` — refresh the
   new tab (Ctrl+R in the new tab) → tile appears.
3. Kill tor.exe, click a tile → blocked page. Click "Retry" → "TOR STARTING"
   page appears, then (once the circuit is back) "Tor: Connected" and the new
   tab loads.
4. Sanity: tiles open through Tor (check.torproject.org still congratulates).

- [ ] **Step 4: Commit**

```bash
git add src/newtab.cpp src/resources/blocked.html
git commit -m "task7: quick-access tiles with defaults and retry flow"
```

---

### Task 8: Packaging + acceptance run

**Files:**
- Create: `scripts/package.ps1`
- Modify: `README.md` (final run instructions)

**Interfaces:**
- Consumes: built exe + runtime + `tor\` + `src\resources\*`.
- Produces: portable `dist\Rezo\` folder; full acceptance checklist passed.

- [ ] **Step 1: Write scripts/package.ps1**

```powershell
param([string]$Config = "Release")

$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root "build"
$out = Join-Path $root "dist\Rezo"

# Locate the built exe (CEF may use build\bin\Release or build\Release).
$exe = Get-ChildItem -Path (Join-Path $build "bin\$Config"), (Join-Path $build $Config) `
    -Filter "Rezo.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $exe) { throw "Rezo.exe not found in build output" }

Remove-Item $out -Recurse -Force -ErrorAction SilentlyContinue
New-Item $out -ItemType Directory -Force | Out-Null

# Everything the exe needs: its whole output dir (CEF post-build copies
# libcef.dll, resources.pak, locales, etc. next to it).
Get-ChildItem $exe.Directory | Copy-Item -Destination $out -Recurse -Force

# Tor bundle + resources next to the exe.
Copy-Item (Join-Path $root "tor") $out -Recurse -Force
Copy-Item (Join-Path $root "src\resources\*") $out -Force

Write-Host "Packaged to $out"
```

- [ ] **Step 2: Package and verify portable layout**

```powershell
.\scripts\package.ps1
Get-ChildItem dist\Rezo
```

Expected: `Rezo.exe`, `libcef.dll`, `chrome_elf.dll`, `resources.pak`,
`icudtl.dat`, `locales\`, `tor\` (tor.exe, geoip, geoip6), `newtab.html`,
`blocked.html`, `quickaccess.txt` (auto-created at runtime).

- [ ] **Step 3: Full acceptance run (fresh state)**

1. Close any running Rezo; delete `%LOCALAPPDATA%\Rezo` (fresh Tor state).
2. Run `dist\Rezo\Rezo.exe`. First launch: Tor bootstrap ~30-90 s.
3. **Acceptance checklist (spec §Testing):**
   - [ ] New tab shows REZO grid, status goes Starting → Connected.
   - [ ] https://check.torproject.org → "Congratulations".
   - [ ] whatismybrowser.com → generic Windows UA (Task 5).
   - [ ] browserleaks.com/webrtc → no local IP leak (Task 5).
   - [ ] dnsleaktest.com → Tor exit only (Task 5).
   - [ ] Tiles + custom tile from quickaccess.txt work (Task 7).
   - [ ] Tabs: new/switch/Ctrl+W; closing last tab exits process.
   - [ ] Address bar: URL + search (DuckDuckGo HTML).
   - [ ] Kill tor.exe mid-session → navigation blocked, "TOR OFFLINE" page.
   - [ ] Retry → Tor restarts → browsing resumes.
   - [ ] Close Rezo → process exits cleanly; no cache/history files on disk.
4. Re-run `ctest --test-dir build -C Release --output-on-failure` → ALL TESTS PASSED.

- [ ] **Step 4: Update README final instructions**

Append to README.md:

```markdown
## Run

Build, then copy the `tor\` folder and `src\resources\*` next to the exe
(first run also works from `dist\Rezo\` after packaging):

```powershell
.\scripts\package.ps1
dist\Rezo\Rezo.exe
```

First launch takes 30-90 s to connect to Tor. If Tor fails, every page shows
the "TOR OFFLINE" screen; click Retry. No IP ever leaves your machine
directly.
```

- [ ] **Step 5: Commit**

```bash
git add scripts/package.ps1 README.md
git commit -m "task8: packaging script and acceptance run"
```

---

## Self-Review Notes

- **Spec coverage:** Architecture (Tasks 1-3), privacy core: routing/kill
  switch (Task 4), hardening flags (Task 5), UI: tabs/bar/status (Task 6),
  quick-access (Task 7), error handling (Tasks 4, 7), testing (Task 3 unit
  tests + Task 8 acceptance), packaging (Task 8). Out-of-scope items (New
  Identity, bridges, canvas randomization, extensions, VPN layer) are not
  implemented.
- **Deliberate v1 simplifications:** popups open as separate native windows
  (still proxied); tab titles fall back to URLs before `OnTitleChanged`;
  blocked-page retry is a manual click rather than auto-reconnect.