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

std::string EnsureConfig(const std::string& cfgPath) {
    if (std::filesystem::exists(cfgPath)) return cfgPath;
    std::ofstream f(cfgPath);
    if (f) {
        f << "# Rezo quick access tiles\n"
          << "# One tile per line: Name<TAB>URL\n\n"
          << "Steam\thttps://store.steampowered.com\n"
          << "Discord\thttps://discord.com/app\n"
          << "Roblox\thttps://www.roblox.com\n"
          << "Reddit\thttps://www.reddit.com\n"
          << "YouTube\thttps://www.youtube.com\n";
    }
    return cfgPath;
}

std::string HostClass(const std::string& url) {
    struct { const char* host; const char* cls; } map[] = {
        {"steampowered.com", "steam"}, {"discord.com", "discord"},
        {"roblox.com", "roblox"}, {"reddit.com", "reddit"}, {"youtube.com", "youtube"},
    };
    for (const auto& m : map) {
        if (url.find(m.host) != std::string::npos) return m.cls;
    }
    return "";
}

std::string StatusClass(TorState s) {
    switch (s) {
        case TorState::Disconnected: return "disconnected";
        case TorState::Starting:     return "starting";
        case TorState::Connected:    return "connected";
        case TorState::Blocked:      return "blocked";
    }
    return "disconnected";
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
        std::string cls = HostClass(url);
        out += "<a href=\"" + EscapeHtml(url) + "\"";
        if (!cls.empty()) out += " data-host=\"" + cls + "\"";
        out += ">" + EscapeHtml(name) + "</a>\n";
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
        if (url.find("retry") != std::string::npos) {
            g_tor.StartAsync();
            return "<html><body style='margin:0;min-height:100vh;background:#0b0b12;color:#e8e8f0;font-family:sans-serif;text-align:center;padding-top:120px;background-image:radial-gradient(60% 50% at 20% 0%,rgba(251,191,36,0.12),transparent 60%)'>"
                   "<h1 style='font-size:40px;letter-spacing:6px;color:#fbbf24;text-shadow:0 0 22px rgba(251,191,36,0.55)'>TOR STARTING</h1>"
                   "<p>Waiting for the Tor circuit...</p>"
                   "<a href='rezo://newtab/' style='display:inline-block;margin-top:34px;padding:12px 28px;background:#7c5cff;color:#fff;border-radius:8px;text-decoration:none;font-weight:600;box-shadow:0 0 18px rgba(124,92,255,0.5)'>Back</a></body></html>";
        }
        if (url.find("blocked") != std::string::npos) {
            std::string html = ReadFile(dir + "\\blocked.html");
            // {{STATUS}} placeholder is fine to leave if absent.
            html = ReplaceAll(html, "{{STATUSCLASS}}", StatusClass(g_tor.State()));
            return ReplaceAll(html, "{{STATUS}}", statusText);
        }
        std::string html = ReadFile(dir + "\\newtab.html");
        html = ReplaceAll(html, "{{TILES}}", BuildTiles(EnsureConfig(dir + "\\quickaccess.txt")));
        html = ReplaceAll(html, "{{STATUSCLASS}}", StatusClass(g_tor.State()));
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