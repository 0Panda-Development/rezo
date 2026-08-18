#include "newtab.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <windows.h>

#include "tor_manager.h"
#include "tor_state.h"

extern TorManager g_tor;

namespace {

std::string ResourceDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::filesystem::path p(buf);
    return p.parent_path().string();
}

std::string ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string EscapeHtml(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

std::string BuildTiles(const std::string& cfgPath) {
    std::ifstream f(cfgPath);
    std::string out;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string name = line.substr(0, tab);
        std::string url = line.substr(tab + 1);
        if (name.empty() || url.empty()) continue;
        out += "<a href=\"" + EscapeHtml(url) + "\">" + EscapeHtml(name) + "</a>\n";
    }
    return out;
}

}  // namespace

class NewTabHandler : public CefResourceHandler {
public:
    bool ProcessRequest(CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback) override {
        url_ = request->GetURL();
        callback->Continue();
        return true;
    }

    void GetResponseHeaders(CefRefPtr<CefResponse> response, int64_t& response_length,
                            CefString& redirect_url) override {
        response->SetMimeType("text/html");
        response->SetStatus(200);
        response->SetHeaderByName("Cache-Control", "no-store", true);
        body_ = BuildPage(url_);
        response_length = static_cast<int64_t>(body_.size());
    }

    bool ReadResponse(void* data_out, int bytes_to_read, int& bytes_read,
                      CefRefPtr<CefCallback> callback) override {
        if (offset_ >= body_.size()) {
            bytes_read = 0;
            return false;
        }
        size_t n = std::min<size_t>(bytes_to_read, body_.size() - offset_);
        memcpy(data_out, body_.data() + offset_, n);
        offset_ += n;
        bytes_read = static_cast<int>(n);
        return true;
    }

    void Cancel() override {}

    IMPLEMENT_REFCOUNTING(NewTabHandler);

private:
    std::string BuildPage(const std::string& url) {
        std::string dir = ResourceDir();
        std::string status = torStateName(g_tor.State());
        std::string statusText = std::string("Tor: ") + status;
        if (url.find("blocked") != std::string::npos) {
            std::string html = ReadFile(dir + "\\blocked.html");
            // {{STATUS}} placeholder is fine to leave if absent.
            return ReplaceAll(html, "{{STATUS}}", statusText);
        }
        std::string html = ReadFile(dir + "\\newtab.html");
        html = ReplaceAll(html, "{{TILES}}", BuildTiles(dir + "\\quickaccess.txt"));
        html = ReplaceAll(html, "{{STATUS}}", statusText);
        return html;
    }

    static std::string ReplaceAll(std::string s, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
        return s;
    }

    std::string url_;
    std::string body_;
    size_t offset_ = 0;
};

CefRefPtr<CefResourceHandler> NewTabFactory::Create(CefRefPtr<CefBrowser> browser,
                                                    CefRefPtr<CefFrame> frame,
                                                    const CefString& scheme_name,
                                                    CefRefPtr<CefRequest> request) {
    return new NewTabHandler();
}