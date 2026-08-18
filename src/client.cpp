#include "client.h"

#include "app.h"
#include "tor_manager.h"
#include "tor_state.h"
#include "window.h"

extern TorManager g_tor;

namespace {

bool IsLocalUrl(const CefString& url) {
    return url.ToString().rfind("rezo://", 0) == 0 ||
           url.ToString().rfind("file:", 0) == 0 ||
           url.ToString().rfind("data:", 0) == 0;
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
    if (shouldBlockRequest(g_tor.State())) {
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
    if (IsLocalUrl(request->GetURL())) return RV_CONTINUE;
    if (shouldBlockRequest(g_tor.State())) {
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
    if (frame->IsMain()) window_->SetAddress(url.ToString());
}

void RezoClient::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                             ErrorCode errorCode, const CefString& errorText,
                             const CefString& failedUrl) {
    if (errorCode != ERR_ABORTED && !IsLocalUrl(failedUrl) && frame->IsMain()) {
        // Proxy/Tor is down: show the blocked page instead of the error.
        ShowBlockedPage(browser, frame);
    }
}