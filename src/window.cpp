#include "window.h"

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/views/cef_window.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_box_layout.h"

#include "app.h"
#include "client.h"

extern RezoApp* g_app;

namespace {

class WindowDelegate : public CefWindowDelegate {
public:
    explicit WindowDelegate(RezoWindow* owner) : owner_(owner) {}

    void OnWindowCreated(CefRefPtr<CefWindow> window) override {
        CefBrowserSettings settings;
        CefRefPtr<CefBrowserView> view = CefBrowserView::CreateBrowserView(
            new RezoClient(owner_), startUrl_, settings, nullptr, nullptr, nullptr);
        window->AddChildView(view);
        window->Show();
    }

    void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
        owner_->DestroyWindow();
        CefQuitMessageLoop();
    }

    CefRect GetInitialBounds(CefRefPtr<CefWindow> window) override {
        return CefRect(0, 0, 1280, 800);
    }
    IMPLEMENT_REFCOUNTING(WindowDelegate);

public:
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
    g_app->SetWindow(nullptr);
}