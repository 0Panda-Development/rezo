#include "discord_rpc.h"
#include <windows.h>
#include <cstring>

DiscordRPC::~DiscordRPC() {
    Shutdown();
}

bool DiscordRPC::LoadLibrary() {
    if (handle_) return true;

    handle_ = LoadLibraryA("discord-rpc.dll");
    if (!handle_) return false;

    discordInitialize_ = (InitializeFunc)GetProcAddress((HMODULE)handle_, "Discord_Initialize");
    discordShutdown_ = (ShutdownFunc)GetProcAddress((HMODULE)handle_, "Discord_Shutdown");
    discordUpdatePresence_ = (UpdatePresenceFunc)GetProcAddress((HMODULE)handle_, "Discord_UpdatePresence");
    discordClearPresence_ = (ClearPresenceFunc)GetProcAddress((HMODULE)handle_, "Discord_ClearPresence");
    discordRunCallbacks_ = (RunCallbacksFunc)GetProcAddress((HMODULE)handle_, "Discord_RunCallbacks");

    if (!discordInitialize_ || !discordShutdown_ || !discordUpdatePresence_ || 
        !discordClearPresence_ || !discordRunCallbacks_) {
        FreeLibrary((HMODULE)handle_);
        handle_ = nullptr;
        return false;
    }
    return true;
}

bool DiscordRPC::Initialize(const char* applicationId) {
    if (initialized_) return true;
    if (!LoadLibrary()) return false;

    DiscordEventHandlers handlers = {};
    memset(&handlers, 0, sizeof(handlers));
    discordInitialize_(applicationId, &handlers, 1, nullptr);
    initialized_ = true;
    return true;
}

void DiscordRPC::Shutdown() {
    if (initialized_ && discordShutdown_) {
        discordShutdown_();
    }
    if (handle_) {
        FreeLibrary((HMODULE)handle_);
        handle_ = nullptr;
    }
    initialized_ = false;
    discordInitialize_ = nullptr;
    discordShutdown_ = nullptr;
    discordUpdatePresence_ = nullptr;
    discordClearPresence_ = nullptr;
    discordRunCallbacks_ = nullptr;
}

void DiscordRPC::UpdatePresence(const char* details, const char* state,
                                const char* largeImageKey, const char* largeImageText,
                                const char* smallImageKey, const char* smallImageText,
                                int64_t startTimestamp, int64_t endTimestamp) {
    if (!initialized_ || !discordUpdatePresence_) return;

    DiscordRichPresence presence = {};
    memset(&presence, 0, sizeof(presence));
    presence.details = details;
    presence.state = state;
    presence.largeImageKey = largeImageKey;
    presence.largeImageText = largeImageText;
    presence.smallImageKey = smallImageKey;
    presence.smallImageText = smallImageText;
    presence.startTimestamp = startTimestamp;
    presence.endTimestamp = endTimestamp;
    presence.instance = 0;

    discordUpdatePresence_(&presence);
}

void DiscordRPC::ClearPresence() {
    if (!initialized_ || !discordClearPresence_) return;
    discordClearPresence_();
}

void DiscordRPC::RunCallbacks() {
    if (!initialized_ || !discordRunCallbacks_) return;
    discordRunCallbacks_();
}