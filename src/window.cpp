#include "window.h"

#include "include/cef_parser.h"
#include "include/cef_task.h"

#include "app.h"
#include "client.h"
#include "tor_manager.h"
#include "tor_state.h"

extern RezoApp* g_app;
extern TorManager g_tor;

namespace {

enum {
    ID_BACK = 1,
    ID_FORWARD,
    ID_RELOAD,
    ID_NEWTAB,
    ID_TAB_FIRST = 100,
};

// CEF headers don't pull in windows.h; VK_RETURN is 0x0D on Windows.
constexpr int kEnterKeyCode = 0x0D;

std::string TrimTitle(const std::string& s) {
    return s.size() > 24 ? s.substr(0, 24) + "..." : s;
}

}  // namespace

RezoWindow::RezoWindow() : chrome_(new Chrome(this)) {}
RezoWindow::~RezoWindow() = default;

void Chrome::OnWindowCreated(CefRefPtr<CefWindow> window) {
    RezoWindow* w = owner_;

    w->window_ = window;
    window->SetTitle("Rezo");
    window->SetBackgroundColor(0xFF101014);

    // Root: vertical [tab bar, top bar, browser area]
    CefRefPtr<CefPanel> root = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings vs;
    vs.horizontal = 0;  // CEF 151: int flag replaces the orientation enum
    vs.between_child_spacing = 2;
    root->SetToBoxLayout(vs);
    w->root_ = root;

    // Tab strip
    CefRefPtr<CefPanel> tabBar = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings ts;
    ts.horizontal = 1;
    ts.between_child_spacing = 2;
    tabBar->SetToBoxLayout(ts);
    w->tabBar_ = tabBar;
    w->plus_ = CefLabelButton::CreateLabelButton(this, "+");
    w->plus_->SetID(ID_NEWTAB);
    tabBar->AddChildView(w->plus_);

    // Top bar: back / forward / reload / address / status
    CefRefPtr<CefPanel> bar = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings hs;
    hs.horizontal = 1;
    hs.between_child_spacing = 4;
    bar->SetToBoxLayout(hs);

    CefRefPtr<CefLabelButton> back = CefLabelButton::CreateLabelButton(this, "◀");
    back->SetID(ID_BACK);
    back->SetTooltipText("Back");
    CefRefPtr<CefLabelButton> fwd = CefLabelButton::CreateLabelButton(this, "▶");
    fwd->SetID(ID_FORWARD);
    fwd->SetTooltipText("Forward");
    CefRefPtr<CefLabelButton> reload = CefLabelButton::CreateLabelButton(this, "⟳");
    reload->SetID(ID_RELOAD);
    reload->SetTooltipText("Reload");
    w->address_ = CefTextfield::CreateTextfield(this);
    w->address_->SetPlaceholderText("Enter address or search");
    // CEF 151 crashes on a null button delegate (brief passed nullptr): use
    // chrome_ instead; SetEnabled(false) keeps it non-interactive.
    w->status_ = CefLabelButton::CreateLabelButton(this, "");
    w->status_->SetEnabled(false);

    bar->AddChildView(back);
    bar->AddChildView(fwd);
    bar->AddChildView(reload);
    bar->AddChildView(w->address_);
    bar->GetLayout()->AsBoxLayout()->SetFlexForView(w->address_, 1);
    bar->AddChildView(w->status_);

    // First tab
    CefBrowserSettings settings;
    CefRefPtr<CefBrowserView> view = CefBrowserView::CreateBrowserView(
        new RezoClient(w), w->startUrl_, settings, nullptr, nullptr, nullptr);

    root->AddChildView(tabBar);
    root->AddChildView(bar);
    root->AddChildView(view);
    root->GetLayout()->AsBoxLayout()->SetFlexForView(view, 1);

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
    else if (id >= ID_TAB_FIRST) w->SelectTab(id - ID_TAB_FIRST);
}

bool Chrome::OnKeyEvent(CefRefPtr<CefTextfield> textfield, const CefKeyEvent& event) {
    if (event.type == KEYEVENT_RAWKEYDOWN && event.windows_key_code == kEnterKeyCode) {
        owner_->NavigateTo(textfield->GetText().ToString());
        return true;
    }
    return false;
}

void StatusTicker::Execute() {
    if (owner_->IsClosing()) return;
    owner_->UpdateTorStatus();
    CefPostDelayedTask(TID_UI, this, 1000);
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
    RebuildTabButtons();
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
            label = TrimTitle(b->GetMainFrame()->GetURL().ToString());
            if (label.empty()) label = "New Tab";
        }
        if (static_cast<int>(i) == selected_) label = "● " + label;
        CefRefPtr<CefLabelButton> b2 = CefLabelButton::CreateLabelButton(chrome_, label);
        b2->SetID(static_cast<int>(ID_TAB_FIRST + i));
        tabBar_->AddChildView(b2);
        tabs_[i].button = b2;
    }
}

void RezoWindow::OpenTab(const std::string& url) {
    std::string target = url.empty() ? "rezo://newtab/" : url;
    CefRefPtr<CefBrowserView> view = CefBrowserView::CreateBrowserView(
        new RezoClient(this), target, CefBrowserSettings(), nullptr, nullptr, nullptr);
    root_->AddChildView(view);
    root_->GetLayout()->AsBoxLayout()->SetFlexForView(view, 1);
    tabs_.push_back({view, nullptr});
    SelectTab(static_cast<int>(tabs_.size()) - 1);
}

void RezoWindow::CloseTab(CefRefPtr<CefBrowser> browser) {
    int idx = TabIndex(browser);
    if (idx < 0) return;
    if (tabs_.size() <= 1) {
        // Last tab: close the window.
        if (window_) window_->Close();
        return;
    }
    CefRefPtr<CefBrowserView> view = tabs_[idx].view;
    tabs_.erase(tabs_.begin() + idx);
    root_->RemoveChildView(view);
    view->GetBrowser()->GetHost()->CloseBrowser(true);
    SelectTab(std::min(selected_, static_cast<int>(tabs_.size()) - 1));
    RebuildTabButtons();
}

void RezoWindow::NavigateTo(const std::string& input) {
    std::string url = input;
    if (url.find("://") == std::string::npos) {
        if (url.find('.') == std::string::npos) {
            // Looks like a search query.
            url = "https://html.duckduckgo.com/html/?q=" + CefURIEncode(url, false).ToString();
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
    if (tabs_[idx].button) tabs_[idx].button->SetText(TrimTitle(title.ToString()));
}

void RezoWindow::SetAddress(const std::string& url) {
    if (address_) address_->SetText(url);
}

void RezoWindow::UpdateTorStatus() {
    if (status_) status_->SetText(std::string("Tor: ") + torStateName(g_tor.State()));
}