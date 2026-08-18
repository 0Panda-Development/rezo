#include "include/cef_app.h"

#include "app.h"
#include "tor_manager.h"

TorManager g_tor;
RezoApp* g_app = nullptr;

int main(int argc, char* argv[]) {
    // CEF 151 removed CefMainArgs(int, char**); Windows uses HINSTANCE.
    CefMainArgs args(GetModuleHandle(nullptr));
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