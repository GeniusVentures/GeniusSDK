---
last_mapped_commit: 35507476cbaa482c1c9a6e3582f8475a9f39304c
refreshed: 2026-07-03
---

<!-- refreshed: 2026-07-03 -->
# Architecture

**Analysis Date:** 2026-07-03

## System Overview

```text
┌──────────────────────────────────────────────────────────────────────────┐
│                     Public API (C ABI)                                   │
│                 `src/GeniusSDK.h` (extern "C" block)                     │
│    ~50 exported functions — init, accounts, transfers, mint, process     │
└──────────────────────────────────┬───────────────────────────────────────┘
                                   │ all calls delegate through
                                   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                    SDK Implementation                                    │
│                   `src/GeniusSDK.cpp` (935 lines)                        │
│         Wraps singleton `shared_ptr<sgns::GeniusNode>`                   │
│   Parses dev_config.json, converts C↔C++ types, handles error codes      │
└──────────────────────────────────┬───────────────────────────────────────┘
                                   │ calls into
                                   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                    Core Engine (external sibling project)                │
│  `sgns::GeniusNode` from `SuperGenius/` repo (via CMake `find_package`)  │
│      Blockchain │ Processing │ Transactions │ DHT │ Accounts │ Wallet    │
│     ~30+ dependencies: Boost, RocksDB, libp2p, Protobuf, MNN, etc.      │
└──────────────────────────────────────────────────────────────────────────┘
```

**Pattern:** Facade (Extern C wrapper around a C++ singleton). GeniusSDK provides a stable C ABI for game engines (Unity, Unreal, custom C/C++ apps) to interact with the Genius token blockchain without exposing internal C++ types.

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| Public API Header | Declares all 50+ `extern "C"` functions, C-compatible structs, enums | `src/GeniusSDK.h` |
| SDK Implementation | Singleton lifecycle, JSON config parsing, C↔C++ type marshalling, error conversion | `src/GeniusSDK.cpp` |
| SDK Init Helpers | Template to reduce boilerplate across 4 init variants; JSON config parsing with RapidJSON | `src/GeniusSDK.cpp` (anonymous namespace) |
| GeniusNode (external) | Blockchain engine, account management, processing, transactions, DHT networking | SuperGenius project (`sgns::GeniusNode`) |
| CLI Service | Headless daemon wrapping `GeniusSDKInit()` blocking forever — packaged as systemd services | `services/service.cpp` |
| Example Apps | Menu-driven interactive demo and idle node demo, linked whole-archive against static lib | `example/SDKExample.cpp`, `example/SDKIdleExample.cpp` |
| Build Infrastructure | Cross-platform CMake scaffold (Android, iOS, OSX, Linux, Windows) | `build/` (git submodule) |
| CI/CD | GitHub Actions release matrix build, tagged artifact uploads | `.github/workflows/cmake.yml`, `.github/workflows/build-release-tag.yml` |

## Pattern Overview

**Overall:** Facade + Singleton

**Key Characteristics:**
- **C ABI surface**: All public functions are `extern "C"` with `GNUS_VISIBILITY_DEFAULT` export attribute — the header compiles as both C and C++ (`src/GeniusSDK.h` lines 25-35)
- **Singleton node**: One global `std::shared_ptr<sgns::GeniusNode>` named `GeniusNodeInstance` (`src/GeniusSDK.cpp` line 182) — not thread-safe by itself, wrapped per call
- **do-while(0) error pattern**: Most action functions use `do { check; delegate; break; } while (0)` for early-exit error handling without exceptions (`src/GeniusSDK.cpp` lines 299-320)
- **No exceptions in C ABI**: All errors returned as `GeniusNodeReturnValue_t` (int32 enum) or null pointers — internal C++ exceptions are caught and converted
- **C↔C++ marshalling**: Custom structs (`GeniusArray`, `GeniusMatrix`, `GeniusAddress`, `GeniusTokenValue`, `GeniusTokenID`) enable vector/string interop across the C boundary
- **Whole-archive linking**: Executables (examples, service) link against the static lib via `TARGET_LINK_LIBRARIES_WHOLE_ARCHIVE` — ensures all symbols are pulled in even if not directly referenced

## Layers

**Public API (C ABI):**
- Purpose: Stable, C-compatible interface for game engines and C consumers
- Location: `src/GeniusSDK.h`
- Contains: ~50 `extern "C"` function declarations, C-compatible structs, enums, typedefs
- Depends on: Standard C headers only (`<stdint.h>`, `<stdbool.h>`)
- Used by: External consumers linking against the library; `src/GeniusSDK.cpp`, `example/`, `services/`, `test/`

