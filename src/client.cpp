#include "client.h"

#include <algorithm>
#include <atomic>
#include <string>
#include <shlobj.h>
#include <windows.h>

#include "app.h"
#include "include/cef_parser.h"
#include "tor_manager.h"
#include "tor_state.h"
#include "trusted.h"
#include "window.h"

extern TorManager g_tor;

namespace {

bool IsLocalUrl(const CefString& url) {
    return url.ToString().rfind("rezo://", 0) == 0 ||
           url.ToString().rfind("file:", 0) == 0 ||
           url.ToString().rfind("data:", 0) == 0;
}

std::wstring DownloadDir() {
    wchar_t profile[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, SHGFP_TYPE_CURRENT, profile) != S_OK)
        return L"C:\\Users\\Public\\Downloads";
    return std::wstring(profile) + L"\\Downloads";
}

void ShowBlockedPage(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) {
    if (frame->IsMain()) frame->LoadURL("rezo://newtab/blocked");
}

}  // namespace

bool RezoClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefRequest> request, bool user_gesture,
                                bool is_redirect) {
    if (!frame->IsMain()) return false;
    if (IsLocalUrl(request->GetURL())) return false;
    if (shouldBlockRequest(g_tor.State()) && !IsTrustedUrl(request->GetURL().ToString())) {
        ShowBlockedPage(browser, frame);
        return true;  // cancel navigation
    }
    return false;
}

CefRefPtr<CefResourceRequestHandler> RezoClient::GetResourceRequestHandler(
    CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request, bool is_navigation, bool is_download,
    const CefString& request_initiator, bool& disable_default_handling) {
    return this;
}

CefResourceRequestHandler::ReturnValue RezoClient::OnBeforeResourceLoad(
    CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback) {
    CefString url = request->GetURL();
    if (IsLocalUrl(url)) return RV_CONTINUE;
    bool trusted = IsTrustedUrl(url.ToString());
    if (!trusted) request->SetFlags(request->GetFlags() | UR_FLAG_SKIP_CACHE);
    if (shouldBlockRequest(g_tor.State()) && !trusted) {
        callback->Cancel();
        ShowBlockedPage(browser, frame);
        return RV_CANCEL;
    }
    return RV_CONTINUE;
}

bool RezoClient::OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                               int popup_id, const CefString& target_url,
                               const CefString& target_frame_name,
                               CefLifeSpanHandler::WindowOpenDisposition target_disposition,
                               bool user_gesture,
                               const CefPopupFeatures& popup_features,
                               CefWindowInfo& window_info, CefRefPtr<CefClient>& client,
                               CefBrowserSettings& settings,
                               CefRefPtr<CefDictionaryValue>& extra_info,
                               bool* no_javascript_access) {
    // Keep it simple: allow popups. They open as plain native windows but
    // still go through this client, so proxy + kill switch apply. Tab-ifying
    // popups is a v2 improvement.
    return false;
}

bool RezoClient::DoClose(CefRefPtr<CefBrowser> browser) {
    return window_->CloseTab(browser);
}

void RezoClient::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) {
    window_->SetTabTitle(browser, title);
}

void RezoClient::OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                 const CefString& url) {
    if (frame->IsMain() && window_->ActiveBrowser() == browser) {
        window_->SetAddress(url.ToString());
    }
}

void RezoClient::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                           int httpStatusCode) {
    if (frame->IsMain()) {
        window_->SetAddress(frame->GetURL().ToString());
    }
}

void RezoClient::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                             ErrorCode errorCode, const CefString& errorText,
                             const CefString& failedUrl) {
    // Only Tor being down (kill switch active) redirects to the blocked page;
    // cert/DNS errors are normal browsing errors and keep the native page.
    if (errorCode != ERR_ABORTED && !IsLocalUrl(failedUrl) && frame->IsMain() &&
        frame->IsValid() && shouldBlockRequest(g_tor.State())) {
        ShowBlockedPage(browser, frame);
    }
}

bool RezoClient::OnBeforeDownload(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefDownloadItem> download_item,
                                  const CefString& suggested_name,
                                  CefRefPtr<CefBeforeDownloadCallback> callback) {
    std::wstring name = suggested_name.ToWString();
    for (auto& c : name) {
        if (c == L'/' || c == L'\\' || c == L':') c = L'_';
    }
    if (name.empty()) name = L"download";
    callback->Continue(DownloadDir() + L"\\" + name, false);
    return true;
}

void RezoClient::OnDownloadUpdated(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefDownloadItem> download_item,
                                   CefRefPtr<CefDownloadItemCallback> callback) {
    if (download_item->IsComplete()) {
        window_->SetDownloadStatus("");
    } else if (download_item->IsInProgress()) {
        window_->SetDownloadStatus("Downloading " +
                                   download_item->GetSuggestedFileName().ToString() + " " +
                                   std::to_string(download_item->GetPercentComplete()) + "%");
    }
}

bool RezoClient::OnPreKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event,
                               CefEventHandle os_event, bool* is_keyboard_shortcut) {
    if (event.type != KEYEVENT_RAWKEYDOWN) return false;
    const int key = event.windows_key_code;
    const bool ctrl = (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0;
    const bool alt = (event.modifiers & EVENTFLAG_ALT_DOWN) != 0;
    const bool shift = (event.modifiers & EVENTFLAG_SHIFT_DOWN) != 0;
    if (ctrl && key == 'L') {
        window_->FocusAddress();
        return true;
    }
    if (ctrl && key == 'W') {
        window_->CloseTab(browser);
        return true;
    }
    if (ctrl && key == 'R') {
        if (shift) browser->ReloadIgnoreCache();
        else browser->Reload();
        return true;
    }
    if (ctrl && (key == VK_OEM_PLUS || key == VK_ADD)) {
        window_->ApplyZoom(0.5);
        return true;
    }
    if (ctrl && (key == VK_OEM_MINUS || key == VK_SUBTRACT)) {
        window_->ApplyZoom(-0.5);
        return true;
    }
    if (ctrl && (key == '0' || key == VK_NUMPAD0)) {
        window_->ApplyZoomReset();
        return true;
    }
    if (alt && key == VK_LEFT) {
        browser->GoBack();
        return true;
    }
    if (alt && key == VK_RIGHT) {
        browser->GoForward();
        return true;
    }
    if (key == VK_F11) {
        window_->ToggleFullscreen();
        return true;
    }
    if (key == VK_ESCAPE) {
        browser->StopLoad();
        return true;
    }
    return false;
}