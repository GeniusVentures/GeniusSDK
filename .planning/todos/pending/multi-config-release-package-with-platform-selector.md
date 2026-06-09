---
title: Combined Debug+Release multi-config release package with platform selector
date: 2026-06-08
priority: medium
---

## What

Update `.github/workflows/build-release-tag.yml` to produce a combined consumer-friendly release package that bundles:

1. Both Debug and Release builds in one archive per platform
2. Headers from a single build (they're the same for both configs)
3. An interactive install script (`install.sh`) that lets users select which platforms to download

## The install script

An interactive script that:
- Lists available platforms (OSX, Linux, Windows, iOS, Android)
- Lets user check/uncheck which they want
- Downloads only the selected platform archives from GitHub releases
- Extracts to maintain `GeniusNetwork/` relative layout
- Prints `find_package` usage instructions after install

## Release workflow changes

- Build Debug and Release for each platform (already done via matrix)
- Combine both into one archive per platform: `GeniusSDK-<Platform>-<Tag>.tar.gz` containing `build/<Platform>/Debug/` and `build/<Platform>/Release/` under the `GeniusSDK/` prefix
- Generate a manifest or index so the install script knows what's available
- Upload the combined archive + install script to the release

## Files to touch

- `.github/workflows/build-release-tag.yml` — add combine step, generate install script
- New file: `scripts/install.sh` (or `cmake/install-release.cmake`) — the interactive selector
