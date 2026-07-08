---
phase: 02-verification-documentation
reviewed: 2026-07-06T18:00:00Z
depth: standard
files_reviewed: 11
files_reviewed_list:
  - src/GeniusSDK.h
  - src/GeniusSDK.cpp
  - example/SDKExample.cpp
  - example/SDKIdleExample.cpp
  - services/service.cpp
  - example/network_config.json
  - example/sgns_config.json
  - services/network_config.json
  - services/sgns_config.json
  - example/dev_config.json
  - example/CMakeLists.txt
findings:
  critical: 2
  warning: 6
  info: 5
  total: 13
status: issues_found
---

# Phase 01-02: Code Review Report

**Reviewed:** 2026-07-06
**Depth:** standard
**Files Reviewed:** 11 (6 source files + 5 config/build files)
**Status:** issues_found — 2 blockers, 6 warnings, 5 info

## Summary

Review of the Init Interface Modernization (Phase 1) implementation. The refactoring successfully reduced 5 init functions to 3, stripped `autodht`/`process`/`baseport`/`is_full_node` params, inlined the `SDKInitHelper` template, and updated callers to pass `dev_config` as a JSON string parameter. The header and implementation signatures are consistent.

**Key concerns:** Two callers (`service.cpp` and `SDKIdleExample.cpp`) discard the init function's return value, silently proceeding into busy-wait loops on failure. The `service.cpp` path concatenation lacks a `/` separator which will cause config-load failure when `base_path` lacks a trailing slash. Pre-existing null-pointer dereference bugs in 8 non-init functions remain unfixed. The four shipped JSON config files (`network_config.json`, `sgns_config.json`) have no consumers — they represent the migrated-away parameters and are now dead configuration.

---

## Critical Issues

### CR-01: `service.cpp` — Init Return Value Discarded; Silent Failure Into Busy Loop

**File:** `services/service.cpp:35`
**Issue:** `GeniusSDKInit( base_path, dev_config.c_str() )` returns a `const char*` that is `nullptr` on failure, but the return value is discarded. The program then enters `while ( true ) { }` on line 37, burning 100% CPU indefinitely as if initialization succeeded. When `dev_config.json` is missing, malformed, or empty, the service provides no diagnostics and fails silently.

This is a **behavioral regression**: the old code had the same pattern, but the new code introduces more failure modes (dev_config.json must exist, must be non-empty, must parse). The service is less robust than before because it no longer has internal defaults.

**Fix:**
```cpp
const char *init_result = GeniusSDKInit( base_path, dev_config.c_str() );
if ( !init_result || strncmp( init_result, "Initialized", strlen( "Initialized" ) ) != 0 )
{
    std::cerr << "Error: GeniusSDK initialization failed: "
              << ( init_result ? init_result : "No response" ) << "\n";
    return 1;
}

while ( true )
{
}
```

### CR-02: `SDKIdleExample.cpp` — Init Return Value Discarded

**File:** `example/SDKIdleExample.cpp:25-26`
**Issue:** `GeniusSDKInitWithKey()` return value is discarded. If `dev_config.json` is missing, empty, or the private key is invalid, the example enters a `while(true)` loop at line 28 as if initialization succeeded. This is a **behavioral regression** from the old code which also discarded the return value, but the old code's `dev_config.json` was loaded internally by the SDK with implicit defaults; the new code adds an external file-load step with an explicit failure mode (empty config at line 23: `if ( dev_config.empty() ) return 1;`) while still discarding the init return for other failures.

**Fix:**
```cpp
const char *init_result = GeniusSDKInitWithKey( no_path, dev_config.c_str(),
    "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef" );
if ( !init_result || strncmp( init_result, "Initialized", strlen( "Initialized" ) ) != 0 )
{
    std::cerr << "Error: GeniusSDK init failed\n";
    return 1;
}

while ( true )
{
}
```

---

## Warnings

### WR-01: `service.cpp` — Missing Path Separator in Config File Load

**File:** `services/service.cpp:20`
**Issue:** `std::string( base_path ) + "dev_config.json"` concatenates without a `/` separator. When `base_path` does not end with `/` (e.g., `argv[1]` = `/home/user/genius`), the resulting path is `/home/user/geniusdev_config.json` instead of `/home/user/genius/dev_config.json`. The old SDK code had the same bug internally (`ReadDevConfigFromJSON` did `base_path + "dev_config.json"`), but it's been propagated into the caller where it's now a per-caller responsibility to get right.

