# GeniusSDK Init Interface Modernization

## What This Is

Adapt the GeniusSDK C ABI init layer to match the new SuperGenius `GeniusNode` API. The
`GeniusNode` has been modernized to use an `AccountSource` variant pattern and JSON-based
node configuration, but GeniusSDK.cpp still calls the old multi-variant API with inline
`autodht`, `baseport`, `is_full_node`, and `process` parameters. This change strips those
parameters from the public C API and delegates node config to JSON files on `base_path`.

## Core Value

A simpler, fewer-parameter C init surface that stays in sync with the underlying C++ engine
so SDK consumers don't carry dead parameters through every init call.

## Requirements

### Validated

- ✓ C ABI facade wrapping `sgns::GeniusNode` singleton — existing
- ✓ `DevConfig_st` JSON parsing with RapidJSON — existing
- ✓ 50+ exported C functions for accounts, transfers, mint, processing — existing
- ✓ Singleton lifecycle: init → use → shutdown — existing
- ✓ Cross-platform build: macOS, iOS, Android, Linux, Windows — existing

### Active

- [ ] **INIT-01**: Strip `autodht`, `baseport`, `is_full_node`, `process` parameters from all C init functions
- [ ] **INIT-02**: `GeniusSDKInit()` calls `GeniusNode::New(config, AccountSource{NewAccount{}})` — no key/restore params
- [ ] **INIT-03**: `GeniusSDKInitWithKey()` calls `GeniusNode::New(config, AccountSource{FromPrivateKey{key}})` — only `base_path` + key
- [ ] **INIT-04**: `GeniusSDKInitWithMnemonic()` calls `GeniusNode::New(config, AccountSource{FromMnemonic{mnemonic}})` — only `base_path` + mnemonic
- [ ] **INIT-05**: `GeniusSDKInitWithKeyAndDevConfig()` uses `GeniusNode::New(config, AccountSource{FromPrivateKey{key}})` — only `base_path` + dev_config + key
- [ ] **INIT-06**: `GeniusSDKInitMinimal()` delegates to `GeniusSDKInitWithKey()` with updated signature (no `process`/`autodht`/`is_full_node` params)
- [ ] **INIT-07**: Node runtime config (DHT, ports, full-node mode, processing) written to JSON files via `WriteNetworkConfig`/`WriteSgnsConfig` or read by `GeniusNode` directly from `base_path`
- [ ] **INIT-08**: Callers (examples, services) updated to match new init signatures
- [ ] **INIT-09**: All existing init functions preserve their return contract (init path string on success, null on failure)

### Out of Scope

- Public key / read-only init — no C API function for `FromPublicKey` yet (defer)
- Thread-safety fixes for `GeniusNodeInstance` — separate concern
- `std::cerr` → `SPDLOG` migration — separate concern
- Null pointer guards on balance/price/version functions — separate concern

## Context

GeniusSDK is a C ABI facade (extern "C") wrapping the `sgns::GeniusNode` from the
SuperGenius project. Game engines (Unity, Unreal) and C consumers link against the
static/shared library. The underlying `GeniusNode` already supports a unified
`New(DevConfig_st, AccountSource)` factory where `AccountSource` is a
`std::variant<NewAccount, FromPrivateKey, FromMnemonic, FromPublicKey>` and node
config (network ports, DHT, full-node mode, processing) is read from JSON files
at `base_path`.

GeniusSDK.cpp at `src/GeniusSDK.cpp:216-297` currently calls old-style methods
(`GeniusNode::New()`, `NewFromPrivateKey()`, `NewFromMnemonic()`) with inline
`autodht` / `baseport` / `is_full_node` / `process` bools. The header at
`src/GeniusSDK.h:203-262` declares 5 init functions, all carrying the old params.

## Constraints

- **C ABI stability**: The public header must remain C-compatible — no C++ types in the API
- **Backward compat**: Callers must only need to drop unused params (recompile, not redesign)
- **External dependency**: `GeniusNode.hpp` lives in the SuperGenius sibling project; the
  new API is already present there — no changes to SuperGenius required
- **Examples**: `example/SDKExample.cpp`, `example/SDKIdleExample.cpp`, and
  `services/service.cpp` must compile after signature changes

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Keep separate per-source C functions | User prefers explicit API over single unified init with type enum | — Pending |
| Drop process param entirely | New GeniusNode reads processing config from JSON | — Pending |
| Strip params from C API, not just implementation | Dead params in the public header confuse consumers | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-07-03 after initialization*
