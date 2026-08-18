# Rezo

Privacy-focused gaming browser for Windows. All traffic routed through the Tor
network. Built with CEF (Chromium) in C++.

## Build

1. Install Visual Studio 2022 with the "Desktop development with C++" workload,
   and CMake >= 3.21.
2. Open a VS 2022 x64 developer terminal in this folder.
3. `cmake -S . -B build -G "Visual Studio 17 2022" -A x64`
4. `cmake --build build --config Release`

The exe lands in `build\bin\Release\` (CEF may use `build\Release\` — check
both). See the spec in `docs\superpowers\specs\` for the design.

## Run

Build, then copy the `tor\` folder and `src\resources\*` next to the exe
(first run also works from `dist\Rezo\` after packaging):

```powershell
.\scripts\package.ps1
dist\Rezo\Rezo.exe
```

First launch takes 30-90 s to connect to Tor. If Tor fails, every page shows
the "TOR OFFLINE" screen; click Retry. No IP ever leaves your machine
directly.
