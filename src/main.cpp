#include "include/cef_app.h"

#include "app.h"
#include "tor_manager.h"

#include <filesystem>
#include <shlobj.h>

TorManager g_tor;
RezoApp* g_app = nullptr;

int main(int argc, char* argv[]) {
    // CEF 151 removed CefMainArgs(int, char**); Windows uses HINSTANCE.
    CefMainArgs args(GetModuleHandle(nullptr));
    CefRefPtr<RezoApp> app(new RezoApp);
    g_app = app.get();
    int code = CefExecuteProcess(args, app.get(), nullptr);
    if (code >= 0) return code;

    // Spawn tor before CEF init so bootstrap overlaps window creation — the
    // kill switch only opens when Tor is Connected, so earlier is faster.
    g_tor.StartAsync();

    CefSettings settings;
    settings.no_sandbox = true;
    settings.log_severity = LOGSEVERITY_WARNING;
    // Scope installation-specific data to our own dir. CEF's default shared
    // AppData\Local\CEF\User Data would otherwise collide with other CEF apps
    // (process singleton lock) and carry state across launches. cache_path
    // stays empty: incognito, no profile data on disk.
    wchar_t appdata[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appdata) == S_OK) {
        std::filesystem::path root = std::filesystem::path(appdata) / "Rezo" / "cef";
        std::filesystem::create_directories(root);
        std::u16string root16 = root.u16string();
        cef_string_utf16_set(root16.c_str(), root16.size(), &settings.root_cache_path, true);
    }
    if (!CefInitialize(args, settings, app.get(), nullptr)) return 1;

    CefRunMessageLoop();

    g_tor.Stop();
    CefShutdown();
    return 0;
}