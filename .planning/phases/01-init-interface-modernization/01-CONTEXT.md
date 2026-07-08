# Phase 1: Init Interface Modernization - Context

**Gathered:** 2026-07-03
**Status:** Ready for planning

## Phase Boundary

Strip legacy params (`autodht`, `process`, `baseport`, `is_full_node`) from the 3 remaining C ABI init functions, repoint to `GeniusNode::New(config, AccountSource{variant})`, remove 2 redundant functions, and update all callers. The SDK stays a thin adapter — no config file generation, no validation beyond JSON parsing.

## Implementation Decisions

### Config & Engine Delegation
- **D-01**: Network config files (`network_config.json`, `sgns_config.json`) are user-provided at `base_path`. The SDK does NOT generate them and does NOT call SuperGenius `WriteNetworkConfig`/`WriteSgnsConfig` helpers. The engine (`GeniusNode`) reads them directly from `base_path`.
- **D-02**: The SDK only parses `dev_config` JSON to extract `DevConfig_st` (Address, Cut, TokenValueInGNUS, TokenID, BaseWritePath). Nothing else is synthesized or validated by the SDK layer.

### Implementation Structure
- **D-03**: No `SDKInitHelper` template. Each init function inlines its logic directly: validate params → parse dev_config JSON via `ParseDevConfig` → call `GeniusNode::New(config, AccountSource{variant})` → set `GeniusNodeInstance`. ~5 lines per function, zero indirection.
- **D-04**: `GeniusSDKInit()` → `AccountSource{NewAccount{}}`. `GeniusSDKInitWithKey()` → `AccountSource{FromPrivateKey{key}}`. `GeniusSDKInitWithMnemonic()` → `AccountSource{FromMnemonic{mnemonic}}`.

### Error Handling
- **D-05**: Null or empty `dev_config` JSON string → return `nullptr` immediately with `SPDLOG_ERROR`. No fallback to reading `dev_config.json` from disk.
- **D-06**: Key/mnemonic string params captured by value (`std::string`) in the lambda/inline logic to avoid use-after-free.

### Signature Changes
- **D-07**: Three init functions remain in `GeniusSDK.h`:
  - `GeniusSDKInit(const char *base_path, const char *dev_config)`
  - `GeniusSDKInitWithKey(const char *base_path, const char *dev_config, const char *eth_private_key)`
  - `GeniusSDKInitWithMnemonic(const char *base_path, const char *dev_config, const char *mnemonic)`
- **D-08**: `GeniusSDKInitWithKeyAndDevConfig` and `GeniusSDKInitMinimal` removed from both header and implementation.

### Return Contract
- **D-09**: Existing return contract preserved: `const char*` (init path) on success, `nullptr` on failure. The `static std::string ret_val` pattern kept for now (fix deferred to v2 — BUF-01).

### the agent's Discretion
- Internal `ParseDevConfig` helper refactoring — the agent decides whether to keep the existing `ReadDevConfigFromJSONStr` + `ParseDevConfig` split, or consolidate into one parse function.
- Exact SPDLOG message text for error cases.
- Whether to keep `GeniusSDKInitWithKeyAndDevConfig`'s inline implementation in a preserved comment for reference.

## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Project Artifacts
- `.planning/PROJECT.md` — Project context, constraints, out-of-scope items
- `.planning/REQUIREMENTS.md` — 17 v1 requirements with REQ-IDs
- `.planning/ROADMAP.md` — Phase 1 scope, success criteria, dependency order

### Codebase Maps
- `.planning/codebase/ARCHITECTURE.md` — Facade pattern, singleton, C↔C++ marshalling
- `.planning/codebase/STACK.md` — C++17, RapidJSON, Boost, C-compatible header
- `.planning/codebase/CONCERNS.md` — Thread-safety, null pointer risks, static buffer bugs

### Source Files (this phase touches)
- `src/GeniusSDK.h` — Public C ABI header — init function declarations to modify
- `src/GeniusSDK.cpp` — Implementation — init function bodies, ParseDevConfig, SDKInitHelper to refactor
- `example/SDKExample.cpp` — Caller to update (interactive menu demo)
- `example/SDKIdleExample.cpp` — Caller to update (idle node demo)
- `services/service.cpp` — Caller to update (headless daemon)

### External Dependency
- `SuperGenius/src/account/GeniusNode.hpp` (sibling project) — `GeniusNode::New(DevConfig_st, AccountSource)` factory, `AccountSource` variant, `DevConfig_st` struct. The new API is already present — no SuperGenius changes needed.

## Existing Code Insights

### Reusable Assets
- `ParseDevConfig()` / `ReadDevConfigFromJSONStr()` in `src/GeniusSDK.cpp` anonymous namespace — JSON-to-DevConfig_st parsing with RapidJSON. Already handles Address, Cut, TokenValueInGNUS, TokenID, BaseWritePath.
- `GeniusNode::New(DevConfig_st, AccountSource)` — external C++ factory. Already compiled and linked.

### Established Patterns
- `extern "C"` with `GNUS_VISIBILITY_DEFAULT` — all public functions follow this export convention.
- `do { check; delegate; break; } while(0)` — error handling pattern used in action functions. Init functions return null on error instead.
- `GeniusNodeInstance` global `shared_ptr` — singleton lifecycle pattern throughout the SDK.

### Integration Points
- Empty `GeniusNodeInstance` guard — most SDK functions check for null before delegating. Init functions set this pointer; Shutdown resets it.
- `static std::string ret_val` return buffer — used by Init functions to return the init path string.

## Specific Ideas

No specific UI or behavioral requests. Purely mechanical refactoring — strip params, repoint engine, remove dead code, update callers.

## Deferred Ideas

None — discussion stayed within phase scope.

---

*Phase: 1-Init Interface Modernization*
*Context gathered: 2026-07-03*
