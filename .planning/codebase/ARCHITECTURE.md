<!-- refreshed: 2026-06-08 -->
# Architecture

**Analysis Date:** 2026-06-08

## System Overview

```text
+-------------------------------------------------------------+
|                      Public C API                           |
|  `src/GeniusSDK.h` (345 lines) — extern "C" boundary        |
+----------------------------+-------------------------------+
|     Static linking         |  Shared/Dynamic linking        |
|  `libGeniusSDK.a`          |  `libGeniusSDK_shared.dylib`   |
|                            |  `GeniusSDK.bundle` (macOS)    |
|                            |  `GeniusSDK.framework` (iOS)   |
+----------------------------+-------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                GeniusSDK Implementation                     |
|  `src/GeniusSDK.cpp` (~22,000 lines)                        |
|  - Singleton: `GeniusNodeInstance` (shared_ptr<GeniusNode>) |
|  - Factory calls: `sgns::GeniusNode::New()`                 |
|  - JSON parsing: RapidJSON                                  |
|  - Error handling: Boost.Outcome + Boost.Exception          |
+----------------------------+-------------------------------+
                              |
                    links to `sgns::genius_node`
                              |
                              v
+-------------------------------------------------------------+
|                  SuperGenius Library                        |
|  `../SuperGenius/` (sibling repository)                     |
|  Configured via `find_package(SuperGenius CONFIG REQUIRED)` |
+----------------------------+-------------------------------+
                              |
              depends on 40+ third-party libraries
                              |
                              v
+-------------------------------------------------------------+
|                    Thirdparty Dependencies                  |
|  `../thirdparty/` — pre-built, platform-typed, auto-fetched |
|  Boost 1.85, RocksDB, OpenSSL, Protobuf, libp2p, etc...    |
+-------------------------------------------------------------+
```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| Public API | C-compatible function declarations, type definitions, enums, visibility macros | `src/GeniusSDK.h` |
| Implementation | Node lifecycle (init/shutdown), token operations (balance/mint/transfer), processing, transaction management | `src/GeniusSDK.cpp` |
| Examples | Three standalone executables demonstrating usage | `example/SDKExample.cpp`, `example/SDKIdleExample.cpp`, `example/SDKExampleCredentials.cpp` |
| Tests | Unit tests for transactions and SDK | `test/CMakeLists.txt` (references `.cpp` files not checked in) |
| Build system | Cross-platform CMake infrastructure, dependency discovery, packaging | `build/` (submodule), `cmake/` |
| Dependency config | Centralised find_package for all 40+ third-party libs | `cmake/CommonBuildParameters.cmake` |
| Compiler config | Compiler flags, C++ standard, warning management, sanitizer support | `cmake/CompilationFlags.cmake`, `build/CommonCompilerOptions.cmake`, `build/cmake/definition.cmake` |
| Platform toolchain | Apple-specific (ios-cmake fork), Android NDK, Windows MSVC | `build/apple.toolchain.cmake`, `build/Android/CMakeLists.txt`, `build/cmake/compile_option_by_platform/Windows.cmake` |

## Pattern Overview

**Overall:** Thin C-wrapper over C++ blockchain library (Facade / Adapter pattern)

**Key Characteristics:**
- The public API is a pure-C `extern "C"` interface in `src/GeniusSDK.h`, decorated with platform-specific visibility attributes (`GNUS_VISIBILITY_DEFAULT`)
- The implementation (`src/GeniusSDK.cpp`) uses C++ internally, including Boost, RapidJSON, and the `sgns` SuperGenius namespace
- A single static file holds all SDK logic — no modular subdirectories
- The SuperGenius library provides the actual blockchain node, transaction manager, and processing pipeline
- Error handling uses `outcome::result<T, E>` for internal operations; public C functions return error codes (`GeniusNodeReturnValue_t`)

## Layers

**Public API Layer:**
- Purpose: Expose a stable C ABI for cross-language and cross-platform consumption (game engines, Unity, Unreal)
- Location: `src/GeniusSDK.h`
- Contains: Function declarations, struct definitions, enums
- Depends on: Nothing (C types only)
- Used by: External consumers, example executables, tests

**Implementation Layer:**
- Purpose: Translate C API calls into C++ SuperGenius operations
- Location: `src/GeniusSDK.cpp`
- Contains: Static helper functions (ParseTokenID, matrix_from_buffer), singleton GeniusNode instance, initialization paths, token operations
- Depends on: SuperGenius (`sgns::GeniusNode`, `sgns::TokenID`, `sgns::sgprocessing::ProcessingManager`), Boost, RapidJSON
- Used by: Nothing internally; linked by examples and tests

