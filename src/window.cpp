#include "window.h"

#include "include/cef_color_ids.h"
#include "include/cef_image.h"
#include "include/cef_parser.h"
#include "include/cef_task.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <dwmapi.h>
#include <windows.h>

#include "app.h"
#include "client.h"
#include "tor_manager.h"
#include "tor_state.h"

extern RezoApp* g_app;
extern TorManager g_tor;

#define IDI_APP 101

namespace {

enum {
    ID_BACK = 1,
    ID_FORWARD,
    ID_RELOAD,
    ID_NEWTAB,
    ID_TAB_FIRST = 100,
    ID_LINK_FIRST = 1000,
};

struct QuickLink {
    const char* label;
    const char* url;
};
const QuickLink kQuickLinks[] = {
    {"Cineb", "https://cineb.cx"},
    {"Playzip", "https://playzip.com"},
};
constexpr size_t kQuickLinkCount = sizeof(kQuickLinks) / sizeof(kQuickLinks[0]);

// CEF headers don't pull in windows.h; VK_RETURN is 0x0D on Windows.
constexpr int kEnterKeyCode = 0x0D;

// Dark cyber theme (cef_color_t is ARGB).
constexpr cef_color_t kBg = 0xFF0B0F1A;          // window / panels
constexpr cef_color_t kBgTabActive = 0xFF16233A; // selected tab
constexpr cef_color_t kBgField = 0xFF0E1422;     // address bar
constexpr cef_color_t kBgFieldFocus = 0xFF12303C;
constexpr cef_color_t kAccent = 0xFF4FE3FF;      // cyan
constexpr cef_color_t kTextDim = 0xFF8B93A7;
constexpr cef_color_t kText = 0xFFE6EAF2;
constexpr cef_color_t kOk = 0xFF34D399;
constexpr cef_color_t kWarn = 0xFFF59E0B;
constexpr cef_color_t kBad = 0xFFF87171;

std::string TrimTitle(const std::string& s) {
    return s.size() > 24 ? s.substr(0, 24) + "..." : s;
}

CefRefPtr<CefImage> LoadUiIcon(const std::string& name) {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::filesystem::path p(buf);
    std::ifstream f((p.parent_path() / "ui" / (name + ".png")).string(), std::ios::binary);
    if (!f) return nullptr;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CefRefPtr<CefImage> img = CefImage::CreateImage();
    if (!img->AddPNG(1.0f, data.data(), data.size())) return nullptr;
    return img;
}

void StyleToolbarButton(CefRefPtr<CefLabelButton> b, const std::string& icon) {
    b->SetText("");
    b->SetBackgroundColor(kBg);
    b->SetMinimumSize(CefSize(30, 28));
    b->SetInkDropEnabled(true);
    b->SetImage(CEF_BUTTON_STATE_NORMAL, LoadUiIcon(icon));
    b->SetImage(CEF_BUTTON_STATE_HOVERED, LoadUiIcon(icon + "_hover"));
    b->SetImage(CEF_BUTTON_STATE_PRESSED, LoadUiIcon(icon + "_hover"));
}

}  // namespace

RezoWindow::RezoWindow() : chrome_(new Chrome(this)) {}
RezoWindow::~RezoWindow() = default;

