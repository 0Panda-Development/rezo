# Rezo — Privacy-Focused Gaming Browser (v1 Design)

Date: 2026-08-18
Status: Approved by user

## Purpose

Rezo is a desktop browser for Windows that routes all traffic through the Tor
network so the user's real IP is never exposed. It doubles as a gaming-themed
browser with a dark UI and quick access to game sites. Delivered as a single
.exe the user builds locally from source.

## Decisions (user-approved)

| Question | Decision |
| --- | --- |
| IP hiding method | Real Tor network (bundled daemon, auto-routed) |
| Browser engine | CEF (Chromium Embedded Framework), C++ |
| Gaming features | Dark theme + game quick-access grid (no resource gadgets, no game launching) |
| Build setup | Agent writes C++ code; user compiles with Visual Studio + CMake |
| Tor integration depth | Bundle + auto-route + kill switch (no New Identity, no bridges in v1) |

## Architecture

One executable plus one child process:

- **Rezo.exe** — the CEF browser application (C++). Contains all UI and the Tor manager.
- **tor.exe** — bundled (~25 MB), spawned by Rezo at launch with a minimal
  config exposing SOCKS5 on `127.0.0.1:9050`.

Internal components:

- `TorManager` — spawns tor.exe, waits for port 9050 to accept, runs a watchdog
  thread that detects Tor death, exposes connection state to the UI.
- `App` — CEF glue: settings, command-line flags, lifecycle.
- `UI` — CEF Views-based window: top bar (back/forward/reload, address bar),
  minimal tab strip, Tor status indicator.
- `NewTabPage` — local HTML page with the game quick-access grid; tile links
  read from a plain config file the user can edit.

## Privacy core

- **Routing**: all CEF traffic goes through the SOCKS5 proxy at
  `127.0.0.1:9050`. Everything exits via Tor.
- **Kill switch**: if the watchdog detects Tor is down, navigation is blocked
  and an error page is shown. There is never a fallback to direct internet.
- **Chromium hardening flags**:
  - WebRTC forced to proxy-only (`--force-webrtc-ip-handling-policy=disable_non_proxied_udp`),
    which combined with a SOCKS5 proxy (no UDP) effectively disables WebRTC.
  - Telemetry/reporting disabled.
  - Third-party cookies blocked.
  - Generic Windows user agent.
  - No disk cache or history — session-only memory, Tor Browser policy.
- **DNS**: resolved inside the SOCKS5 tunnel (remote DNS through proxy), so no
  local DNS leaks.

## UI (gaming angle)

- Dark theme, gaming-styled.
- Top bar: back / forward / reload / address bar.
- Minimal tab strip (multiple browser views, switchable).
- Tor status indicator (connected / starting / failed).
- New-tab page: grid of game quick-access tiles (Steam, Roblox, Discord, game
  wikis). Tile links in a user-editable config file.

## Data flow and error handling

1. Launch → spawn tor.exe → wait for port 9050 → open window → all requests via Tor.
2. Tor fails to start → error page with retry button.
3. Tor dies mid-session → kill switch blocks navigation, warning shown, auto-retry.

## Testing

- One small C++ test for the kill-switch logic (tor dies → traffic blocked).
- Manual acceptance checklist:
  - IP check site (e.g. whatismyip) shows a Tor exit node.
  - WebRTC leak test clean.
  - DNS leak test clean.
  - Killing tor.exe mid-session → browser refuses to load pages.

## Explicitly out of scope for v1

- New Identity button, circuit display, bridges (censorship bypass)
- Canvas fingerprint randomization
- Extensions / download manager / profiles
- VPN layer for direct in-game play (Tor latency makes most games unplayable)

Each is a clean v2 add-on.