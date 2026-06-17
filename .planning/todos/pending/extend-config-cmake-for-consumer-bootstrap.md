---
title: Add consumer-side find_package bootstrap to GeniusSDKConfig.cmake
date: 2026-06-08
priority: high
---

## What

Extend `cmake/config.cmake.in` so that downstream consumers calling `find_package(GeniusSDK)` get:

1. **Static/shared target selection** — `find_package(GeniusSDK COMPONENTS static)` or `find_package(GeniusSDK COMPONENTS shared)`
2. **Dependency resolution cascade** — if SuperGenius/thirdparty/zkLLVM not found under `PROJECT_SUPER_ROOT`, download from GitHub releases using the same pattern as `build/CommonCompilerOptions.cmake`
3. **All transitive include paths and link libraries** — consumer's target links `sgns::GeniusSDK` and everything resolves

## Files to touch

- `cmake/config.cmake.in` — primary target, add dependency discovery and download logic
- `cmake/CommonBuildParameters.cmake` — may need refactoring to extract reusable download functions
- `build/CommonCompilerOptions.cmake` — reference for download patterns to replicate

## Key constraints

- Must preserve the `PROJECT_SUPER_ROOT` relative layout
- Download logic should be idempotent (skip if already present)
- Must work on all target platforms (macOS, Linux, Windows, iOS, Android)
