---
title: GeniusSDK Release Packaging Strategy
date: 2026-06-08
context: /gsd:explore — how to package GeniusSDK for downstream consumers
---

## Problem

Downstream projects that want to use GeniusSDK currently need the full source tree with SuperGenius, thirdparty, and zkLLVM all built from source. There's no self-contained release package a consumer can download and `find_package(GeniusSDK)` against.

## Current State

- **Release pipeline exists** (`.github/workflows/build-release-tag.yml`): builds for OSX, Linux, Windows, iOS, Android × Debug/Release, downloads thirdparty/zkLLVM/SuperGenius from their respective GitHub releases, builds GeniusSDK, installs, and uploads a platform-specific tarball
- **Install tree generated** at `build/<Platform>/<BuildType>/GeniusSDK/` with `lib/libGeniusSDK.a`, `lib/libGeniusSDK_shared.dylib`, headers, and cmake config
- **config.cmake.in is minimal** — just `@PACKAGE_INIT@` + targets include. No dependency resolution, no SuperGenius/thirdparty path discovery
- **Download cascade already exists** in `build/CommonCompilerOptions.cmake`: if thirdparty/zkLLVM not found locally, downloads from GitHub releases and extracts under `PROJECT_SUPER_ROOT`
- **SuperGenius also has release pipeline** (`.github/workflows/build-release-tags.yml`)

## Strategy: Two-Part Bootstrap

### Part 1: Small bootstrap package
A minimal archive containing:
- The `build/` submodule (cmake template with toolchain files, download logic, helper functions)
- A `CMakeLists.txt` that knows how to bootstrap
- A `README.md` with platform selection instructions
- An interactive install script (`install.sh`) that lets the user pick platforms and build types

### Part 2: Per-platform release archives
Downloaded on-demand from `GeniusVentures/GeniusSDK/releases`:
- Pre-built static lib, shared lib, and headers
- The `GeniusSDKConfig.cmake` that wires up dependency resolution

## Key Design Decisions

1. **Replicate download cascade in consumer cmake**: `GeniusSDKConfig.cmake` should do what `CommonCompilerOptions.cmake` does — find thirdparty/SuperGenius/zkLLVM locally, or download from releases
2. **Preserve relative directory layout**: Consumer's machine gets a `GeniusNetwork/`-style tree so `PROJECT_SUPER_ROOT` relative paths work
3. **Static vs shared selection**: Consumer chooses via `find_package(GeniusSDK COMPONENTS static)` or similar, matching how CMake handles multi-config
4. **Multi-config packaging**: Bundle Debug + Release in one archive so consumer can choose build type without re-downloading
