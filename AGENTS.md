# Agent notes

- C++20/SDL3 River Raid. Entry: `src/main.cpp`; rules in `src/game` stay SDL-independent.
- Compile shipped assets from `src/assets/`; no original game files or emulator dependencies.
- Preserve playable behavior, regression fixtures and complete sessions.
- Keep README limited to Play, Controls and Build from source. Comments and Markdown are English.
- Build in `.build/`. Check with `make doctor build test smoke check-standalone check-native-tools check-ubsan`.
- Automated SDL checks use dummy drivers or isolated Xvfb, never the real desktop.
