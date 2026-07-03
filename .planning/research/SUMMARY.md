# Project Research Summary

**Project:** GeniusSDK Init Interface Modernization
**Domain:** C SDK refactoring — parameter stripping + engine realignment
**Researched:** 2026-07-03
**Confidence:** HIGH — all findings verified against actual source code

## Executive Summary

This is a surgical refactoring of the GeniusSDK C API initialization interface. The SuperGenius engine has already migrated to a unified `GeniusNode::New(config, AccountSource)` factory — but the public C header still exposes 5 init functions carrying legacy params (`autodht`, `process`, `baseport`, `is_full_node`) that the new engine reads from JSON config files instead. The refactoring strips these dead params from every init signature, repoints the internal factory calls to the unified API using `AccountSource` variants, and adds config bootstrapping so new callers don't need to pre-create JSON files.

The recommended approach is a two-phase execution: **Phase 1 strips signatures across all 5 init functions and updates every caller** (examples, service) so the C API aligns with the engine. This is a tightly-coupled change — callers can't compile with the old signatures once the header changes. **Phase 2 adds config bootstrapping** (auto-write default `network_config.json` and `sgns_config.json` on first init) so the DX remains clean after the params are gone.

The primary risk is **silent behavior change from missing config files**: callers who were passing `is_full_node=true` or `autodht=false` as params will get the engine defaults (Light node, DHT enabled) unless config files exist. The mitigation is config bootstrapping in Phase 2, writing explicit defaults so behavior is visible and overridable.

## Key Findings

### Recommended Stack

This is a refactoring — no new dependencies. The stack is already in place:

**Core technologies:**
- **C89 public header (`GeniusSDK.h`):** C ABI consumed by Unity, Unreal, and C apps — must remain free of C++ types
- **C++17 implementation (`GeniusSDK.cpp`):** Wraps `sgns::GeniusNode` using `std::variant`/`std::shared_ptr`
- **`sgns::GeniusNode::New(DevConfig_st, AccountSource)`:** Unified engine factory — already present, no SuperGenius changes needed
- **RapidJSON:** Parses `dev_config.json` and config files — already bundled via SuperGenius
- **SPDLOG:** Structured logging for init errors — already used in `SDKInitHelper`

**Key change:** Every `GeniusNode::NewFromPrivateKey(key, autodht, baseport, is_full_node)` call becomes `GeniusNode::New(config, AccountSource{FromPrivateKey{key}})`. The `AccountSource` variant dispatches account creation internally. The facade only needs to build the right variant type, not the full dispatch logic.

### Expected Features

**Must have (table stakes — Phase 1):**
- **T1:** Strip `autodht`, `process`, `baseport`, `is_full_node` from all 5 C init signatures
- **T2-T5:** Repoint each init function to `GeniusNode::New(config, AccountSource{...})` with the appropriate variant
- **T6:** Update `GeniusSDKInitMinimal` to delegate to the stripped `InitWithKey`
- **T7:** Update `SDKInitHelper` template for single-param `New()` call
- **T8:** Update all callers — `SDKExample.cpp`, `SDKIdleExample.cpp`, `SDKExampleCredentials.cpp`, `service.cpp`
- **T9-T10:** Preserve `const char*` return contract; keep header C89-compatible

**Should have (differentiators — Phase 2):**
- **D1-D2:** Auto-write default `network_config.json` and `sgns_config.json` on first init — eliminates "config file not found" setup step
- **D3:** Clear error messages when config files are missing and can't be auto-created
- **D4:** Strip `baseport` from `GeniusSDKInitMinimal` too — if it becomes identical to `InitWithKey`, document as alias
- **D5:** Keep `SDKInitHelper` template generic (lambda-based, variant-agnostic)

**Defer:**
- `GeniusSDKInitWithPublicKey()` — no C consumer has requested read-only node init yet
- `GeniusSDKInitWithCredentials()` promotion to public header — migrate example to `InitWithKey()` instead

### Architecture Approach

The architecture follows a **Facade pattern**: `GeniusSDK.h/.cpp` is a thin C ABI wrapper over the C++ engine. The `SDKInitHelper<Creator>` template handles config loading, optional bootstrapping, and node creation — each init function provides a lambda that builds the right `AccountSource` variant and calls `GeniusNode::New(config, source)`.

**Data flow:** C call → `SDKInitHelper` → `ReadDevConfigFromJSON()` → [optional: `WriteNetworkConfig/WriteSgnsConfig` if files missing] → lambda → `GeniusNode::New(config, AccountSource)` → engine reads JSON configs internally → async subsystem init → return node instance.

