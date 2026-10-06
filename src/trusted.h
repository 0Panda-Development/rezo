#pragma once

#include <cctype>
#include <string>
#include <vector>

inline const std::vector<std::string> kTrustedDomains = {
    "youtube.com", "googlevideo.com", "ytimg.com", "youtube-nocookie.com", "google.com",
    "googleapis.com", "gstatic.com",
    "steampowered.com", "steamcommunity.com", "steamstatic.com", "steamcontent.com",
    "discord.com", "discordapp.com", "twitch.tv", "reddit.com", "roblox.com", "github.com",
    "cineb.cx",
};

inline bool IsTrustedHost(const std::string& host) {
    std::string h = host;
    for (char& c : h) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (!h.empty() && h.back() == '.') h.pop_back();
    for (const auto& d : kTrustedDomains) {
        if (h == d) return true;
        if (h.size() > d.size() &&
            h.compare(h.size() - d.size() - 1, d.size() + 1, "." + d) == 0)
            return true;
    }
    return false;
}

inline bool IsTrustedUrl(const std::string& url) {
    size_t scheme = url.find("://");
    size_t start = (scheme == std::string::npos) ? 0 : scheme + 3;
    size_t end = url.find_first_of("/?#", start);
    if (end == std::string::npos) end = url.length();
    std::string host = url.substr(start, end - start);
    size_t at = host.rfind('@');
    if (at != std::string::npos) host = host.substr(at + 1);
    size_t colon = host.rfind(':');
    if (colon != std::string::npos) host = host.substr(0, colon);
    return IsTrustedHost(host);
}