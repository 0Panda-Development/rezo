#include "app.h"

#include <shellapi.h>
#include <shlobj.h>
#include <filesystem>

#include "include/cef_version_info.h"
#include "include/cef_cookie.h"
#include "newtab.h"
#include "trusted.h"
#include "window.h"

void ClearAllCookies() {
    CefRefPtr<CefCookieManager> manager = CefCookieManager::GetGlobalManager(nullptr);
    if (manager) {
        manager->DeleteCookies("", "", nullptr);
        manager->FlushStore(nullptr);
    }
}

RezoApp::RezoApp() = default;
RezoApp::~RezoApp() = default;

void RezoApp::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    // Clear all cookies on browser close
    ClearAllCookies();
}

std::string RezoApp::UserAgent() const {
    // Generic Windows UA; Chrome major version matches the bundled CEF build.
    return "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
           "(KHTML, like Gecko) Chrome/" + std::to_string(cef_version_info(4)) +
           " Safari/537.36";
}

// Privacy hardening switch set (task 5 verified):
//   proxy-server                     socks5://127.0.0.1:9050  — Tor SOCKS5 proxy (tor.exe must run)
//   proxy-bypass-list                trusted domains + loopback — trusted sites bypass the proxy (direct)
//   host-resolver-rules              MAP * ~NOTFOUND + EXCLUDE localhost/127.0.0.1 — all DNS via proxy, no local leaks
//   force-webrtc-ip-handling-policy  disable_non_proxied_udp  — WebRTC over SOCKS5 effectively off
//   user-agent                       generic Windows Chrome   — no build/arch specifics beyond Win64
//   block-third-party-cookies                                 — 3rd-party cookies blocked
//   disable-breakpad / disable-component-update / disable-domain-reliability — no telemetry/updates
//   disable-features=Translate,MediaRouter,OptimizationHints  — telemetry/network features off
void RezoApp::OnBeforeCommandLineProcessing(const CefString& process_type,
                                            CefRefPtr<CefCommandLine> command_line) {
    std::string bypass = "<-loopback>";
    std::string resolve = "MAP * ~NOTFOUND, EXCLUDE localhost, EXCLUDE 127.0.0.1";
    for (const auto& d : kTrustedDomains) {
        bypass += "," + d + ",*." + d;
        resolve += ", EXCLUDE " + d + ", EXCLUDE *." + d;
    }
    command_line->AppendSwitchWithValue("proxy-server", "socks5://127.0.0.1:9050");
    command_line->AppendSwitchWithValue("proxy-bypass-list", bypass);
    command_line->AppendSwitchWithValue("host-resolver-rules", resolve);
    command_line->AppendSwitchWithValue("force-webrtc-ip-handling-policy",
                                        "disable_non_proxied_udp");
    command_line->AppendSwitchWithValue("user-agent", UserAgent());
    command_line->AppendSwitch("block-third-party-cookies");
    command_line->AppendSwitch("disable-breakpad");
    command_line->AppendSwitch("disable-component-update");
    command_line->AppendSwitch("disable-domain-reliability");
    command_line->AppendSwitch("disable-background-networking");
    command_line->AppendSwitch("disable-features=Translate,MediaRouter,OptimizationHints");
    // GPU child crashes on this machine (VM without working GPU path);
    // software rendering instead.
    command_line->AppendSwitch("disable-gpu");
}

void RezoApp::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
    registrar->AddCustomScheme("rezo", CEF_SCHEME_OPTION_STANDARD);
}

void RezoApp::OnContextInitialized() {
    CefRegisterSchemeHandlerFactory("rezo", "newtab", new NewTabFactory());

    // Run updater once (first launch only) - it checks for updates silently
    // and only shows UI if update is available. Uses a marker file to run once.
    wchar_t appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr,
                                   SHGFP_TYPE_CURRENT, appdata))) {
        std::filesystem::path markerDir(appdata);
        markerDir /= L"Rezo";
        std::filesystem::create_directories(markerDir);
        std::filesystem::path marker = markerDir / L"updater_ran.marker";

        if (!std::filesystem::exists(marker)) {
            std::wstring updater = (markerDir / L"RezoUpdater.exe").wstring();
            if (GetFileAttributesW(updater.c_str()) != INVALID_FILE_ATTRIBUTES) {
                ShellExecuteW(nullptr, L"open", updater.c_str(), L"-s", nullptr, SW_HIDE);
            }
            // Create marker so we only run once
            std::ofstream ofs(marker);
            ofs << "1";
        }
    }
    SetWindow(new RezoWindow());
    window()->Create("rezo://newtab/");
}