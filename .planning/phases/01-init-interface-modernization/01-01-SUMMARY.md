---
phase: 01-init-interface-modernization
plan: 01
subsystem: api
tags: [c-abi, refactoring, geniusnode, accountsource, sdk-init]

# Dependency graph
requires: []
provides:
  - "3 C ABI init functions: Init(base_path, dev_config), InitWithKey(base_path, dev_config, key), InitWithMnemonic(base_path, dev_config, mnemonic)"
  - "Zero legacy params (autodht, process, baseport, is_full_node) in public API"
  - "All init paths calling GeniusNode::New(config, AccountSource{variant})"
affects: [callers-update, verify-compilation]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Inline init functions (~12 lines each) with no template indirection"
    - "AccountSource variant dispatch: NewAccount{}, FromPrivateKey{key}, FromMnemonic{mnemonic}"
    - "dev_config JSON string validated inline (null/empty check + RapidJSON parse)"
    - "std::string copy capture before AccountSource construction (prevent use-after-free)"

key-files:
  created: []
  modified:
    - src/GeniusSDK.h
    - src/GeniusSDK.cpp

key-decisions:
  - "D-03: No SDKInitHelper template — each init function inlines ~12 lines directly"
  - "D-04: AccountSource variant dispatch — NewAccount{}, FromPrivateKey{key}, FromMnemonic{mnemonic}"
  - "D-05: Null/empty dev_config returns nullptr with SPDLOG_ERROR — no fallback to disk reads"
  - "D-06: Key/mnemonic captured by value (std::string) before AccountSource construction"
  - "D-07: Three init functions remain — Init, InitWithKey, InitWithMnemonic"
  - "D-08: GeniusSDKInitWithKeyAndDevConfig and GeniusSDKInitMinimal fully removed"
  - "D-09: static std::string ret_val pattern preserved with .assign() reset per function"

patterns-established:
  - "Thin C ABI adapter: validate → parse JSON → call GeniusNode::New → return path/null"
  - "Per-function static ret_val to avoid cross-function interference"

requirements-completed:
  - INIT-01
  - INIT-02
  - INIT-03
  - INIT-04
  - INIT-05
  - IMPL-01
  - IMPL-02
  - IMPL-03
  - IMPL-04
  - IMPL-05
  - IMPL-06

# Metrics
duration: 8min
completed: 2026-07-06
---

# Phase 01 Plan 01: Init Interface Modernization Summary

**Stripped 5 legacy params from 3 C ABI init functions, removed 2 redundant functions, and repointed all init paths to GeniusNode::New(config, AccountSource{variant})**

## Performance

- **Duration:** ~8 min
- **Started:** 2026-07-06
- **Completed:** 2026-07-06
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Header reduced from 70 lines of init declarations (5 functions with 4+ params each) to 30 lines (3 functions with 2-3 params)
- Implementation reduced from 80+ lines (template + 5 function bodies) to ~60 lines (3 inline functions)
- Zero reference to `autodht`, `process`, `baseport`, `is_full_node` in any init signature
- Zero reference to `SDKInitHelper`, `ReadDevConfigFromJSON`, `ReadDevConfigFromJSONStr`, `NewFromPrivateKey`, `NewFromMnemonic`
- All 3 init functions use `GeniusNode::New(config, AccountSource{...})` — unified factory pattern
- String capture guards (D-06) prevent use-after-free from C caller stack unwinding

## Task Commits

Each task was committed atomically:

1. **Task 1: Rewrite Init Function Declarations in GeniusSDK.h** - `988c97c` (feat)
2. **Task 2: Rewrite Init Function Implementations in GeniusSDK.cpp** - `624992d` (feat)

## Files Created/Modified

- `src/GeniusSDK.h` — 3 init declarations: GeniusSDKInit(base_path, dev_config), GeniusSDKInitWithKey(base_path, dev_config, eth_private_key), GeniusSDKInitWithMnemonic(base_path, dev_config, mnemonic)
- `src/GeniusSDK.cpp` — 3 inline init implementations calling GeniusNode::New(config, AccountSource{variant}) with per-function validation and static ret_val

## Decisions Made

None — plan executed exactly as specified. All 9 decision IDs (D-01 through D-09) were followed precisely.

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None. The mechanical refactoring proceeded without issues. All 14 verification checks (7 per task + 7 plan-level) passed on first attempt.

## Threat Flags

None — all threat surface documented in the plan's threat model (T-01-01 through T-01-05). No new surface introduced.

## User Setup Required

None — no external service configuration required. This is purely an internal ABI refactoring.

## Next Phase Readiness

- Header and implementation are in lockstep with matching 3-function signatures
- Ready for caller updates (plan 01-02): `example/SDKExample.cpp`, `example/SDKIdleExample.cpp`, `services/service.cpp`
- No blockers — the new GeniusNode API is already present in SuperGenius

---
## Self-Check: PASSED

- SUMMARY.md exists ✓
- Commit 988c97c (Task 1: header declarations) ✓
- Commit 624992d (Task 2: cpp implementations) ✓
- Modified files exist: src/GeniusSDK.h, src/GeniusSDK.cpp ✓
- No accidental deletions ✓

---
*Phase: 01-init-interface-modernization*
*Completed: 2026-07-06*