**SuperGenius Layer:**
- Purpose: Blockchain node operations (balance, transfer, mint, processing, DHT, transactions)
- Location: `../SuperGenius/` (sibling repository)
- Contains: Full node and client mode blockchain implementation
- Depends on: All third-party libraries
- Used by: GeniusSDK implementation

**Build Infrastructure Layer:**
- Purpose: Cross-platform CMake configuration, dependency resolution, packaging
- Location: `build/` (submodule from `GeniusVentures/cmaketemplate`) + `cmake/`
- Contains: Platform CMakeLists, common build parameters, compiler flags, toolchain files, install logic
- Depends on: CMake 3.22+, Ninja/Visual Studio/Xcode, thirdparty repository
- Used by: All build targets

## Data Flow

### Primary Initialization Path

1. Caller invokes `GeniusSDKInit()` or variant via the C API (`src/GeniusSDK.h:165`)
2. Implementation parses configuration JSON from file path (`src/GeniusSDK.cpp` ~line 210)
3. Constructs `sgns::GeniusNode::New(config, autodht, process, baseport, is_full_node, true)` (`src/GeniusSDK.cpp` ~line 216)
4. Stores result in `GeniusNodeInstance` shared_ptr singleton (`src/GeniusSDK.cpp:176`)
5. Returns the configuration path string on success, `nullptr` on failure

### Token Balance Query Path

1. Caller invokes `GeniusSDKGetBalance(token_id)` (`src/GeniusSDK.h:212`)
2. Implementation calls `GeniusNodeInstance->GetBalance(sgns::TokenID::FromBytes(...))` (`src/GeniusSDK.cpp:359`)
3. Returns raw `uint64_t` Minion token amount

### Processing Submission Path

1. Caller invokes `GeniusSDKProcess(jsondata)` (`src/GeniusSDK.h:311`)
2. Implementation creates `sgns::sgprocessing::ProcessingManager::Create(jsondata)` (`src/GeniusSDK.cpp:334`)
3. Submits processing job; returns error code via `GeniusNodeReturnValue_t`

### Build Configuration Flow

1. Platform CMakeLists.txt (`build/<Platform>/CMakeLists.txt`) sets toolchain, includes `build/cmake/functions.cmake`
2. `get_default_root()` resolves `PROJECT_ROOT` and `PROJECT_ROOT_NAME`
3. `project()` call establishes the build
4. `include(../CommonCompilerOptions.cmake)` sets C++17, compiler flags, discovers thirdparty and zkLLVM
5. `include(../CommonBuildParameters.cmake)` delegates to `cmake/CommonBuildParameters.cmake`
6. `cmake/CommonBuildParameters.cmake` configures all 40+ find_package calls, adds `src/` subdirectory, optionally adds `test/` and `example/`
7. Install targets and package config generated

**State Management:**
- SDK state: Single `std::shared_ptr<sgns::GeniusNode>` module-level variable (`GeniusNodeInstance` in anonymous namespace at `src/GeniusSDK.cpp:176`)
- No explicit lock or thread-safety mechanism visible in the SDK layer; thread safety depends on SuperGenius internals

## Key Abstractions

**GeniusNode (SuperGenius):**
- Purpose: Core blockchain node — manages account, DHT, transaction manager, processing pipeline
- Examples: `sgns::GeniusNode::New()`, accessed via `GeniusNodeInstance->` calls
- Pattern: Factory method (static `New()`), shared ownership

**TokenID (SuperGenius):**
- Purpose: 32-byte token identifier (hex string in C API, `sgns::TokenID` in C++ internals)
- Examples: `sgns::TokenID::FromBytes()`, `GeniusTokenID` struct in C API
- Pattern: Value type

**ProcessingManager (SuperGenius):**
- Purpose: Image/processing job management, cost calculation
- Examples: `sgns::sgprocessing::ProcessingManager::Create()`
- Pattern: Factory method

**GeniusSDK C Types:**
- Purpose: Stable ABI for C interop — structs with fixed layout, no virtual functions
- Examples: `GeniusArray`, `GeniusAddress`, `GeniusTokenValue`, `GeniusTokenID`
- Pattern: POD structs, enum typedefs, opaque pointers via `uint8_t*`

## Entry Points

**Build Entry:**
- Location: `build/<Platform>/CMakeLists.txt` (5 files: OSX, Linux, Windows, iOS, Android)
- Triggers: `cmake ..` from `build/<Platform>/<BuildType>/`
- Responsibilities: Set toolchain, define project, include common build configuration