void Chrome::OnWindowCreated(CefRefPtr<CefWindow> window) {
    RezoWindow* w = owner_;

    w->window_ = window;
    window->SetTitle("Rezo");

    // CEF views windows default to the CEF/chrome icon; swap in Rezo's icon.
    HWND hwnd = static_cast<HWND>(window->GetWindowHandle());
    HICON icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCE(IDI_APP));
    if (hwnd && icon) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
    }
    // Dark title bar + rounded corners (Win11); no-op on older Windows.
    if (hwnd) {
        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
        DWORD round = 2 /*DWMWCP_ROUND*/;
        DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &round, sizeof(round));
    }
    window->SetBackgroundColor(kBg);

    // Root: vertical [tab bar, top bar, browser area]
    CefRefPtr<CefPanel> root = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings vs;
    vs.horizontal = 0;  // CEF 151: int flag replaces the orientation enum
    vs.between_child_spacing = 2;
    root->SetToBoxLayout(vs);
    root->SetBackgroundColor(kBg);
    w->root_ = root;

    // Tab strip
    CefRefPtr<CefPanel> tabBar = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings ts;
    ts.horizontal = 1;
    ts.between_child_spacing = 2;
    tabBar->SetToBoxLayout(ts);
    tabBar->SetBackgroundColor(kBg);
    w->tabBar_ = tabBar;
    w->plus_ = CefLabelButton::CreateLabelButton(this, "");
    w->plus_->SetID(ID_NEWTAB);
    w->plus_->SetTooltipText("New tab (Ctrl+T)");
    StyleToolbarButton(w->plus_, "plus");

    // Top bar: back / forward / reload / address / status
    CefRefPtr<CefPanel> bar = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings hs;
    hs.horizontal = 1;
    hs.between_child_spacing = 4;
    bar->SetToBoxLayout(hs);
    bar->SetBackgroundColor(kBg);

    CefRefPtr<CefLabelButton> back = CefLabelButton::CreateLabelButton(this, "");
    back->SetID(ID_BACK);
    back->SetTooltipText("Back (Alt+Left)");
    StyleToolbarButton(back, "back");
    CefRefPtr<CefLabelButton> fwd = CefLabelButton::CreateLabelButton(this, "");
    fwd->SetID(ID_FORWARD);
    fwd->SetTooltipText("Forward (Alt+Right)");
    StyleToolbarButton(fwd, "fwd");
    CefRefPtr<CefLabelButton> reload = CefLabelButton::CreateLabelButton(this, "");
    reload->SetID(ID_RELOAD);
    reload->SetTooltipText("Reload (Ctrl+R)");
    StyleToolbarButton(reload, "reload");
    w->address_ = CefTextfield::CreateTextfield(this);
    w->address_->SetPlaceholderText("Enter address or search");
    w->address_->SetBackgroundColor(kBgField);
    w->address_->SetFontList("Segoe UI, 11px");
    w->address_->ApplyTextColor(kText, CefRange(0, 0));
    // CEF 151 crashes on a null button delegate (brief passed nullptr): use
    // chrome_ instead; SetEnabled(false) keeps it non-interactive.
    w->status_ = CefLabelButton::CreateLabelButton(this, "");
    w->status_->SetEnabled(false);
    w->status_->SetBackgroundColor(kBg);
    w->status_->SetFontList("Segoe UI, 10px");

    bar->AddChildView(back);
    bar->AddChildView(fwd);
    bar->AddChildView(reload);
    bar->AddChildView(w->address_);
    bar->GetLayout()->AsBoxLayout()->SetFlexForView(w->address_, 1);
    bar->AddChildView(w->status_);

    // First tab
    CefBrowserSettings settings;
    settings.background_color = kBg;  // transparent pages show the window background
    CefRefPtr<CefBrowserView> view = CefBrowserView::CreateBrowserView(
        new RezoClient(w), w->startUrl_, settings, nullptr, nullptr, nullptr);

    // Middle: sidebar (built-in links) + browser area
    CefRefPtr<CefPanel> middle = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings ms;
    ms.horizontal = 1;
    middle->SetToBoxLayout(ms);
    middle->SetBackgroundColor(kBg);
    w->middle_ = middle;

    CefRefPtr<CefPanel> sidebar = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings ss;
    ss.horizontal = 0;
    ss.between_child_spacing = 2;
    ss.inside_border_insets = CefInsets(4, 4, 4, 4);
    sidebar->SetToBoxLayout(ss);
    sidebar->SetBackgroundColor(kBgField);
    sidebar->SetSize(CefSize(120, 0));
    w->sidebar_ = sidebar;
    for (size_t i = 0; i < kQuickLinkCount; ++i) {
        CefRefPtr<CefLabelButton> b =
            CefLabelButton::CreateLabelButton(this, kQuickLinks[i].label);
        b->SetID(static_cast<int>(ID_LINK_FIRST + i));
        b->SetFontList("Segoe UI, 10px");
        b->SetBackgroundColor(kBgField);
        b->SetInkDropEnabled(true);
        b->SetMinimumSize(CefSize(112, 26));
        b->SetTextColor(CEF_BUTTON_STATE_NORMAL, kTextDim);
        b->SetTextColor(CEF_BUTTON_STATE_HOVERED, kText);
        b->SetTextColor(CEF_BUTTON_STATE_PRESSED, kText);
        sidebar->AddChildView(b);
    }

    middle->AddChildView(sidebar);
    middle->AddChildView(view);
    middle->GetLayout()->AsBoxLayout()->SetFlexForView(view, 1);

    root->AddChildView(bar);
    root->AddChildView(middle);
    root->GetLayout()->AsBoxLayout()->SetFlexForView(middle, 1);

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