**Fix:**
```cpp
std::string base_path = argv[1];
// Ensure trailing separator
if ( base_path.back() != '/' && base_path.back() != '\\' )
    base_path += '/';

std::ifstream cfg_file( base_path + "dev_config.json" );
```

### WR-02: `SDKIdleExample.cpp` — Fragile Hardcoded Path Concatenation

**File:** `example/SDKIdleExample.cpp:18`
**Issue:** `std::string( no_path ) + "dev_config.json"` — `no_path` is hardcoded to `"./"` so the concatenation happens to produce `"./dev_config.json"` correctly, but the pattern is fragile. If the path constant is ever changed (e.g., to `"."` or `"."`, which are semantically identical), the concatenation silently breaks. Same issue as WR-01 but masked by the trailing-slash default.

**Fix:** Normalize the path with a trailing separator check as in WR-01, or use a filesystem path join utility.

### WR-03: `GeniusSDKGetVersion` — Null Dereference When Not Initialized

**File:** `src/GeniusSDK.cpp:292`
**Issue:** `static std::string version = GeniusNodeInstance->GetVersion();` dereferences `GeniusNodeInstance` without a null check. If called before any init function, this crashes. Pre-existing bug (documented in `.planning/codebase/CONCERNS.md:33`) that was not addressed by the refactoring. The function is in the reviewed file and remains dangerous to external consumers.

### WR-04: Seven More Functions — Null Dereference on Uninitialized `GeniusNodeInstance`

**Files:** `src/GeniusSDK.cpp:280,298,303,306,321,326,345,350,544`
**Issue:** The following functions dereference `GeniusNodeInstance` without null guards:

| Function | Line(s) |
|----------|---------|
| `GeniusSDKGetGNUSPrice` | 280 |
| `GeniusSDKGetBalance` | 298 |
| `GeniusSDKGetBalanceGNUS` | 303, 306 |
| `GeniusSDKGetBalanceGNUSString` | 321, 326 |
| `GeniusSDKGetOutTransactions` | 345 |
| `GeniusSDKGetInTransactions` | 350 |
| `GeniusSDKGetCost` | 544 |

All are pre-existing bugs (documented in CONCERNS.md, lines 28-43). Calling any of these before initialization causes undefined behavior / segfault. The refactoring did not touch these functions, but they remain in the reviewed file.

**Fix for all:** Add a `!GeniusNodeInstance` guard returning a zero/empty sentinel. Example for `GeniusSDKGetBalance`:
```cpp
uint64_t GeniusSDKGetBalance( GeniusTokenID token_id )
{
    if ( !GeniusNodeInstance )
        return 0;
    return GeniusNodeInstance->GetBalance( sgns::TokenID::FromBytes( token_id.data, sizeof( token_id.data ) ) );
}
```

### WR-05: `GeniusSDKGetCost` — Null Dereference on `GeniusNodeInstance` AND `procmgr.value()` Unvalidated

**File:** `src/GeniusSDK.cpp:539-544`
**Issue:** Two problems in one function:
1. Line 544: `GeniusNodeInstance->GetProcessCost(procmgr.value())` — no null guard on `GeniusNodeInstance` (crashes before init).
2. If `ProcessingManager::Create()` succeeds but returns an empty/invalid state, `procmgr.value()` may produce garbage. The `.value()` call is made without additional validity checks.

(This is a pre-existing issue.)

### WR-06: All Three Init Functions — `static std::string` Return Buffer Not Thread-Safe

**Files:** `src/GeniusSDK.cpp:182-185,210-213,238-241`
**Issue:** Each init function uses its own `static std::string ret_val` as a return buffer. Concurrent calls to the same init function from different threads will race on the static buffer. The old code had one shared static buffer (in `SDKInitHelper`); the refactoring tripled the number of static buffers (one per init function). While this reduces cross-function contention, concurrent calls to the same function still race.

This is a pre-existing pattern (documented in CONCERNS.md, lines 16-24). The refactoring propagated it without improvement.

