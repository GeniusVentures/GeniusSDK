# Feature Landscape

**Domain:** C SDK Init Interface Refactoring (parameter stripping + engine realignment)
**Researched:** 2026-07-03
**Confidence:** HIGH — source code surveyed (GeniusSDK.h, GeniusSDK.cpp, GeniusNode.hpp, all examples)

## Table Stakes

Features users expect. Missing = compile breaks or runtime crash.

| # | Feature | Why Expected | Complexity | Dependencies |
|---|---------|-------------|------------|-------------|
| T1 | **Strip `autodht`, `process`, `baseport`, `is_full_node` from all 5 C init signatures** | These are dead params on the new `GeniusNode::New()` unified API. Leaving them in the public header forces callers to pass meaningless values. | Low | None |
| T2 | **Repoint `GeniusSDKInit()` → `GeniusNode::New(config, NewAccount{})`** | No-key init generates a new identity. Old impl called `GeniusNode::New(config, autodht, baseport, is_full_node)` — the new API only takes `(config, AccountSource)`. | Low | T1 |
| T3 | **Repoint `GeniusSDKInitWithKey()` → `GeniusNode::New(config, FromPrivateKey{key})`** | Key-restore via unified factory. Only `base_path` + `eth_private_key` remain as params. | Low | T1 |
| T4 | **Repoint `GeniusSDKInitWithMnemonic()` → `GeniusNode::New(config, FromMnemonic{mnemonic})`** | Mnemonic-restore via unified factory. Only `base_path` + `mnemonic` remain as params. | Low | T1 |
| T5 | **Repoint `GeniusSDKInitWithKeyAndDevConfig()` → `GeniusNode::New(config, FromPrivateKey{key})`** | Inline dev_config JSON string overrides file-based config. Only `base_path` + `dev_config` + `eth_private_key` remain. Already special-cases its own init flow (no `SDKInitHelper`), keeps that. | Medium | T1 |
| T6 | **Repoint `GeniusSDKInitMinimal()` → updated `GeniusSDKInitWithKey()`** | Convenience wrapper. Currently delegates with hardcoded `true, true, baseport, false`. After stripping, simply calls `GeniusSDKInitWithKey(base_path, eth_private_key)` — no more bool params to pass. | Low | T3 |
| T7 | **Update `SDKInitHelper` template to call `GeniusNode::New(config, AccountSource)`** | The internal helper templates the node-creation lambda. Must accept the new single-parameter `New()` call. | Medium | T1-T4 |
| T8 | **Update all callers (examples + services) for new signatures** | Every `GeniusSDKInitWithKey(base_path, key, true, true, 40001, false)` must become `GeniusSDKInitWithKey(base_path, key)`. Affects: `SDKExample.cpp`, `SDKIdleExample.cpp`, `SDKExampleCredentials.cpp`, `service.cpp`. | Medium | T3 |
| T9 | **Preserve return contract: `const char*` path on success, `null` on failure** | Backward compatibility for the single return type. No new struct required. | Low | T1-T6 |
| T10 | **Header-only C ABI change — no C++ types leak into `GeniusSDK.h`** | C consumers (Unity, Unreal) must not see `std::variant`, `AccountSource`, or `GeniusNode` types. Parameter stripping alone achieves this. | Low | T1-T6 |

## Differentiators

Features that improve quality beyond "it compiles."