void Chrome::OnButtonPressed(CefRefPtr<CefButton> button) {
    RezoWindow* w = owner_;
    int id = button->GetID();
    CefRefPtr<CefBrowser> browser = w->ActiveBrowser();
    if (id == ID_BACK && browser) browser->GoBack();
    else if (id == ID_FORWARD && browser) browser->GoForward();
    else if (id == ID_RELOAD && browser) browser->Reload();
    else if (id == ID_NEWTAB) w->OpenTab("");
    else if (id >= ID_LINK_FIRST) {
        size_t i = static_cast<size_t>(id - ID_LINK_FIRST);
        if (i < kQuickLinkCount && browser) browser->GetMainFrame()->LoadURL(kQuickLinks[i].url);
    }
    else if (id >= ID_TAB_FIRST) w->SelectTab(id - ID_TAB_FIRST);
}

bool Chrome::OnKeyEvent(CefRefPtr<CefTextfield> textfield, const CefKeyEvent& event) {
    if (event.type == KEYEVENT_RAWKEYDOWN && event.windows_key_code == kEnterKeyCode) {
        owner_->NavigateTo(textfield->GetText().ToString());
        return true;
    }
    return false;
}

void Chrome::OnFocus(CefRefPtr<CefView> view) {
    if (owner_->address_ && view->IsSame(owner_->address_))
        owner_->address_->SetBackgroundColor(kBgFieldFocus);
}

void Chrome::OnBlur(CefRefPtr<CefView> view) {
    if (owner_->address_ && view->IsSame(owner_->address_))
        owner_->address_->SetBackgroundColor(kBgField);
}

void StatusTicker::Execute() {
    if (owner_->IsClosing()) return;
    owner_->UpdateTorStatus();
    CefPostDelayedTask(TID_UI, this, 1000);
}

void RebuildTabs::Execute() {
    if (owner_->IsClosing()) return;
    owner_->RebuildTabButtons();
}

void TabViewDelegate::OnBrowserCreated(CefRefPtr<CefBrowserView> view,
                                      CefRefPtr<CefBrowser> browser) {
    browser->GetMainFrame()->LoadURL(url_);
    owner_->SetAddress(url_);
    CefPostDelayedTask(TID_UI, new AssertTabUrl(owner_, browser, url_), 1000);
}

void AssertTabUrl::Execute() {
    if (owner_->IsClosing()) return;
    CefRefPtr<CefFrame> frame = browser_->GetMainFrame();
    if (frame && frame->GetURL().ToString() != url_) {
        frame->LoadURL(url_);
        owner_->SetAddress(url_);
    }
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
    CefPostTask(TID_UI, new RebuildTabs(this));
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
        std::string label = "New Tab";
        CefRefPtr<CefBrowser> b = tabs_[i].view->GetBrowser();
        if (b) {
            std::string u = b->GetMainFrame()->GetURL().ToString();
            if (u.rfind("rezo://", 0) != 0 && !u.empty()) label = TrimTitle(u);
        }
        bool active = static_cast<int>(i) == selected_;
        if (active) label = "● " + label;
        CefRefPtr<CefLabelButton> b2 = CefLabelButton::CreateLabelButton(chrome_, label);
        b2->SetID(static_cast<int>(ID_TAB_FIRST + i));
        b2->SetFontList("Segoe UI, 10px");
        b2->SetBackgroundColor(active ? kBgTabActive : kBg);
        b2->SetInkDropEnabled(true);
        b2->SetTextColor(CEF_BUTTON_STATE_NORMAL, active ? kAccent : kTextDim);
        b2->SetTextColor(CEF_BUTTON_STATE_HOVERED, kText);
        b2->SetTextColor(CEF_BUTTON_STATE_PRESSED, kText);
        tabBar_->AddChildView(b2);
        tabs_[i].button = b2;
    }
}

