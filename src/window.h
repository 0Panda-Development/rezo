#pragma once

#include "include/cef_browser.h"
#include "include/cef_task.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_browser_view_delegate.h"
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
    CefRect GetInitialBounds(CefRefPtr<CefWindow> window) override { return CefRect(0, 0, 1280, 800); }

    // CefButtonDelegate
    void OnButtonPressed(CefRefPtr<CefButton> button) override;

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

class RebuildTabs : public CefTask {
public:
    explicit RebuildTabs(RezoWindow* owner) : owner_(owner) {}
    void Execute() override;
    IMPLEMENT_REFCOUNTING(RebuildTabs);
private:
    RezoWindow* owner_;
};

class TabViewDelegate : public CefBrowserViewDelegate {
public:
    TabViewDelegate(RezoWindow* owner, const std::string& url) : owner_(owner), url_(url) {}
    void OnBrowserCreated(CefRefPtr<CefBrowserView> view, CefRefPtr<CefBrowser> browser) override;
    IMPLEMENT_REFCOUNTING(TabViewDelegate);
private:
    RezoWindow* owner_;
    std::string url_;
};

class AssertTabUrl : public CefTask {
public:
    AssertTabUrl(RezoWindow* owner, CefRefPtr<CefBrowser> browser, const std::string& url)
        : owner_(owner), browser_(browser), url_(url) {}
    void Execute() override;
    IMPLEMENT_REFCOUNTING(AssertTabUrl);
private:
    RezoWindow* owner_;
    CefRefPtr<CefBrowser> browser_;
    std::string url_;
};

class RezoWindow {
public:
    RezoWindow();
    ~RezoWindow();

    void Create(const std::string& startUrl);
    void NavigateTo(const std::string& input);
    void OpenTab(const std::string& url);
    bool CloseTab(CefRefPtr<CefBrowser> browser);
    void SelectTab(int index);
    void SetTabTitle(CefRefPtr<CefBrowser> browser, const CefString& title);
    void SetAddress(const std::string& url);
    void UpdateTorStatus();
    void RebuildTabButtons();
    bool IsClosing() const { return closing_; }
    void DestroyWindow();
    CefRefPtr<CefBrowser> ActiveBrowser() const;

private:
    friend class Chrome;
    struct Tab {
        CefRefPtr<CefBrowserView> view;
        CefRefPtr<CefLabelButton> button;
    };
    int TabIndex(CefRefPtr<CefBrowser> browser) const;

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