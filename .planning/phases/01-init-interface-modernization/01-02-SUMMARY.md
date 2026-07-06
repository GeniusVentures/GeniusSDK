---
phase: 01-init-interface-modernization
plan: 02
type: execute
subsystem: caller-updates
tags: [refactor, config-files, caller-migration]
requires: [01-01]
provides:
  - updated-initsdk-caller
  - updated-idle-caller
  - updated-service-caller
  - example-config-files
  - service-config-files
key-decisions:
  - "SDKExample initSDK loads dev_config.json from base_path before calling 3-arg GeniusSDKInitWithKey"
  - "SDKIdleExample reads dev_config.json from ./ before calling 3-arg GeniusSDKInitWithKey"
  - "service.cpp accepts only <base_path> CLI arg, loads dev_config.json, calls 2-arg GeniusSDKInit"
  - "network_config.json (port 40001, enable_dht: true) and sgns_config.json (enable_processing: true, is_full_node: false) shipped in both example/ and services/ directories"
tech-stack:
  added: []
  patterns:
    - "C++17 std::ifstream + std::stringstream for file reading (SDKIdleExample, service)"
    - "JsonData_t (char[2048]) buffer for dev_config JSON in SDKExample (reuses existing loadJsonFromFile helper)"
key-files:
  created:
    - example/network_config.json
    - example/sgns_config.json
    - services/network_config.json
    - services/sgns_config.json
  modified:
    - example/SDKExample.cpp
    - example/SDKIdleExample.cpp
    - services/service.cpp
    - example/CMakeLists.txt
metrics:
  duration: ~5m
  completed: "2026-07-06T16:23:33Z"
---

# Phase 01 Plan 02: Caller Migration & Config File Shipping Summary

**One-liner:** Updated all 3 SDK callers to use the new 2/3-parameter init signatures and shipped network/sgns config files with old hardcoded defaults.

## What Was Built

### Caller Updates

1. **`example/SDKExample.cpp`** — `initSDK()` now loads `dev_config.json` from `base_path` using the existing `loadJsonFromFile()` helper, then calls `GeniusSDKInitWithKey(base_path, dev_config, eth_private_key)` (3 args). `getSDKConfig()` stripped of `autodht`/`process`/`baseport` parameters — now only accepts `base_path` and `eth_private_key`.

2. **`example/SDKIdleExample.cpp`** — `main()` reads `dev_config.json` from `./` using `std::ifstream` + `std::stringstream`, then calls `GeniusSDKInitWithKey(no_path, dev_config.c_str(), "deadbeef...")` (3 args). All legacy `true, true, 40001, false` params removed.

3. **`services/service.cpp`** — Complete rewrite: accepts only `<base_path>` as CLI arg (no DHT/port/full-node flags). Reads `dev_config.json` from `base_path` using `std::ifstream` + `std::stringstream`, calls `GeniusSDKInit(base_path, dev_config.c_str())` (2 args). `parse_bool` lambda removed.

### Config Files

Four new JSON config files created, encoding the previous hardcoded defaults:

| File | Content |
|------|---------|
| `example/network_config.json` | `{"port": 40001, "enable_dht": true}` |
| `example/sgns_config.json` | `{"enable_processing": true, "is_full_node": false}` |
| `services/network_config.json` | `{"port": 40001, "enable_dht": true}` |
| `services/sgns_config.json` | `{"enable_processing": true, "is_full_node": false}` |

### Build Integration

- `example/CMakeLists.txt` updated to copy `network_config.json` and `sgns_config.json` to `SDKExample` build output using `copy_if_different` (matching the existing `dev_config.json` pattern).
- `services/CMakeLists.txt` not modified — no existing copy targets for config files (operator deploys alongside binary).

## Verification Summary

All plan-level verification gates pass:

- ✅ SDKExample: `GeniusSDKInitWithKey` called with 3 args (2 commas)
- ✅ SDKExample: zero `autodht`/`process`/`baseport` in `initSDK`/`getSDKConfig`
- ✅ SDKIdleExample: `GeniusSDKInitWithKey` called with 3 args (2 commas)
- ✅ service.cpp: `GeniusSDKInit` called with 2 args (1 comma)
- ✅ service.cpp: `parse_bool` removed, usage shows only `<base_path>`
- ✅ All 4 config files exist and parse as valid JSON with correct defaults
- ✅ No caller file references `is_full_node` in init context

## Deviations from Plan

None — plan executed exactly as written.

## Threat Flags

None — all threat surface matches the plan's threat model. The hardcoded private key in `SDKIdleExample.cpp` is documented as a demo key (T-01-08: accept). Error handling for missing/empty `dev_config.json` is present in both `SDKIdleExample.cpp` and `service.cpp` (T-01-07: mitigate).

## Commits

| Hash | Message |
|------|---------|
| `263cbd2` | feat(01-init-interface-modernization): update SDKExample.cpp with 3-arg init, ship config files |
| `4bc4a5f` | feat(01-init-interface-modernization): update SDKIdleExample and service callers, ship service configs |
