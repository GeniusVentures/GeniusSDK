# Roadmap: GeniusSDK Init Interface Modernization

## Overview

Add `base_path` back to all C ABI init functions alongside `dev_config` JSON, strip dead legacy params (`autodht`, `process`, `baseport`, `is_full_node`), and repoint the SDK facade to the unified `GeniusNode::New(config, AccountSource{...})` factory. Two tightly-coupled phases: all code changes land together in one compilation gate, then formal verification and documentation finalize the refactoring.

## Phases

- [ ] **Phase 1: Init Interface Modernization** — Header signatures, implementation repointing, caller migration, and config file shipping
- [x] **Phase 2: Verification & Documentation** — Return contract validation, dead-reference cleanup, and Doxygen updates (completed 2026-07-06)

## Phase Details

### Phase 1: Init Interface Modernization
**Goal**: The C ABI init surface accepts `base_path` and `dev_config` JSON as the two configuration inputs — no dead legacy params in any signature, and all callers compile with the new interface.
**Mode**: mvp
**Depends on**: Nothing (first phase)
**Requirements**: INIT-01, INIT-02, INIT-03, INIT-04, INIT-05, IMPL-01, IMPL-02, IMPL-03, IMPL-04, IMPL-05, IMPL-06, CALL-01, CALL-02, CALL-03, CALL-04
**Success Criteria** (what must be TRUE):
  1. The public `GeniusSDK.h` header declares exactly 3 init functions (`GeniusSDKInit`, `GeniusSDKInitWithKey`, `GeniusSDKInitWithMnemonic`), each accepting `const char *base_path` as the first parameter, `const char *dev_config` as the second parameter, and zero legacy params (`autodht`, `process`, `baseport`, `is_full_node`)
  2. All 3 init functions call `GeniusNode::New(config, AccountSource{...})` via the unified factory — no references to `NewFromPrivateKey`, `NewFromMnemonic`, or old multi-param `New()` remain in `GeniusSDK.cpp`
  3. All caller files (`example/SDKExample.cpp`, `example/SDKIdleExample.cpp`, `services/service.cpp`) compile against the new signatures, passing `base_path` and `dev_config` JSON strings and no legacy params
  4. `GeniusSDKInitWithKeyAndDevConfig` and `GeniusSDKInitMinimal` are removed from both header and implementation
   5. Examples and service ship `network_config.json` and `sgns_config.json` files matching their previous hardcoded defaults (port 40001, DHT on, full-node/processor per existing flags)
**Plans**: 2 plans

Plans:
- [ ] 01-01-PLAN.md — Header declarations + implementation refactoring (INIT-01..05, IMPL-01..06)
- [ ] 01-02-PLAN.md — Caller migration + config files (CALL-01..04)
**UI hint**: no

### Phase 2: Verification & Documentation
**Goal**: The refactored init interface is formally verified to preserve its return contract, contains no dead code, and has accurate developer-facing documentation.
**Mode**: mvp
**Depends on**: Phase 1
**Requirements**: VER-01, VER-02, VER-03
**Success Criteria** (what must be TRUE):
  1. Every init function returns a non-null path string on success and `nullptr` on failure — the existing return contract is preserved across all code paths
  2. No references to old factory methods (`NewFromPrivateKey`, `NewFromMnemonic`, or old multi-param `New(autodht, ...)`) exist anywhere in `GeniusSDK.cpp`
  3. `GeniusSDK.h` Doxygen `@param` tags accurately describe the new signatures with no references to removed parameters
**Plans**: 1 plan

Plans:
- [x] 02-01-PLAN.md — Static verification sweep (VER-01 return contract, VER-02 dead references, VER-03 Doxygen accuracy) + formal verification report
**UI hint**: no

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Init Interface Modernization | 0/2 | Not started | - |
| 2. Verification & Documentation | 1/1 | Complete   | 2026-07-06 |
