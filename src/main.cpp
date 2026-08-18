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
    // CEF 151 removed CefMainArgs(int, char**); Windows uses HINSTANCE.
    CefMainArgs args(GetModuleHandle(nullptr));
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