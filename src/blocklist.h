#pragma once

#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Host-based ad/tracker blocklist, loaded once at startup from blocklist.txt
// next to the exe (plain "0.0.0.0 domain" hosts format). Matches the host and
// any subdomain of a blocked entry.
class Blocklist {
public:
    bool Load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;
        rev_.clear();
        std::string line;
        while (std::getline(f, line)) {
            size_t hash = line.find('#');
            if (hash != std::string::npos) line = line.substr(0, hash);
            size_t space = line.find_first_of(" \t");
            std::string domain = space == std::string::npos ? line : line.substr(space + 1);
            if (domain.empty()) continue;
            if (domain.find(':') != std::string::npos) continue;  // ipv6
            rev_.insert(std::string(domain.rbegin(), domain.rend()));
        }
        return !rev_.empty();
    }

    bool Matches(const std::string& host) const {
        std::string rev(host.rbegin(), host.rend());
        size_t pos = rev.size();
        while (true) {
            if (rev_.count(rev.substr(0, pos))) return true;
            size_t dot = rev.rfind('.', pos - 1);
            if (dot == std::string::npos) return false;
            pos = dot;
        }
    }

private:
    std::unordered_set<std::string> rev_;
};

// EasyList-style URL rules (urlrules.txt, lowercased, one per line).
// Rules are grouped by their leading host so a request only scans the few
// patterns belonging to its own host chain (not all 20k).
class UrlRules {
public:
    bool Load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;
        byHost_.clear();
        generic_.clear();
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#' || line[0] == '!') continue;
            size_t slash = line.find('/');
            size_t dot = line.find('.');
            if (slash != std::string::npos && dot != std::string::npos && dot < slash) {
                byHost_[line.substr(0, slash)].push_back(line.substr(slash));
            } else {
                generic_.push_back(line);
            }
        }
        return !byHost_.empty() || !generic_.empty();
    }

    bool Matches(const std::string& url) const {
        std::string u(url);
        std::transform(u.begin(), u.end(), u.begin(), ::tolower);
        for (const std::string& r : generic_)
            if (u.find(r) != std::string::npos) return true;
        size_t scheme = u.find("://");
        size_t start = scheme == std::string::npos ? 0 : scheme + 3;
        size_t end = u.find_first_of("/?#", start);
        std::string host = u.substr(start, end == std::string::npos ? std::string::npos : end - start);
        size_t colon = host.find(':');
        if (colon != std::string::npos) host = host.substr(0, colon);
        while (!host.empty()) {
            auto it = byHost_.find(host);
            if (it != byHost_.end())
                for (const std::string& p : it->second)
                    if (u.find(p) != std::string::npos) return true;
            size_t dot = host.find('.');
            if (dot == std::string::npos) break;
            host = host.substr(dot + 1);
        }
        return false;
    }

private:
    std::unordered_map<std::string, std::vector<std::string>> byHost_;
    std::vector<std::string> generic_;
};