# Technology Stack

**Project:** GeniusSDK Init Interface Modernization
**Researched:** 2026-07-03
**Confidence:** HIGH — source code verified (GeniusSDK.cpp, GeniusNode.hpp)

## Recommended Stack

This is a refactoring project — the stack is already chosen. The research confirms what exists and what the new engine requires.

### Core Framework

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| C ABI layer | C89 | Public header (`GeniusSDK.h`) consumed by Unity, Unreal, C consumers | Must remain C-compatible — no C++ types in the API |
| C++17 implementation | C++17 | `GeniusSDK.cpp` wraps `sgns::GeniusNode` | Required by SuperGenius engine (`std::variant`, `std::shared_ptr`) |
| `sgns::GeniusNode` | SuperGenius HEAD | Unified node factory — `New(DevConfig_st, AccountSource)` | Already present in sibling project; no SuperGenius changes needed |

### Database / Config

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| RapidJSON | (bundled via SuperGenius) | Parse `dev_config.json` and `sgns_config.json` | Already used for `ParseDevConfig()`, `ReadDevConfigFromJSON()` |
| JSON config files | N/A | Node runtime config at `base_path/` | New `GeniusNode::New()` reads `network_config.json` and `sgns_config.json` |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| SPDLOG | (bundled via SuperGenius) | Structured logging for init errors | Already used in `SDKInitHelper` and all error paths |
| Boost.Outcome | (bundled via SuperGenius) | Error handling for config parsing (`outcome::result`) | Existing pattern in `ReadDevConfigFromJSON()` |
| Boost.Algorithm | (bundled via SuperGenius) | Hex string conversion for token IDs | Already used for `boost::algorithm::unhex` |

### Build / Platform

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| CMake | (existing) | Cross-platform build | Already supports macOS, iOS, Android, Linux, Windows |
| GNUS_VISIBILITY_DEFAULT | (existing macro) | DLL export on Windows, visibility on Unix | Existing pattern in `GeniusSDK.h` |

## What Changes (and What Doesn't)

### Changes

| Component | Old | New |
|-----------|-----|-----|
| Init function signatures | `GeniusSDKInit(path, autodht, process, baseport, is_full_node)` | `GeniusSDKInit(path)` |
| Internal factory call | `GeniusNode::New(config, autodht, baseport, is_full_node)` | `GeniusNode::New(config, AccountSource{NewAccount{}})` |
| Internal factory call (key) | `GeniusNode::NewFromPrivateKey(config, key, autodht, baseport, is_full_node)` | `GeniusNode::New(config, AccountSource{FromPrivateKey{key}})` |
| Internal factory call (mnemonic) | `GeniusNode::NewFromMnemonic(config, mnemonic, autodht, baseport, is_full_node)` | `GeniusNode::New(config, AccountSource{FromMnemonic{mnemonic}})` |
| Node config source | Inline bool/uint params | JSON files at `base_path` |

### Unchanged

| Component | Why Unchanged |
|-----------|--------------|
| `GeniusSDKShutdown()` | Singleton lifecycle — separate concern |
| All 50+ non-init C functions (`GetBalance`, `Transfer`, `Mint`, etc.) | Only init functions are affected |
| `SDKInitHelper` template pattern | Still valid — just the lambda changes from multi-param to single-param `New()` |
| `ParseDevConfig()` / `ReadDevConfigFromJSON()` | `dev_config.json` parsing unchanged — `DevConfig_st` still needed |
| `GNUS_EXPORT_BEGIN` / `GNUS_EXPORT_END` macros | C ABI wrapping unchanged |
| `GeniusNodeInstance` singleton | Module-level `shared_ptr<sgns::GeniusNode>` unchanged |

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Init function design | Separate per-source C functions | Single `GeniusSDKInit()` with source-type enum | User explicitly prefers explicit function names |
| Config bootstrapping | Auto-write default JSON files if missing | Require callers to pre-create config files | Worse DX; all examples and the service would need setup scripts |
| Old param handling | Strip completely (compile error) | Keep as deprecated no-ops | Dead params confuse; compile error is a one-line fix |
| `FromPublicKey` exposure | Defer (no C function) | Add `GeniusSDKInitWithPublicKey()` now | Out of scope per PROJECT.md; no C consumer has asked for it |

## Installation

No new dependencies. Refactoring-only change.

```bash
# Build verification
mkdir build && cd build
cmake ..
make
# Verify examples compile with new signatures
./example/SDKExample
```

## Sources

- **SuperGenius GeniusNode.hpp (75-130)**: AccountSource variant and New() factory signature. [HIGH — source code]
- **GeniusSDK.cpp (1-50)**: Existing includes — RapidJSON, SPDLOG, Boost.Outcome, Boost.Algorithm. [HIGH — source code]
- **GeniusSDK.h (16-43)**: C ABI export macros — GNUS_EXPORT_BEGIN/END, GNUS_VISIBILITY_DEFAULT. [HIGH — source code]
