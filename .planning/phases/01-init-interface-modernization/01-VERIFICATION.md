---
phase: 01-init-interface-modernization
verified: 2026-07-06T17:00:00Z
status: passed
score: 11/11 must-haves verified
overrides_applied: 0
---

# Phase 01: Init Interface Modernization — Verification Report

**Phase Goal:** The C ABI init surface accepts base_path and dev_config JSON as the two configuration inputs — no dead legacy params in any signature, and all callers compile with the new interface.

**Verified:** 2026-07-06T17:00:00Z
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| P1-1 | Header declares exactly 3 init functions: Init(base_path, dev_config), InitWithKey(base_path, dev_config, key), InitWithMnemonic(base_path, dev_config, mnemonic) | ✓ VERIFIED | `src/GeniusSDK.h` L200, L209-211, L220-222 — exactly 3 `GNUS_VISIBILITY_DEFAULT const char *GeniusSDKInit` declarations with correct signatures |
| P1-2 | No autodht, process, baseport, or is_full_node parameters remain in any public init signature | ✓ VERIFIED | `grep -r 'autodht|baseport|is_full_node' src/GeniusSDK.h` → zero matches |
| P1-3 | GeniusSDKInitWithKeyAndDevConfig and GeniusSDKInitMinimal do not exist in header or cpp | ✓ VERIFIED | `grep -r 'GeniusSDKInitWithKeyAndDevConfig|GeniusSDKInitMinimal' src/` → zero matches in `.h` or `.cpp` |
| P1-4 | Each init function calls GeniusNode::New(config, AccountSource{variant}) | ✓ VERIFIED | `src/GeniusSDK.cpp` L179 (NewAccount{}), L207 (FromPrivateKey{key_copy}), L235 (FromMnemonic{mnemonic_copy}) — exactly 3 calls, all using unified factory |
| P1-5 | Null/empty dev_config returns nullptr with SPDLOG_ERROR — no fallback to disk reads | ✓ VERIFIED | `src/GeniusSDK.cpp` L168, L190, L218 — all 3 functions check `dev_config[0] == '\0'` with `SPDLOG_ERROR` + `return nullptr` |
| P1-6 | Key/mnemonic strings captured by value (std::string) before passing to AccountSource | ✓ VERIFIED | `src/GeniusSDK.cpp` L206 (`std::string key_copy`), L234 (`std::string mnemonic_copy`) — exactly 2 captures |
| P2-1 | SDKExample.cpp calls GeniusSDKInitWithKey with (base_path, dev_config, key) — no legacy params | ✓ VERIFIED | `example/SDKExample.cpp` L213: `GeniusSDKInitWithKey( base_path, dev_config, eth_private_key )` — 3 args, zero legacy params in initSDK or getSDKConfig |
| P2-2 | SDKIdleExample.cpp calls GeniusSDKInitWithKey with (base_path, dev_config, key) — no legacy params | ✓ VERIFIED | `example/SDKIdleExample.cpp` L25-26: `GeniusSDKInitWithKey( no_path, dev_config.c_str(), "deadbeef...")` — 3 args, includes `<fstream>`/`<sstream>` |
| P2-3 | service.cpp calls GeniusSDKInit with (base_path, dev_config) — no CLI flags for DHT/port/full-node | ✓ VERIFIED | `services/service.cpp` L35: `GeniusSDKInit( base_path, dev_config.c_str() )` — 2 args, usage shows only `<base_path>` |
| P2-4 | example/ ships network_config.json and sgns_config.json matching hardcoded defaults | ✓ VERIFIED | `example/network_config.json`: `{"port_seed": 40001, "auto_dht": true}`; `example/sgns_config.json`: `{"enable_processing": true, "is_full_node": false}` |
| P2-5 | services/ ships network_config.json and sgns_config.json matching same defaults | ✓ VERIFIED | `services/network_config.json`: `{"port_seed": 40001, "auto_dht": true}`; `services/sgns_config.json`: `{"enable_processing": true, "is_full_node": false}` |