**SDK Implementation:**
- Purpose: Bridge C API to C++ engine, parse configs, manage singleton lifecycle
- Location: `src/GeniusSDK.cpp`
- Contains: Function bodies for all public API, anonymous namespace with JSON parsing helpers, C↔C++ vector conversion helpers (`matrix_from_vector_of_vector`, `matrix_from_buffer`), singleton `GeniusNodeInstance`
- Depends on: `sgns::GeniusNode`, RapidJSON, Boost (algorithm/hex, outcome, exception), `spdlog`, SuperGenius internal types (`sgns::TokenID`, `sgns::base::Buffer`, `sgns::sgprocessing::ProcessingManager`, `DevConfig_st`)
- Used by: Everything that links against GeniusSDK

**Core Engine (external — SuperGenius):**
- Purpose: Blockchain node, accounts, token transfers, minting, DHT peering, job processing (ML inference via MNN)
- Location: Sibling project at `../SuperGenius/` (resolved via CMake's `find_package(SuperGenius CONFIG REQUIRED)`)
- Contains: `sgns::GeniusNode`, `sgns::TokenID`, `sgns::sgprocessing::ProcessingManager`, transaction management, wallet integration (TrustWalletCore, TrezorCrypto)
- Depends on: ~30+ third-party libraries (Boost 1.85, RocksDB, libp2p, Protobuf, OpenSSL, zkLLVM/crypto3, MNN, etc.)
- Used by: `src/GeniusSDK.cpp` exclusively through the `sgns::GeniusNode` interface

**Build Scaffold (external — cmaketemplate):**
- Purpose: Cross-platform CMake entry points and build toolchain config
- Location: `build/` (git submodule → `GeniusVentures/cmaketemplate`)
- Contains: Per-platform `CMakeLists.txt` (`build/OSX/CMakeLists.txt`, `build/Android/CMakeLists.txt`, etc.), compiler flags, dependency discovery, test/add-proto helpers
- Used by: CI pipeline, developer builds

## Data Flow

### Primary Request Path (SDK Initialization)

1. **Consumer calls init** — e.g., `GeniusSDKInitWithKey(base_path, key, ...)` — entry point at `src/GeniusSDK.cpp:223`
2. **Config loaded** — `SDKInitHelper()` calls `ReadDevConfigFromJSON(base_path)` which opens `dev_config.json` and parses it with RapidJSON → extracts `Address`, `Cut`, `TokenValue`, `TokenID` (`src/GeniusSDK.cpp:89-129`)
3. **Node created** — Lambda calls `sgns::GeniusNode::NewFromPrivateKey(config, key, ...)` — returns `shared_ptr` stored in `GeniusNodeInstance` (`src/GeniusSDK.cpp:204`)
4. **Status returned** — Static string `"Initialized on <path>"` returned to caller, or `nullptr` on failure

### Primary Request Path (Token Transfer)

1. **Consumer calls** `GeniusSDKTransfer(amount, dest, token_id)` — `src/GeniusSDK.cpp:511`
2. **Guard check** — `if (!GeniusNodeInstance) break;` returns `GENIUS_NODE_ERROR_NOT_INITIALIZED`
3. **Type conversion** — C strings → `std::string`, C byte array → `sgns::TokenID::FromBytes()`
4. **Delegate to engine** — `GeniusNodeInstance->TransferFunds(amount, destination, token_id)`
5. **Error mapping** — `outcome::result` → `GeniusNodeReturnValue_t` enum: non-OK maps to `GENIUS_NODE_ERROR_TRANSFER`, OK maps to `GENIUS_NODE_RET_OK`

### Primary Request Path (Processing Job)

1. **Consumer calls** `GeniusSDKProcess(jsondata)` — `src/GeniusSDK.cpp:299`
2. **Guard check** — null `GeniusNodeInstance` → `GENIUS_NODE_ERROR_NOT_INITIALIZED`
3. **Delegate** — `GeniusNodeInstance->ProcessImage(std::string{jsondata})`
4. **Return** — `GENIUS_NODE_RET_OK` or `GENIUS_NODE_ERROR_PROCESS_IMAGE`

**State Management:**
- Single `static std::shared_ptr<sgns::GeniusNode>` holds all runtime state (`src/GeniusSDK.cpp:182`)
- Init creates it, Shutdown `reset()`s it (`src/GeniusSDK.cpp:635`)
- No per-call locks — consumer must serialize if needed (single-threaded assumption for gaming SDK)

## Key Abstractions

**GeniusNodeInstance singleton:**
- Purpose: The single active node instance — created once per process, shared by all API calls
- Location: `src/GeniusSDK.cpp:182` — `std::shared_ptr<sgns::GeniusNode> GeniusNodeInstance;`
- Pattern: Module-level global (not truly a singleton class, no lazy init, no thread safety)
- Access: Direct reference from every API function — no getter/accessor

**SDKInitHelper template:**
- Purpose: Factor out shared init logic (load config, create node, format return string) from 4 init variants
- Location: `src/GeniusSDK.cpp:184-213`
- Pattern: Template function taking a `Creator` callable — each init variant provides its own factory lambda
- Variants: `GeniusSDKInit` (existing wallet), `GeniusSDKInitWithKey` (private key), `GeniusSDKInitWithMnemonic` (mnemonic phrase)
- Exception: `GeniusSDKInitWithKeyAndDevConfig` doesn't use the helper — it has its own config-from-string path

**C-Compatible Interop Structs:**
- `GeniusArray` (`uint64_t size, uint8_t *ptr`) — mirrors C++ `std::vector<uint8_t>` across C boundary (`src/GeniusSDK.h:45-49`)
- `GeniusMatrix` (`uint64_t size, GeniusArray *ptr`) — mirrors `std::vector<std::vector<uint8_t>>` (`src/GeniusSDK.h:51-55`)
- `GeniusAddress` — fixed `char[131]` for 0x-prefixed hex address (`src/GeniusSDK.h:59-64`)
- `GeniusTokenValue` — fixed `char[22]` for formatted GNUS value (`src/GeniusSDK.h:72-75`)
- `GeniusTokenID` — fixed `unsigned char[32]` for raw token identifier (`src/GeniusSDK.h:80-83`)
- All designed for stack allocation by C consumers — no heap allocation required for the structs themselves

**do-while(0) Error Pattern:**
- Purpose: Structured error handling without goto in functions that need early return on guard failures
- Pattern (seen in `src/GeniusSDK.cpp:299-320, 422-445, 512-534`):
  ```cpp
  GeniusNodeReturnValue ret = GENIUS_NODE_ERROR_NOT_INITIALIZED;
  do {
      if (!GeniusNodeInstance) break;
      auto result = GeniusNodeInstance->SomeMethod(args);
      if (!result.has_value()) { ret = ERROR_CODE; break; }
      ret = GENIUS_NODE_RET_OK;
  } while (0);
  return ret;
  ```

## Entry Points

### Initialization (5 variants)

| Function | Location | Use Case |
|----------|----------|----------|
| `GeniusSDKInit` | `src/GeniusSDK.cpp:216` | Use existing local wallet (no key provided) |
| `GeniusSDKInitWithKey` | `src/GeniusSDK.cpp:223` | Ethereum private key as hex string |
| `GeniusSDKInitWithMnemonic` | `src/GeniusSDK.cpp:281` | BIP39 mnemonic phrase |
| `GeniusSDKInitWithKeyAndDevConfig` | `src/GeniusSDK.cpp:241` | Inline dev_config JSON + private key |
| `GeniusSDKInitMinimal` | `src/GeniusSDK.cpp:294` | Convenience: autoDHT=true, process=true, isFullNode=false |

All variants accept `base_path` (must contain `dev_config.json`), `autodht`, `process`, `baseport`, `is_full_node`. All return `const char*` (initialization path or null).

### Shutdown

- `GeniusSDKShutdown()` — `src/GeniusSDK.cpp:630` — `reset()`s the `GeniusNodeInstance` shared_ptr

### Account Management

| Function | Location |
|----------|----------|
| `GeniusSDKGetAvailableAccounts()` | `src/GeniusSDK.cpp:666` |
| `GeniusSDKAddAccountWithPrivateKey()` | `src/GeniusSDK.cpp:700` |
| `GeniusSDKAddAccountWithMnemonic()` | `src/GeniusSDK.cpp:713` |
| `GeniusSDKAddAccountWithRandomMnemonic()` | `src/GeniusSDK.cpp:726` |
| `GeniusSDKSelectGeniusAccount()` | `src/GeniusSDK.cpp:742` |
| `GeniusSDKTransferGeniusAccount()` | `src/GeniusSDK.cpp:756` |
| `GeniusSDKMergeGeniusAccount()` | `src/GeniusSDK.cpp:770` |
| `GeniusSDKDeleteAccount()` | `src/GeniusSDK.cpp:784` |
| `GeniusSDKSetPayoutAddress()` | `src/GeniusSDK.cpp:798` |
| `GeniusSDKGetAddress()` | `src/GeniusSDK.cpp:475` |
| `GeniusSDKGetMnemonic()` | `src/GeniusSDK.cpp:490` |

### Token Operations

| Function | Location |
|----------|----------|
| `GeniusSDKGetBalance()` | `src/GeniusSDK.cpp:351` |
| `GeniusSDKGetBalanceGNUS()` | `src/GeniusSDK.cpp:356` |
| `GeniusSDKGetBalanceGNUSString()` | `src/GeniusSDK.cpp:374` |
| `GeniusSDKMint()` | `src/GeniusSDK.cpp:417` |
| `GeniusSDKMintGNUS()` | `src/GeniusSDK.cpp:447` |
| `GeniusSDKTransfer()` | `src/GeniusSDK.cpp:511` |
| `GeniusSDKTransferGNUS()` | `src/GeniusSDK.cpp:536` |
| `GeniusSDKPayDev()` | `src/GeniusSDK.cpp:570` |
| `GeniusSDKGetGNUSPrice()` | `src/GeniusSDK.cpp:333` |

### Processing & Transactions

| Function | Location |
|----------|----------|
| `GeniusSDKGetCost()` | `src/GeniusSDK.cpp:592` |
| `GeniusSDKGetCostGNUS()` | `src/GeniusSDK.cpp:602` |
| `GeniusSDKProcess()` | `src/GeniusSDK.cpp:299` |
| `GeniusSDKCheckJobValidity()` | `src/GeniusSDK.cpp:322` |
| `GeniusSDKGetMyTaskIds()` | `src/GeniusSDK.cpp:877` |
| `GeniusSDKGetTaskResult()` | `src/GeniusSDK.cpp:912` |
| `GeniusSDKGetInTransactions()` | `src/GeniusSDK.cpp:398` |
| `GeniusSDKGetOutTransactions()` | `src/GeniusSDK.cpp:403` |
| `GeniusSDKFreeTransactions()` | `src/GeniusSDK.cpp:408` |
| `GeniusSDKGetTransactionStatus()` | `src/GeniusSDK.cpp:830` |

### Status & Version

| Function | Location |
|----------|----------|
| `GeniusSDKGetInitializationStatus()` | `src/GeniusSDK.cpp:654` |
| `GeniusSDKGetNodeState()` | `src/GeniusSDK.cpp:821` |
| `GeniusSDKGetTransactionManagerState()` | `src/GeniusSDK.cpp:812` |
| `GeniusSDKGetProcessingStatus()` | `src/GeniusSDK.cpp:842` |
| `GeniusSDKGetVersion()` | `src/GeniusSDK.cpp:345` |
| `GeniusSDKLoadLogConfig()` | `src/GeniusSDK.cpp:641` |
| `GeniusSDKFree()` | `src/GeniusSDK.cpp:649` |

### Executable Entry Points

| Binary | Source | Purpose |
|--------|--------|---------|
| `GeniusSDKService` | `services/service.cpp` | CLI daemon: parses args, calls `GeniusSDKInit`, blocks forever |
| `SDKExample` | `example/SDKExample.cpp` | Interactive menu-driven demo: init, wallet, transactions, processing |
| `SDKIdleExample` | `example/SDKIdleExample.cpp` | Minimal: init with hardcoded key, block forever |

## Architectural Constraints

- **Threading:** Not thread-safe by design. The `GeniusNodeInstance` singleton is unprotected. Consumer must serialize all SDK calls from a single thread. This is acceptable for Unity/Unreal game loops which run on a main thread.
- **Global state:** One module-level shared_ptr (`src/GeniusSDK.cpp:182`). No lazy init, no double-check locking.
- **Single instance:** Only one `GeniusNodeInstance` per process. Calling init twice without shutdown is undefined behavior.
- **C ABI stability:** The public header is the API contract. Internal C++ types (`sgns::GeniusNode`, `outcome::result`, `boost::*`) are never exposed. All types crossing the boundary are POD structs or enums.
- **Config dependency:** `dev_config.json` MUST exist at `base_path` — required fields: `Address`, `Cut`, `TokenValue`, `TokenID`. Missing any field → init returns null with error message.
- **Build platform isolation:** Source code is single-platform; platform differences are handled entirely by the `build/` CMake scaffold (toolchains, compiler flags, export macros).
- **Dependency resolution:** All third-party prebuilt binaries must exist at `THIRDPARTY_BUILD_DIR` before CMake configure. The build system can auto-download from GitHub releases if they're missing (`build/CommonCompilerOptions.cmake`).

## Anti-Patterns

### Static char buffer for return string

**What happens:** `GeniusSDKGetBalanceGNUSString()` returns a pointer to a `static char buffer[64]` (`src/GeniusSDK.cpp:379`). Every call overwrites the same buffer.
**Why it's wrong:** If a consumer stores two balance pointers from different accounts, the second call silently corrupts the first result. Not thread-safe even with serialized access (interleaved calls).
**Do this instead:** Either (a) return heap-allocated strings the caller must free (like `GeniusSDKGetAvailableAccounts` does), (b) use `GeniusTokenValue` struct (stack allocated by caller — already the preferred API in `GeniusSDKGetBalanceGNUS()`), or (c) use a thread-local buffer.

### Separate init paths (SDKInitHelper vs inline)

**What happens:** 3 of the 4 init variants use the `SDKInitHelper` template; `GeniusSDKInitWithKeyAndDevConfig` duplicates the same logic inline with subtle differences (uses `ReadDevConfigFromJSONStr` instead of `ReadDevConfigFromJSON`, has its own static string and null checks).
**Why it's wrong:** Bug fixes in one path may not propagate to the other. The `process` parameter in `SDKInitHelper` is passed to the lambda but never actually used (the lambda takes only `config`).
**Do this instead:** Make `SDKInitHelper` accept an optional JSON string override parameter, unifying all paths.

### Unused `process` parameter in init lambdas

**What happens:** `GeniusSDKInit`, `GeniusSDKInitWithKey`, and `GeniusSDKInitWithMnemonic` all accept a `bool process` parameter, pass it to `SDKInitHelper` which passes it to the factory lambda — but the lambda signature is `[&](const auto &config)` and never references the captured `process` variable (`src/GeniusSDK.cpp:218-220`, `231-238`, `290-291`).
**Why it's wrong:** The parameter appears in the public API and is passed through the chain, but the factory functions (`GeniusNode::New`, `NewFromPrivateKey`, `NewFromMnemonic`) don't accept a process flag. If the engine later supports it, behavior may silently change when the parameter starts being used.
**Do this instead:** Either deprecate/remove the parameter or plumb it through to the GeniusNode factory.

## Error Handling

**Strategy:** Return codes + null pointers. No exceptions cross the C ABI boundary.

**Patterns:**
- All action functions return `GeniusNodeReturnValue_t` (an `int32_t` enum) — `GENIUS_NODE_RET_OK = 0` on success, specific error codes on failure
- Init functions return `const char*` — non-null on success (initialization path string), null on failure
- Data retrieval functions return zero-initialized structs or null pointers on failure
- Internal C++ exceptions from Boost, RapidJSON, etc. are caught at the `GeniusNode` level (within SuperGenius) and converted to `outcome::result` errors

**Error enum values** (defined in `src/GeniusSDK.h:93-102`):
| Value | Condition |
|-------|-----------|
| `GENIUS_NODE_RET_OK` | Success |
| `GENIUS_NODE_ERROR_NOT_INITIALIZED` | `GeniusNodeInstance` is null (SDK not inited) |
| `GENIUS_NODE_ERROR_PROCESS_IMAGE` | Processing job submission failed |
| `GENIUS_NODE_ERROR_MINT` | Token minting failed |
| `GENIUS_NODE_INVALID_ARGUMENT` | Null or malformed input parameter |
| `GENIUS_NODE_ERROR_TRANSFER` | Token transfer failed |
| `GENIUS_NODE_ERROR_PAY_DEV` | Developer payout failed |

## Cross-Cutting Concerns

**Logging:** `spdlog` via `SPDLOG_ERROR` macro (`src/GeniusSDK.cpp:191, 200`) — writes to stderr in default configuration. Log level can be reloaded at runtime via `GeniusSDKLoadLogConfig()` which delegates to `GeniusNodeInstance->LoadLogConfig()`.

**Validation:** Input validation is inconsistent — some functions validate null pointers (e.g., `GeniusSDKTransferGNUS` checks `amount` and `dest` at lines 545-554), others don't (e.g., `GeniusSDKGetVersion()` dereferences `GeniusNodeInstance` without null check at line 347). The `do-while(0)` pattern is used in some action functions but not others.

**Memory ownership:** The rule is "caller frees SDK-allocated memory with `GeniusSDKFree()`" — applies to `GeniusStatusInfo.message`, return value of `GetAvailableAccounts()`, `GetMyTaskIds()`, and `GetTaskResult().ptr`. This is documented in the header for each relevant function.

**Authentication:** Account authentication is delegated to SuperGenius's `wallet-core` (TrustWalletCore + TrezorCrypto). The SDK accepts private keys or mnemonics as plain C strings — no encryption at the SDK layer.

---

*Architecture analysis: 2026-07-03*