**Major components:**
1. **`GeniusSDK.h`** — Public C ABI declarations (5 init + 50+ runtime functions)
2. **`SDKInitHelper` template** — Config loading, node creation, error handling, config bootstrapping
3. **Init function lambdas** — Build one `AccountSource` variant from C params, pass to `New()`
4. **`sgns::GeniusNode::New()`** — Unified factory: config validation, account creation, async subsystem init

**Key pattern:** Each init function is a one-liner lambda. The dispatch logic lives in the engine's `New()`, not duplicated in the facade.

### Critical Pitfalls

1. **Missing config files → wrong engine defaults.** `GeniusNode::New()` reads `sgns_config.json` and `network_config.json`. If absent, the engine uses hardcoded defaults (Light node, DHT enabled). Callers who were passing `is_full_node=true` or `autodht=false` as params will silently get different behavior. **Prevent:** Bootstrap default config files before calling `New()` (Phase 2, D1-D2).

2. **`GeniusSDKInitMinimal` signature collision.** After stripping 4 params from all functions, `InitMinimal(base_path, key, baseport)` and `InitWithKey(base_path, key)` have nearly identical signatures. **Prevent:** Strip `baseport` from `InitMinimal` too. If identical, document it as an alias of `InitWithKey`.

3. **`static std::string` accumulation across re-init.** `SDKInitHelper` uses `static std::string ret_val` that appends without reset. After `Shutdown()` + re-`Init()` with a new path, the returned string becomes `"Initialized on /old/path/new/path"`. **Prevent:** Reset `ret_val` at the start of `SDKInitHelper` (one-liner fix).

4. **ABI break from header-only change.** Removing 4 params changes the C stack frame. Binary-only consumers (dynamic linking) can crash if linked against old binaries. **Prevent:** Full caller audit before changing signatures; bump SONAME if shared library is distributed.

5. **Examples and service carry dead param logic.** `SDKExample.cpp`'s `getSDKConfig()` still prompts for `autodht`/`process`/`baseport`. `service.cpp` parses these from `argv`. **Prevent:** Remove dead prompts and CLI args. Config comes from JSON files.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Signature Stripping & Caller Migration (T1-T10)

**Rationale:** This is the core refactoring and must come first. Every init function loses 4 params and repoints to `GeniusNode::New(config, AccountSource)`. Callers must be updated simultaneously because the header changes break compilation otherwise — these are not separable.

**Delivers:**
- 5 C init functions with stripped signatures (only `base_path` + optional key/mnemonic/dev_config remain)
- All functions repointed to `GeniusNode::New(config, AccountSource{...})`
- Updated `SDKInitHelper` template with single-param `New()` call
- `GeniusSDKInitMinimal` stripped and documented as `InitWithKey` alias
- All 4 caller files updated: `SDKExample.cpp`, `SDKIdleExample.cpp`, `SDKExampleCredentials.cpp`, `service.cpp`
- `static std::string ret_val` reset fix (Pitfall 3)
- Compatibility verification: return contract preserved, header remains C89

**Avoids:** Pitfall 2 (InitMinimal collision), Pitfall 3 (static string accumulation), Pitfall 4 (ABI break via full caller audit), Pitfall 5 (credentials implicit declaration), Pitfall 6 (dead example prompts), Pitfall 7 (service CLI arg shift)

### Phase 2: Config Bootstrapping & Developer Experience (D1-D6)

**Rationale:** Once signatures are clean and the engine reads from JSON files, add auto-bootstrapping so new callers don't hit "file not found" on first init. This phase depends on Phase 1's updated `SDKInitHelper` — the bootstrapping logic is inserted there.