void RezoWindow::OpenTab(const std::string& url) {
    std::string target = url.empty() ? "rezo://newtab/" : url;
    CefBrowserSettings settings;
    settings.background_color = kBg;  // transparent pages show the window background
    CefRefPtr<CefBrowserView> view = CefBrowserView::CreateBrowserView(
        new RezoClient(this), "about:blank", settings, nullptr, nullptr,
        new TabViewDelegate(this, target));
    middle_->AddChildView(view);
    middle_->GetLayout()->AsBoxLayout()->SetFlexForView(view, 1);
    tabs_.push_back({view, nullptr});
    SelectTab(static_cast<int>(tabs_.size()) - 1);
}

bool RezoWindow::CloseTab(CefRefPtr<CefBrowser> browser) {
    int idx = TabIndex(browser);
    if (idx < 0) return false;
    if (tabs_.size() <= 1) {
        // Last tab: close the window.
        if (window_) window_->Close();
        return true;
    }
    CefRefPtr<CefBrowserView> view = tabs_[idx].view;
    tabs_.erase(tabs_.begin() + idx);
    view->GetBrowser()->GetHost()->CloseBrowser(true);
    middle_->RemoveChildView(view);
    SelectTab(std::min(selected_, static_cast<int>(tabs_.size()) - 1));
    RebuildTabButtons();
    return true;
}

void RezoWindow::NavigateTo(const std::string& input) {
    std::string url = input;
    if (url.find("://") == std::string::npos) {
        if (url.find(' ') != std::string::npos || url.find('.') == std::string::npos) {
            // Search query.
            url = "https://www.google.com/search?q=" + CefURIEncode(url, false).ToString();
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
    if (tabs_[idx].button) {
        std::string t = TrimTitle(title.ToString());
        if (t.empty()) t = "New Tab";
        if (idx == selected_) t = "● " + t;
        tabs_[idx].button->SetText(t);
    }
}

void RezoWindow::SetAddress(const std::string& url) {
    if (address_) address_->SetText(url);
}

void RezoWindow::UpdateTorStatus() {
    if (status_) {
        std::string text = std::string("Tor: ") + torStateName(g_tor.State());
        if (!downloadStatus_.empty()) text += "  |  " + downloadStatus_;
        status_->SetText(text);
        cef_color_t c = kWarn;
        switch (g_tor.State()) {
            case TorState::Connected: c = kOk; break;
            case TorState::Disconnected:
            case TorState::Blocked: c = kBad; break;
            default: break;
        }
        status_->SetTextColor(CEF_BUTTON_STATE_DISABLED, c);
    }
}

void RezoWindow::SetDownloadStatus(const std::string& text) {
    downloadStatus_ = text;
    UpdateTorStatus();
}

void RezoWindow::FocusAddress() {
    if (address_) {
        address_->RequestFocus();
        address_->SelectAll(false);
    }
}

void RezoWindow::ApplyZoom(double delta) {
    zoom_ += delta;
    if (CefRefPtr<CefBrowser> b = ActiveBrowser()) b->GetHost()->SetZoomLevel(zoom_);
}

void RezoWindow::ApplyZoomReset() {
    zoom_ = 0.0;
    if (CefRefPtr<CefBrowser> b = ActiveBrowser()) b->GetHost()->SetZoomLevel(0.0);
}

void RezoWindow::ToggleFullscreen() {
    if (window_) window_->SetFullscreen(!window_->IsFullscreen());
}