**Fix:** Return a heap-allocated string via `strdup()`, documented to be freed with `GeniusSDKFree()`. Or accept a caller-owned output buffer with a size parameter.

---

## Info

### IN-01: Init Functions — Significant Code Duplication

**Files:** `src/GeniusSDK.cpp:166-242`
**Issue:** The three init functions (`GeniusSDKInit`, `GeniusSDKInitWithKey`, `GeniusSDKInitWithMnemonic`) share ~90% of their implementation: null checks on `base_path`/`dev_config`, `ParseDevConfig` call, null check on `cfg`, null check on `GeniusNodeInstance` after `New()`, and identical `static std::string ret_val` pattern. Only the third-parameter validation and the `AccountSource` variant differ. ~25 lines × 3 of near-identical code.

**Suggestion:** Extract a shared helper:
```cpp
template <typename AccountSourceVariant>
static const char *doInit( const char *base_path, const char *dev_config, AccountSourceVariant &&src )
{
    if ( !base_path || !dev_config || dev_config[0] == '\0' )
    {
        SPDLOG_ERROR( "base_path and dev_config must not be empty!" );
        return nullptr;
    }
    auto cfg = ParseDevConfig( std::string( dev_config ), std::string( base_path ) );
    if ( !cfg ) { SPDLOG_ERROR( "{}", cfg.error().what() ); return nullptr; }
    GeniusNodeInstance = sgns::GeniusNode::New( cfg.value(), sgns::AccountSource{ std::forward<AccountSourceVariant>( src ) } );
    if ( !GeniusNodeInstance ) return nullptr;
    static std::string ret_val = "Initialized on ";
    ret_val.assign( "Initialized on " );
    ret_val.append( base_path );
    return ret_val.c_str();
}
```
(This is the old `SDKInitHelper` pattern adapted for the new `AccountSource` API.)

### IN-02: Redundant `static std::string` Initializer

**Files:** `src/GeniusSDK.cpp:182-183,210-211,238-239`
**Issue:** In all three init functions, the pattern is:
```cpp
static std::string ret_val = "Initialized on ";
ret_val.assign( "Initialized on " );
```
The `assign()` on the next line immediately overwrites the initializer with the same value. The initializer is dead code. Either remove the initializer or remove the `assign()`.

**Fix:**
```cpp
static std::string ret_val;
ret_val.assign( "Initialized on " );
ret_val.append( base_path );
return ret_val.c_str();
```

### IN-03: Orphaned `network_config.json` and `sgns_config.json` — Shipped But Not Consumed

**Files:** `example/network_config.json`, `example/sgns_config.json`, `services/network_config.json`, `services/sgns_config.json`
**Issue:** These JSON configuration files represent the migrated-away parameters (`autodht`, `process`/`enable_processing`, `baseport`/`port`, `is_full_node`). They are shipped in the repository and `example/CMakeLists.txt` copies them to the build directory, but **no code in this codebase reads them**. The SDK no longer accepts these parameters, and the examples/services do not reference these files.

If the `GeniusNode` internals (in a dependency) read these files from the filesystem relative to `BaseWritePath`, then their presence is justified. But from the shipped SDK bridge layer perspective, they appear to be dead configuration.

**Suggestion:** Verify whether `GeniusNode` internals consume these files. If yes, document in a README. If no, remove the files and the `CMakeLists.txt` copy commands.

### IN-04: `example/dev_config.json` — Hardcoded Address Committed to Repository

**File:** `example/dev_config.json:2`
**Issue:** Contains `"Address": "0xcafe"` — a test/demo address that is committed. While clearly a placeholder, security scanners will flag committed addresses. Already documented in CONCERNS.md (line 99). The refactoring has increased visibility of this file by making it required for all init paths.

### IN-05: `GeniusSDKGetVersion` — Static Variable Initialized with Dereference

**File:** `src/GeniusSDK.cpp:292`
**Issue:** `static std::string version = GeniusNodeInstance->GetVersion();` — the `static` variable is initialized on first call with the current version, then never updated. If the node is shut down and re-initialized with a different version, this function returns the stale cached version. Pre-existing issue.

---

_Reviewed: 2026-07-06T18:00:00Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