**Delivers:**
- Auto-write `network_config.json` with defaults (port_seed=40001, auto_dht=true) if missing
- Auto-write `sgns_config.json` with defaults (node_type=Light, is_processor=true) if missing
- Clear error messages for missing `dev_config.json` (can't be auto-created — requires user-specific values)
- Config files become the single source of truth; no hardcoded values pass through the facade

**Avoids:** Pitfall 1 (missing config files → wrong defaults) — this is the primary mitigation. Anti-pattern A3 (hardcoded defaults hidden from callers).

### Phase Ordering Rationale

- **Phase 1 first** because signature changes and caller updates are tightly coupled — the header changes break all callers simultaneously. This is the compilation gate.
- **Phase 2 second** because config bootstrapping is quality-of-life, not a compilation requirement. The engine runs without it (using hardcoded defaults), but Phase 2 makes behavior explicit.
- Grouping all 5 init functions + all 4 callers into Phase 1 avoids partial states where some functions are updated but callers reference old signatures.
- The `InitWithCredentials` cleanup (Pitfall 5) is handled naturally in Phase 1's caller update — the credentials example migrates to `InitWithKey`.

### Research Flags

Phases likely needing deeper research during planning:
- **None.** Both phases modify existing code with well-understood patterns. The SuperGenius `GeniusNode::New()` API is already stable and tested. The refactoring is mechanical: replace old factory calls with new variant-based ones.

Phases with standard patterns (skip `/gsd-plan-phase --research-phase`):
- **Phase 1:** Template-based creator injection is an existing pattern. All 5 init functions follow the same mechanical transformation.
- **Phase 2:** Config bootstrapping uses existing `WriteNetworkConfig()`/`WriteSgnsConfig()` helpers already in the engine. Simple file-exists check before calling them.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Existing codebase — no new dependencies. All technologies verified in `GeniusSDK.cpp` includes. |
| Features | HIGH | All 10 table-stake items verified against current signatures in `GeniusSDK.h` (lines 203-261) and new API in `GeniusNode.hpp` (lines 75-130). |
| Architecture | HIGH | `SDKInitHelper` template pattern verified in `GeniusSDK.cpp` (lines 184-213). Engine factory verified in `GeniusNode.hpp` (lines 106-107). |
| Pitfalls | HIGH | Pitfall 3 (static string) confirmed in source. Pitfall 1 (engine defaults) confirmed in `LoadSgnsConfig()`. Pitfalls 5/6/7 confirmed in caller files. |

**Overall confidence:** HIGH — this is a refactoring of existing code, not a greenfield build. Every finding is traceable to a specific line in the source code.

### Gaps to Address

- **Exact behavior of `GeniusNode::New()` when config files are missing:** Does the engine silently use defaults, emit a warning, or error out? The research says it reads defaults from `LoadSgnsConfig()` (GeniusNode.cpp 374-455) but the exact error/warning path should be verified during Phase 2 implementation.
- **Whether `WriteNetworkConfig`/`WriteSgnsConfig` are exported for SDK linkage:** These are static methods on `sgns::GeniusNode`. Verify they are accessible from `GeniusSDK.cpp` during Phase 2 planning — if not, the bootstrapping logic may need to be inlined.
- **`GeniusSDKInitWithCredentials` fate:** Currently called via C89 implicit declaration in one example. During Phase 1, decide whether to add it to the public header (unlikely) or migrate the example to `GeniusSDKInitWithKey()` (recommended).
- **SONAME bump:** If the shared library is distributed to binary consumers, a SONAME bump prevents linking against old binaries post-refactoring. Confirm distribution model during Phase 1 planning.

## Sources

### Primary (HIGH confidence — source code)
- **GeniusSDK.h (16-43, 203-261):** C ABI export macros, current 5 init function signatures with 4 legacy params each
- **GeniusSDK.cpp (1-50, 184-297):** Includes (RapidJSON, SPDLOG, Boost), `SDKInitHelper` template, current implementations using old `NewFromPrivateKey()`/`NewFromMnemonic()` calls
- **SuperGenius GeniusNode.hpp (75-130):** `AccountSource` variant definition, unified `New(DevConfig_st, AccountSource)` factory, `WriteNetworkConfig()`/`WriteSgnsConfig()` helpers
- **SuperGenius GeniusNode.cpp (254-355, 374-455):** `WriteNetworkConfig`/`WriteSgnsConfig` implementations, `LoadSgnsConfig()` JSON parsing with defaults, `InitNetwork()` config reading
- **SDKExample.cpp (199-218, 467-483):** Calls `InitWithKey` with old params, `getSDKConfig()` prompts for dead params
- **SDKIdleExample.cpp (14-18):** Hardcoded old param values
- **SDKExampleCredentials.cpp (9):** Implicit declaration of `GeniusSDKInitWithCredentials` (not in public header)
- **service.cpp (8-34):** CLI argument parsing for old params

### Secondary (MEDIUM confidence — project spec)
- **PROJECT.md (26-37):** INIT-01 through INIT-09 requirements with explicit out-of-scope items

---
*Research completed: 2026-07-03*
*Ready for roadmap: yes*
