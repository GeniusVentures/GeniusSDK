---
title: GeniusSDK Installer Script
trigger_condition: When onboarding new downstream projects that need GeniusSDK
planted_date: 2026-06-08
---

## Idea

A standalone `install.sh` script (or cross-platform equivalent) that assembles a complete GeniusNetwork tree from GitHub releases.

## What it does

1. Asks user which platforms they need (OSX, Linux, Windows, iOS, Android — select multiple)
2. Asks which build types (Debug, Release, or both)
3. Creates the `GeniusNetwork/` directory structure
4. Downloads GeniusSDK, SuperGenius, thirdparty, and zkLLVM from their respective GitHub releases
5. Extracts them to preserve the relative `PROJECT_SUPER_ROOT` layout
6. Outputs a `GeniusSDKConfig.cmake` or environment setup so `find_package(GeniusSDK)` just works

## Design notes

- Pattern already exists in `build/CommonCompilerOptions.cmake` (curl download + tar extract)
- Could be a standalone shell script or a CMake script invoked via `cmake -P`
- Should cache downloads so re-running doesn't re-download
- Could be the README's "Quick Start" instructions for new projects
