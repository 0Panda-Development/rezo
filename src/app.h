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
    void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;
    void OnContextInitialized() override;

    RezoWindow* window() { return window_; }
    void SetWindow(RezoWindow* w) { window_ = w; }

    IMPLEMENT_REFCOUNTING(RezoApp);

private:
    std::string UserAgent() const;
    RezoWindow* window_ = nullptr;
};