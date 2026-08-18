#include "app.h"

#include "include/cef_version_info.h"
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

// Privacy hardening switch set (task 5 verified):
//   proxy-server                     socks5://127.0.0.1:9050  — Tor SOCKS5 proxy (tor.exe must run)
//   proxy-bypass-list                <-loopback>              — only loopback bypasses the proxy
//   host-resolver-rules              MAP * ~NOTFOUND + EXCLUDE localhost/127.0.0.1 — all DNS via proxy, no local leaks
//   force-webrtc-ip-handling-policy  disable_non_proxied_udp  — WebRTC over SOCKS5 effectively off
//   user-agent                       generic Windows Chrome   — no build/arch specifics beyond Win64
//   block-third-party-cookies                                 — 3rd-party cookies blocked
//   disable-breakpad / disable-component-update / disable-domain-reliability — no telemetry/updates
//   disable-features=Translate,MediaRouter,OptimizationHints  — telemetry/network features off
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
    command_line->AppendSwitch("disable-background-networking");
    command_line->AppendSwitch("disable-features=Translate,MediaRouter,OptimizationHints");
}

void RezoApp::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
    registrar->AddCustomScheme("rezo", CEF_SCHEME_OPTION_STANDARD);
}

void RezoApp::OnContextInitialized() {
    CefRegisterSchemeHandlerFactory("rezo", "newtab", new NewTabFactory());
    SetWindow(new RezoWindow());
    window()->Create("rezo://newtab/");
}