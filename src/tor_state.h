#pragma once

enum class TorState { Disconnected, Starting, Connected, Blocked };

inline bool shouldBlockRequest(TorState s) {
    return s != TorState::Connected;
}

inline const char* torStateName(TorState s) {
    switch (s) {
        case TorState::Disconnected: return "Disconnected";
        case TorState::Starting:     return "Starting";
        case TorState::Connected:    return "Connected";
        case TorState::Blocked:      return "Blocked";
    }
    return "Unknown";
}