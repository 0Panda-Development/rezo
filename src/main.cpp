#include "include/cef_app.h"
#include "include/cef_parser.h"

#include "app.h"
#include "tor_manager.h"

#include <filesystem>
#include <shlobj.h>

TorManager g_tor;
RezoApp* g_app = nullptr;
// Held for the process lifetime; a second instance fails CreateMutex and
// exits immediately (single-instance guard).
static HANDLE g_singleton = nullptr;

int main(int argc, char* argv[]) {
    // CEF 151 removed CefMainArgs(int, char**); Windows uses HINSTANCE.
    CefMainArgs args(GetModuleHandle(nullptr));
    CefRefPtr<RezoApp> app(new RezoApp);
    g_app = app.get();
    int code = CefExecuteProcess(args, app.get(), nullptr);
    if (code >= 0) return code;

    // Browser process: enforce one copy. Renderer/GPU subprocesses exit above.
    g_singleton = CreateMutexW(nullptr, TRUE, L"Local\\RezoSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return 0;

    // Spawn tor before CEF init so bootstrap overlaps window creation — the
    // kill switch only opens when Tor is Connected, so earlier is faster.
    g_tor.StartAsync();

    CefSettings settings;
    settings.no_sandbox = true;
    // Log off entirely: the two error types Chromium still emits here (frame
    // latency / browser-info handshake races) are benign and the GPU crash
    // spam is already fixed via --disable-gpu. Flip to LOGSEVERITY_WARNING to
    // diagnose issues later.
    settings.log_severity = LOGSEVERITY_DISABLE;
    // Scope installation-specific data to our own dir. CEF's default shared
    // AppData\Local\CEF\User Data would otherwise collide with other CEF apps
    // (process singleton lock) and carry state across launches. cache_path is
    // a disk cache only for trusted sites (untrusted traffic is uncached via
    // UR_FLAG_SKIP_CACHE); the rest stays incognito.
    wchar_t appdata[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appdata) == S_OK) {
        std::filesystem::path root = std::filesystem::path(appdata) / "Rezo" / "cef";
        std::filesystem::create_directories(root);
        std::u16string root16 = root.u16string();
        cef_string_utf16_set(root16.c_str(), root16.size(), &settings.root_cache_path, true);
        std::filesystem::path cache = root / "cache";
        std::filesystem::create_directories(cache);
        std::u16string cache16 = cache.u16string();
        cef_string_utf16_set(cache16.c_str(), cache16.size(), &settings.cache_path, true);
    }

    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);

    if (!CefInitialize(args, settings, app.get(), nullptr)) return 1;

    CefRunMessageLoop();

    g_tor.Stop();
    CefShutdown();
    return 0;
}