**Score:** 11/11 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/GeniusSDK.h` | 3 init declarations with dev_config as 2nd param | ✓ VERIFIED | 512 lines, substantive — 3 declarations at L200-222, Doxygen tags updated, `extern "C"` wrapping via `GNUS_EXPORT_BEGIN`/`GNUS_EXPORT_END`, zero legacy params |
| `src/GeniusSDK.cpp` | 3 inline init implementations calling GeniusNode::New | ✓ VERIFIED | 880 lines, substantive — 3 init functions at L166-242, each ~13 lines inline. ParseDevConfig preserved (L90-129). SDKInitHelper/ReadDevConfigFromJSON/old factory methods all removed |
| `example/SDKExample.cpp` | Updated initSDK loading dev_config.json | ✓ VERIFIED | 703 lines, substantive — `initSDK()` L195-221 loads `dev_config.json` via `loadJsonFromFile`, calls 3-arg `GeniusSDKInitWithKey`. `getSDKConfig()` L467-476 stripped to 2 params |
| `example/SDKIdleExample.cpp` | Updated init with file-based dev_config loading | ✓ VERIFIED | 32 lines, substantive — reads `dev_config.json` via `std::ifstream`+`std::stringstream`, calls 3-arg `GeniusSDKInitWithKey` |
| `services/service.cpp` | Updated headless daemon, 2-arg init | ✓ VERIFIED | 41 lines, substantive — accepts only `<base_path>` CLI arg, reads `dev_config.json`, calls 2-arg `GeniusSDKInit` |
| `example/network_config.json` | Port 40001, DHT enabled | ✓ VERIFIED | 4 lines, valid JSON, correct values |
| `example/sgns_config.json` | Processing enabled, not full node | ✓ VERIFIED | 4 lines, valid JSON, correct values |
| `services/network_config.json` | Port 40001, DHT enabled | ✓ VERIFIED | 4 lines, valid JSON, correct values |
| `services/sgns_config.json` | Processing enabled, not full node | ✓ VERIFIED | 4 lines, valid JSON, correct values |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `GeniusSDK.h` init declarations | `GeniusSDK.cpp` init implementations | `extern "C"` linkage via `GNUS_EXPORT_BEGIN` | ✓ WIRED | Header L28 `extern "C"` (via macro); all 3 cpp implementations match header parameter counts/types exactly |
| `GeniusSDK.cpp` init functions | `GeniusNode::New(config, AccountSource{variant})` | 3 variant dispatches | ✓ WIRED | L179 `NewAccount{}`, L207 `FromPrivateKey{key_copy}`, L235 `FromMnemonic{mnemonic_copy}` — all 3 using unified factory |
| `GeniusNodeInstance` assignment | `static std::string ret_val` | null-check before returning | ✓ WIRED | L180-181, L208-209, L236-237 — `if (!GeniusNodeInstance) return nullptr;` before `ret_val.assign()`+`ret_val.append()` in all 3 functions |
| `SDKExample.cpp initSDK()` | `GeniusSDKInitWithKey` | `loadJsonFromFile` → 3-arg call | ✓ WIRED | L206-213: reads `{base_path}/dev_config.json`, passes to `GeniusSDKInitWithKey(base_path, dev_config, eth_private_key)` |
| `SDKIdleExample.cpp main()` | `GeniusSDKInitWithKey` | `std::ifstream` read → 3-arg call | ✓ WIRED | L18-26: reads `./dev_config.json` via `std::stringstream`, calls 3-arg init |
| `service.cpp main()` | `GeniusSDKInit` | `argv[1]` + file read → 2-arg call | ✓ WIRED | L20-35: reads `{base_path}dev_config.json`, calls 2-arg `GeniusSDKInit` |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|---------------|--------|--------------------|--------|
| `GeniusSDKInit()` | `cfg` (DevConfig_st) | `ParseDevConfig(std::string(dev_config), std::string(base_path))` | ✓ Yes — RapidJSON parser with field validation | ✓ FLOWING |
| `GeniusSDKInitWithKey()` | `cfg` (DevConfig_st) | `ParseDevConfig(std::string(dev_config), std::string(base_path))` | ✓ Yes — same parser | ✓ FLOWING |
| `GeniusSDKInitWithMnemonic()` | `cfg` (DevConfig_st) | `ParseDevConfig(std::string(dev_config), std::string(base_path))` | ✓ Yes — same parser | ✓ FLOWING |
| `SDKExample initSDK()` | `dev_config` (JsonData_t) | `loadJsonFromFile(config_path, dev_config)` | ✓ Yes — reads from filesystem via `fread` | ✓ FLOWING |
| `SDKIdleExample main()` | `dev_config` (std::string) | `std::ifstream + rdbuf()` → `std::stringstream` | ✓ Yes — reads from filesystem | ✓ FLOWING |
| `service main()` | `dev_config` (std::string) | `std::ifstream + rdbuf()` → `std::stringstream` | ✓ Yes — reads from filesystem | ✓ FLOWING |

### Behavioral Spot-Checks

**Step 7b: SKIPPED** — No runnable entry points. The phase produces C++ library code and compiled example binaries that require the full SuperGenius engine to link and run. All verification is structural/static.

### Probe Execution

**Step 7c: SKIPPED** — No probes declared in PLAN.md or SUMMARY.md. Not a migration/tooling phase.

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| INIT-01 | 01-01 | `GeniusSDKInit()` stripped of legacy params, new signature `(base_path, dev_config)` | ✓ SATISFIED | `GeniusSDK.h` L200, `GeniusSDK.cpp` L166-186 |
| INIT-02 | 01-01 | `GeniusSDKInitWithKey()` stripped of legacy params, new signature adds `dev_config` | ✓ SATISFIED | `GeniusSDK.h` L209-211, `GeniusSDK.cpp` L188-214 |
| INIT-03 | 01-01 | `GeniusSDKInitWithMnemonic()` stripped of legacy params, new signature adds `dev_config` | ✓ SATISFIED | `GeniusSDK.h` L220-222, `GeniusSDK.cpp` L216-242 |
| INIT-04 | 01-01 | `GeniusSDKInitWithKeyAndDevConfig()` removed | ✓ SATISFIED | Zero occurrences in `src/GeniusSDK.{h,cpp}` |
| INIT-05 | 01-01 | `GeniusSDKInitMinimal()` removed | ✓ SATISFIED | Zero occurrences in `src/GeniusSDK.{h,cpp}` |
| IMPL-01 | 01-01 | Init path calls `GeniusNode::New(config, AccountSource{variant})` | ✓ SATISFIED (plan alternative) | Plan D-03 eliminated the template — 3 inline functions achieve same outcome. See note below. |
| IMPL-02 | 01-01 | `GeniusSDKInit()` → `GeniusNode::New(NewAccount{})` | ✓ SATISFIED | `GeniusSDK.cpp` L179: `AccountSource{NewAccount{}}` |
| IMPL-03 | 01-01 | `GeniusSDKInitWithKey()` → `GeniusNode::New(FromPrivateKey{key})` | ✓ SATISFIED | `GeniusSDK.cpp` L207: `AccountSource{FromPrivateKey{key_copy}}` |
| IMPL-04 | 01-01 | `GeniusSDKInitWithMnemonic()` → `GeniusNode::New(FromMnemonic{mnemonic})` | ✓ SATISFIED | `GeniusSDK.cpp` L235: `AccountSource{FromMnemonic{mnemonic_copy}}` |
| IMPL-05 | 01-01 | `GeniusSDKInitWithKeyAndDevConfig()` and `GeniusSDKInitMinimal()` implementations removed | ✓ SATISFIED | Zero occurrences in `src/GeniusSDK.cpp`; SDKInitHelper template also removed per D-03 |
| IMPL-06 | 01-01 | Lambda key/mnemonic capture by value (std::string) | ✓ SATISFIED | `GeniusSDK.cpp` L206 (`std::string key_copy`), L234 (`std::string mnemonic_copy`) |
| CALL-01 | 01-02 | `example/SDKExample.cpp` updated with new init call | ✓ SATISFIED | `SDKExample.cpp` L213: 3-arg `GeniusSDKInitWithKey`, `getSDKConfig` stripped to 2 params |
| CALL-02 | 01-02 | `example/SDKIdleExample.cpp` updated with new init call | ✓ SATISFIED | `SDKIdleExample.cpp` L25-26: 3-arg call, file-based dev_config loading |
| CALL-03 | 01-02 | `services/service.cpp` updated with new init call | ✓ SATISFIED | `service.cpp` L35: 2-arg call, `parse_bool` removed, usage shows only `<base_path>` |
| CALL-04 | 01-02 | Examples and services ship config files matching defaults | ✓ SATISFIED | 4 config files at `example/network_config.json`, `example/sgns_config.json`, `services/network_config.json`, `services/sgns_config.json` |

**Note on IMPL-01:** The original requirement described a `SDKInitHelper` template approach. Plan 01-01 (D-03) deliberately chose to eliminate the template in favor of inline init functions (~13 lines each). This produces identical functional behavior — each init function calls `GeniusNode::New(config, AccountSource{variant})` — while reducing indirection. The plan documented this decision. The requirement's functional intent (unified factory dispatch) is fully satisfied.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| — | — | — | — | No anti-patterns found |

**Debt markers:** Zero `TBD`/`FIXME`/`XXX` markers in any modified file.
**Stubs:** Zero `return null`/`return {}` stubs in init functions — all `return nullptr` are genuine error-handling paths with prior validation.
**Hardcoded empty data:** None in the init path.

### Human Verification Required

None — all must-haves are verifiable via static code analysis. The phase is a structural refactoring of C ABI declarations, implementation bodies, and caller wiring. Every truth is binary (exists/doesn't exist, matches/doesn't match).

---

## Verification Summary

**All 11 must-haves verified.** The phase goal is fully achieved:

1. **Header:** Exactly 3 init functions with `(base_path, dev_config)` signatures. Zero legacy params. Zero removed functions. Doxygen `@param` tags updated to match new signatures.
2. **Implementation:** 3 inline init functions (~13 lines each), all calling `GeniusNode::New(config, AccountSource{variant})`. Old template, file-based config reading, and legacy factory methods (`NewFromPrivateKey`, `NewFromMnemonic`) fully removed. String-capture guards (D-06) prevent use-after-free.
3. **Callers:** All 3 caller files (`SDKExample.cpp`, `SDKIdleExample.cpp`, `service.cpp`) updated to new signatures. Zero legacy params in any init call. Each caller loads `dev_config.json` from `base_path` and passes the JSON string to init.
4. **Config files:** 4 JSON config files (`network_config.json` + `sgns_config.json` in both `example/` and `services/`) ship with correct defaults (port 40001, DHT on, processing on, not full-node).
5. **Build integration:** `example/CMakeLists.txt` copies `network_config.json` and `sgns_config.json` to build output.
6. **Out of scope file:** `example/SDKExampleCredentials.cpp` (L9) still has the legacy `GeniusSDKInitWithCredentials(...)` call with 6 args — this is documented as "Out of Scope / not compiled" in REQUIREMENTS.md and is expected.

**Requirements coverage:** All 15 Phase 1 requirement IDs (INIT-01 through INIT-05, IMPL-01 through IMPL-06, CALL-01 through CALL-04) are satisfied. IMPL-01 was satisfied via the plan's documented alternative (inline vs. template, D-03).

**Phase 2 readiness:** VER-01 (init returns path/null), VER-02 (no old factory refs), and VER-03 (Doxygen tags) are mapped to Phase 2. Of these: VER-01 and VER-02 are already fully satisfied by the Phase 1 implementation. VER-03 (Doxygen update) is also already done. The REQUIREMENTS.md traceability table still shows these as "Phase 2 | Pending" — a metadata update is needed but does not block Phase 1 closure.

---

*Verified: 2026-07-06T17:00:00Z*
*Verifier: gsd-verifier agent*
