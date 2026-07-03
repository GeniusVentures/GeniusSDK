# Requirements: GeniusSDK Init Interface Modernization

**Defined:** 2026-07-03
**Core Value:** Every init function takes a `dev_config` JSON string as primary input — `base_path` (already in `BaseWritePath`) and node runtime config (DHT, ports, full-node, processor) move out of the C API entirely.

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### Init Signatures — Public Header

- [ ] **INIT-01**: `GeniusSDKInit()` — strip `base_path`, `autodht`, `process`, `baseport`, `is_full_node`. New signature: `(const char *dev_config)`. Parses JSON to extract `BaseWritePath`, creates new account.
- [ ] **INIT-02**: `GeniusSDKInitWithKey()` — strip `base_path`, `autodht`, `process`, `baseport`, `is_full_node`. New signature: `(const char *dev_config, const char *eth_private_key)`.
- [ ] **INIT-03**: `GeniusSDKInitWithMnemonic()` — strip `base_path`, `autodht`, `process`, `baseport`, `is_full_node`. New signature: `(const char *dev_config, const char *mnemonic)`.
- [ ] **INIT-04**: `GeniusSDKInitWithKeyAndDevConfig()` — **removed**. Redundant — all init functions now take `dev_config` JSON as first parameter.
- [ ] **INIT-05**: `GeniusSDKInitMinimal()` — **removed**. Redundant — same as `GeniusSDKInitWithKey(dev_config, key)`.

### Implementation — GeniusSDK.cpp

- [ ] **IMPL-01**: `SDKInitHelper` template takes `dev_config` JSON string, parses it, extracts `base_path` from `BaseWritePath`, constructs `DevConfig_st`, calls `GeniusNode::New(config, AccountSource{variant})`.
- [ ] **IMPL-02**: `GeniusSDKInit()` → `GeniusNode::New(config, AccountSource{NewAccount{}})`.
- [ ] **IMPL-03**: `GeniusSDKInitWithKey()` → `GeniusNode::New(config, AccountSource{FromPrivateKey{key}})`.
- [ ] **IMPL-04**: `GeniusSDKInitWithMnemonic()` → `GeniusNode::New(config, AccountSource{FromMnemonic{mnemonic}})`.
- [ ] **IMPL-05**: `GeniusSDKInitWithKeyAndDevConfig()` and `GeniusSDKInitMinimal()` implementations removed.
- [ ] **IMPL-06**: Lambda key/mnemonic capture by value (`std::string`) — avoid use-after-free hazard.

### Caller Migration

- [ ] **CALL-01**: Update `example/SDKExample.cpp` — pass `dev_config` JSON string, drop removed params.
- [ ] **CALL-02**: Update `example/SDKIdleExample.cpp` — pass `dev_config` JSON string, drop removed params.
- [ ] **CALL-03**: Update `services/service.cpp` — pass `dev_config` JSON string, drop removed params.
- [ ] **CALL-04**: Examples and services ship `network_config.json` / `sgns_config.json` matching their current hardcoded defaults (port 40001, DHT on, full-node/processor per existing flags).

### Verification

- [ ] **VER-01**: All init functions return init path string on success, null on failure (existing contract preserved).
- [ ] **VER-02**: No remaining references to old factory methods (`NewFromPrivateKey`, `NewFromMnemonic`, old-style `New(autodht, ...)`) in `GeniusSDK.cpp`.
- [ ] **VER-03**: `GeniusSDK.h` Doxygen `@param` tags updated to match new signatures.

## v2 Requirements

Deferred to future release.

- **CONF-01**: Auto-write default `network_config.json` / `sgns_config.json` in SDK layer when files are missing.
- **BUF-01**: Fix `static std::string ret_val` accumulation across init→shutdown→re-init cycles.
- **VER-04**: Set `SOVERSION` on shared library to mark ABI break.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Public key / read-only init (`FromPublicKey`) | No C API function requested yet |
| Thread-safety fixes for `GeniusNodeInstance` | Separate concern |
| `std::cerr` → `SPDLOG` migration | Separate concern |
| Null pointer guards on balance/price/version | Separate concern |
| `GeniusSDKInitWithCredentials` implementation | `SDKExampleCredentials.cpp` stub, not compiled |
| Auto-write default JSON configs in SDK layer | User preference: config files belong in examples/services, not in SDK |
| Keep `GeniusSDKInitWithKeyAndDevConfig` | Redundant — all init calls now take dev_config JSON |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| INIT-01 | — | Pending |
| INIT-02 | — | Pending |
| INIT-03 | — | Pending |
| INIT-04 | — | Pending |
| INIT-05 | — | Pending |
| IMPL-01 | — | Pending |
| IMPL-02 | — | Pending |
| IMPL-03 | — | Pending |
| IMPL-04 | — | Pending |
| IMPL-05 | — | Pending |
| IMPL-06 | — | Pending |
| CALL-01 | — | Pending |
| CALL-02 | — | Pending |
| CALL-03 | — | Pending |
| CALL-04 | — | Pending |
| VER-01 | — | Pending |
| VER-02 | — | Pending |
| VER-03 | — | Pending |

**Coverage:**
- v1 requirements: 17 total
- Mapped to phases: 0 (pending roadmap)
- Unmapped: 17

---
*Requirements defined: 2026-07-03*
*Last updated: 2026-07-03 after revision*
