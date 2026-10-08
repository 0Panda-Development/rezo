#pragma once

#include <string>
#include <functional>

class DiscordRPC {
public:
    static DiscordRPC& Instance() {
        static DiscordRPC instance;
        return instance;
    }

    ~DiscordRPC();

    bool Initialize(const char* applicationId);
    void Shutdown();

    void UpdatePresence(const char* details, const char* state, 
                        const char* largeImageKey = nullptr, const char* largeImageText = nullptr,
                        const char* smallImageKey = nullptr, const char* smallImageText = nullptr,
                        int64_t startTimestamp = 0, int64_t endTimestamp = 0);

    void ClearPresence();

    bool IsInitialized() const { return initialized_; }

    // Call this periodically (e.g., from a timer) to process callbacks
    void RunCallbacks();

private:
    DiscordRPC() = default;
    DiscordRPC(const DiscordRPC&) = delete;
    DiscordRPC& operator=(const DiscordRPC&) = delete;

    void* handle_ = nullptr;
    bool initialized_ = false;

    // Function pointers
    using InitializeFunc = void(*)(const char*, void*, int, const char*);
    using ShutdownFunc = void(*)();
    using UpdatePresenceFunc = void(*)(const void*);
    using ClearPresenceFunc = void(*)();
    using RunCallbacksFunc = void(*)();

    InitializeFunc discordInitialize_ = nullptr;
    ShutdownFunc discordShutdown_ = nullptr;
    UpdatePresenceFunc discordUpdatePresence_ = nullptr;
    ClearPresenceFunc discordClearPresence_ = nullptr;
    RunCallbacksFunc discordRunCallbacks_ = nullptr;

    bool LoadLibrary();
};

struct DiscordRichPresence {
    const char* state;
    const char* details;
    const char* largeImageKey;
    const char* largeImageText;
    const char* smallImageKey;
    const char* smallImageText;
    int64_t startTimestamp;
    int64_t endTimestamp;
    const char* partyId;
    int partySize;
    int partyMax;
    const char* matchSecret;
    const char* joinSecret;
    const char* spectateSecret;
    int8_t instance;
};