**Code Entry (Library):**
- Location: `src/GeniusSDK.h` — all public functions
- Triggers: External caller linking GeniusSDK static or shared library
- Responsibilities: Initialize/Shutdown node, query balances, mint/transfer tokens, submit processing jobs, check transaction status

**Code Entry (Examples):**
- Location: `example/SDKExample.cpp`, `example/SDKIdleExample.cpp`, `example/SDKExampleCredentials.cpp`
- Triggers: Executable run
- Responsibilities: Demonstrate SDK initialization and usage patterns

**Code Entry (Tests):**
- Location: `test/CMakeLists.txt` — references test source files
- Triggers: CTest
- Responsibilities: Unit tests for transaction handling and SDK functionality

## Architectural Constraints

- **Single-file SDK:** All implementation lives in one file (`src/GeniusSDK.cpp`, ~22K lines). New features must be added to this file or new files must be explicitly added to `src/CMakeLists.txt`.
- **C ABI boundary:** All public functions use C linkage (`extern "C"`). No C++ types, exceptions, or templates may cross the API boundary.
- **Threading:** No explicit threading model enforced at the SDK layer. The SuperGenius node manages its own threading internally.
- **Global state:** One module-level singleton `GeniusNodeInstance` (`std::shared_ptr<sgns::GeniusNode>`) in `src/GeniusSDK.cpp:176`. Must be guarded if concurrent init/shutdown is possible.
- **Circular imports:** Not detected. The SDK is a leaf consumer of SuperGenius.
- **No shared_ptr in public API:** C types only; `shared_ptr` is internal implementation detail.
- **Static libs preferred:** `Boost_USE_STATIC_LIBS ON`, `OPENSSL_USE_STATIC_LIBS ON`, `BUILD_SHARED_LIBS OFF`.

## Anti-Patterns

### Monolithic Implementation File

**What happens:** All SDK logic (~22,000 lines) lives in a single `src/GeniusSDK.cpp` file, mixing initialization, token operations, processing, JSON parsing, and error handling.
**Why it's wrong:** Makes the codebase hard to navigate, test in isolation, or parallel-develop. A change to token logic risks merge conflicts with unrelated processing changes.
**Do this instead:** When adding significant new functionality (e.g., new token types, new processing pipelines), create separate implementation files under `src/` and add them to `src/CMakeLists.txt`. The public header can remain a single `GeniusSDK.h` for API stability.

### Global Mutable Singleton

**What happens:** `GeniusNodeInstance` is a module-level `std::shared_ptr<sgns::GeniusNode>` at `src/GeniusSDK.cpp:176`. All functions access it directly.
**Why it's wrong:** No lifecycle guarantees — shutdown may race with in-flight operations. Test isolation is impossible without resetting global state.
**Do this instead:** Consider an opaque handle pattern (`GeniusSDKHandle`) returned by init and passed to all operations. This isolates state and enables multi-instance usage.

### Duplicated addtest() functions

**What happens:** The `addtest()` function is defined in both `cmake/functions.cmake` and `build/cmake/functions.cmake` with slightly different implementations. Both include GTest targets but the cmake version also links gmock.
**Why it's wrong:** Confusion about which version is used where; maintenance burden of keeping both in sync.
**Do this instead:** Consolidate to a single definition. The version in `cmake/functions.cmake` is more complete (includes gmock, FORCE_MULTILE support).

## Error Handling

**Strategy:** Error-code return values at the C API boundary; `outcome::result<T, E>` internally.

**Patterns:**
- Public C functions return `GeniusNodeReturnValue_t` (int32 enum: `GENIUS_NODE_RET_OK = 0`, then specific error codes)
- Internal functions use `outcome::result<T, Error>` with `outcome::success()` / `outcome::failure()`
- Custom exception class `JsonError : public boost::exception` for JSON parsing errors
- Initialization functions return `const char*` (null on failure)
- Balance functions return `uint64_t` (cannot indicate error); caller must check init state first

## Cross-Cutting Concerns

**Logging:** spdlog (via SuperGenius); SDK itself does not do its own spdlog logging. Compile definition `SPDLOG_FMT_EXTERNAL` in `cmake/CommonBuildParameters.cmake:146`.
**Validation:** Token ID validation (hex format, 64 hex chars) in `ParseTokenID()`. JSON validation via RapidJSON in processing paths.
**Authentication:** Credentials-based (`GeniusSDKInitWithCredentials`), private-key-based (`GeniusSDKInitWithKey`), and dev-config-based (`GeniusSDKInitWithKeyAndDevConfig`) initialization paths. Credentials handled by SuperGenius internally.

---

*Architecture analysis: 2026-06-08*