| # | Feature | Value Proposition | Complexity | Dependencies |
|---|---------|-------------------|------------|-------------|
| D1 | **Auto-write default `network_config.json` on first init** | New callers don't need to manually create JSON files. `GeniusNode::WriteNetworkConfig(base_path, 40001, true)` writes defaults if file missing. Eliminates "config file not found" as a setup step. | Low | T2-T5 |
| D2 | **Auto-write default `sgns_config.json` on first init** | Same as D1 for node role config. `GeniusNode::WriteSgnsConfig(base_path, "Light", true)` writes defaults (`node_type: Light`, `is_processor: true`). | Low | T2-T5 |
| D3 | **Clear error messages when config files missing and can't be auto-created** | Instead of a generic "Parse error" or "File not found", produce: `"dev_config.json not found at <path>. Create it with keys: Address, Cut, TokenValue, TokenID."`  SPGLOG_ERROR already wired. | Low | D1, D2 |
| D4 | **Remove `baseport` from `GeniusSDKInitMinimal` signature** | PROJ-08 says `GeniusSDKInitMinimal` delegates with updated signature (no `process`/`autodht`/`is_full_node`). But `baseport` is ALSO a stripped param. After stripping, `InitMinimal` becomes simply `GeniusSDKInitMinimal(base_path, eth_private_key)` — removing the last config param. Users who need custom ports use `network_config.json`. | Low | T6 |
| D5 | **Keep `SDKInitHelper` template generic** | The template accepts a lambda `Creator(DevConfig_st) → shared_ptr<GeniusNode>`. This stays clean: each init function provides a lambda that builds the right `AccountSource` and calls `GeniusNode::New(config, source)`. No branching on init type inside the helper. | Low | T7 |
| D6 | **`InitWithKeyAndDevConfig` preserves inline JSON override** | This function is unique: it takes a raw JSON string instead of reading `dev_config.json`. The new `GeniusNode::New()` still accepts a `DevConfig_st` parsed from that string — the override mechanism is unaffected by param stripping. | Low | T5 |

## Anti-Features

Features to explicitly NOT build.

| # | Anti-Feature | Why Avoid | What to Do Instead |
|---|-------------|-----------|-------------------|
| A1 | **Unified C init with source-type enum** | User explicitly prefers separate per-source C functions. Adding an enum (e.g., `GENIUS_INIT_NEW_ACCOUNT`, `GENIUS_INIT_WITH_KEY`) adds complexity callers don't need. Each function name is self-documenting. | Keep 5 separate init functions. |
| A2 | **Add `GeniusSDKInitWithPublicKey()` for `FromPublicKey` variant** | Out of scope per PROJ-08. The `FromPublicKey` variant requires the account to already exist in local storage — a different workflow (read-only node). Premature addition without use cases. | Defer; add when a C consumer requests read-only node init. |
| A3 | **Silently use hardcoded defaults for `autodht`/`process`/`is_full_node`** | Hiding node config behind C defaults makes behavior invisible and unconfigurable. Contradicts the whole point of the migration (JSON-driven config). | Write defaults to JSON files (D1, D2) so behavior is explicit and overridable. |
| A4 | **Change the singleton lifecycle pattern** | `GeniusNodeInstance` as a module-level `shared_ptr` + `GeniusSDKShutdown()` to reset it. Works. Separate concern (thread safety, multiple instances). | Don't touch. Leave singleton as-is. |
| A5 | **Add C++ types to public C header** | `GeniusSDK.h` is consumed by C compilers. `std::variant`, `AccountSource`, `sgns::GeniusNode` are C++14+ types that would break the ABI. | Keep header C89-compatible. Internal implementation uses C++ types freely. |
| A6 | **`process` param repurposed as a config-file write flag** | Ambiguous — does `process=true` mean "write `is_processor: true` to sgns_config.json" or "ignore config, force enable"? The single source of truth should be the JSON file. | Don't repurpose. All node config comes from JSON files at `base_path`. |
| A7 | **Add `GeniusSDKInitWithCredentials()` to public header** | Not in public header currently — only used in one example (`SDKExampleCredentials.cpp`) via implicit declaration. Not part of the 5-function API surface described in the requirements. | Don't promote. The example should migrate to `GeniusSDKInitWithKey()`. |
| A8 | **Keep old params as deprecated (do-nothing) placeholders** | Dead params confuse consumers. "Why does the API take `autodht` if it ignores it?" Better to break compilation and force callers to drop them — a one-line fix per callsite. | Strip completely. Callers get a compile error telling them exactly which params to remove. |

## Feature Dependencies

```
T1 (strip params from signatures)
 ├── T2 (Init → NewAccount)
 ├── T3 (InitWithKey → FromPrivateKey)
 ├── T4 (InitWithMnemonic → FromMnemonic)
 └── T5 (InitWithKeyAndDevConfig → FromPrivateKey)
      ├── T7 (SDKInitHelper update)
      │    └── T2, T3, T4 all flow through this
      ├── T6 (InitMinimal → updated InitWithKey)
      │    └── D4 (remove baseport from Minimal too)
      └── T8 (update callers)
           └── Must happen AFTER T3-T6 (compilation gate)

D1 ─── T2-T5 (auto-write network_config.json on first init)
D2 ─── T2-T5 (auto-write sgns_config.json on first init)
D3 ─── D1, D2 (error messages when auto-write fails)
D5 ─── T7 (keep SDKInitHelper generic — structural choice)
D6 ─── T5 (no-op: InitWithKeyAndDevConfig already uses its own init path)
```

## MVP Recommendation

**Phase 1: Signature stripping + callers (T1-T10)**
The core refactoring. Every init function loses 4 params, implementations point to `GeniusNode::New(config, AccountSource)`, callers update one line each.

- T1-T7: Implementation in `GeniusSDK.cpp` and `GeniusSDK.h`
- T8: Update `SDKExample.cpp`, `SDKIdleExample.cpp`, `SDKExampleCredentials.cpp`, `service.cpp`
- T9-T10: Verification that nothing broke

**Phase 2: Config bootstrapping (D1-D3)**
Once signatures are clean, add the quality-of-life features: auto-write default JSON configs so new callers don't hit "file not found" errors.

- D1: Write `network_config.json` with defaults (port_seed=40001, auto_dht=true)
- D2: Write `sgns_config.json` with defaults (node_type=Light, is_processor=true)
- D3: Improved error messages for missing `dev_config.json`

**Defer:**
- A2 (PublicKey init) — out of scope, add when requested
- D4 (remove baseport from InitMinimal) — part of T6 naturally, not separate work
- A7 (Credentials init cleanup) — do in T8 caller update, not as its own feature

## Sources

- **GeniusSDK.h (203-261)**: Current 5 init function signatures — all carry `autodht`, `process`, `baseport`, `is_full_node`. [HIGH — source code]
- **GeniusSDK.cpp (216-297)**: Current implementations using `SDKInitHelper` template and old-style `GeniusNode::New()`/`NewFromPrivateKey()`/`NewFromMnemonic()`. [HIGH — source code]
- **SuperGenius GeniusNode.hpp (75-130)**: New unified API — `AccountSource` variant, `New(DevConfig_st, AccountSource)` factory, `WriteNetworkConfig()`, `WriteSgnsConfig()` helpers. [HIGH — source code]
- **SuperGenius GeniusNode.cpp (254-280)**: `WriteNetworkConfig` writes `{"port_seed": ... , "auto_dht": ...}`. `WriteSgnsConfig` writes `{"node_type": "...", "is_processor": ...}`. [HIGH — source code]
- **SuperGenius GeniusNode.cpp (374-455)**: `LoadSgnsConfig()` reads `node_type` (Full/Light/Archive → derives `is_full_node_`), `is_processor`, `net_id`, `subnet_id` from JSON. `InitNetwork()` reads `port_seed`, `auto_dht`, `pubsub_port` from `network_config.json`. [HIGH — source code]
- **SDKExample.cpp (199-218)**: Calls `GeniusSDKInitWithKey(base_path, key, autodht, process, baseport, false)`. `getSDKConfig()` (467-483) prompts for old params. [HIGH — source code]
- **SDKIdleExample.cpp (14-18)**: Calls `GeniusSDKInitWithKey(path, key, true, true, 40001, false)`. [HIGH — source code]
- **service.cpp (28-34)**: CLI parses old params and calls `GeniusSDKInit(base_path, autodht, process, baseport, is_full_node)`. [HIGH — source code]
- **PROJECT.md (26-37)**: INIT-01 through INIT-09 requirements with explicit out-of-scope items. [HIGH — project spec]
