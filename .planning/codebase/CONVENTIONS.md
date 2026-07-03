---
last_mapped_commit: 35507476cbaa482c1c9a6e3582f8475a9f39304c
---

# Coding Conventions

**Analysis Date:** 2026-07-03

## Language & Standard

**Primary language:** C++17 (`g++`/`clang++`), with a C-compatible public API surface.
- `CMAKE_CXX_STANDARD 17` / `CMAKE_C_STANDARD 17` in `build/CommonCompilerOptions.cmake:22-24`
- `.clang-format` declares `Standard: c++17` (`.clang-format:46`)
- The public API header `src/GeniusSDK.h` wraps everything in `extern "C"` via `GNUS_EXPORT_BEGIN`/`GNUS_EXPORT_END` macros, uses C-compatible types (structs, enums, `uint64_t`), and avoids C++ features that break C linkage.

## Code Formatting

**Tool:** clang-format, configured via `.clang-format`.

**Base style:** `BasedOnStyle: Microsoft` with these overrides:

| Setting | Value | File Line |
|---------|-------|-----------|
| Column limit | 120 | `.clang-format:25` |
| Indentation | Spaces (Microsoft default: 4) | Base style |
| Access modifier offset | -4 | `.clang-format:3` |
| Break before braces | Custom — `AfterCaseLabel: true`, `BeforeLambdaBody: true` | `.clang-format:19-20` |
| Bin pack arguments/params | false (each on its own line) | `.clang-format:21-22` |
| Align consecutive assignments | Enabled with compound alignment | `.clang-format:6-8` |
| Align consecutive declarations | Enabled | `.clang-format:11-12` |
| Insert braces | Always (no unbraced single-line bodies) | `.clang-format:29` |
| Insert trailing commas | Wrapped params only | `.clang-format:31` |
| Namespace indentation | All | `.clang-format:33` |
| Sort includes | Never (manual order preserved) | `.clang-format:41` |
| Spaces in parens | Custom — in conditionals and other contexts | `.clang-format:42-45` |
| Remove parentheses | Return statements only | `.clang-format:38` |
| Remove semicolon | Yes | `.clang-format:39` |
| Separate definition blocks | Always | `.clang-format:40` |
| Fix namespace comments | false | `.clang-format:27` |
| Break string literals | false | `.clang-format:24` |
| Constructor initializers | Break after colon | `.clang-format:23` |
| Pack constructor initializers | Next line | `.clang-format:34` |
| Always break template decls | true | `.clang-format:17` |
| Reflow comments | false | `.clang-format:37` |

## Static Analysis (clang-tidy)

**Tool:** clang-tidy, configured via `.clang-tidy`.

**Enabled check suites:**
- `boost-*` — all Boost checks
- `bugprone-*` — minus `easily-swappable-parameters`, `reserved-identifier`
- `cert-*` — minus `dcl51-cpp`, `err58-cpp`
- `concurrency-*` — all concurrency checks
- `cppcoreguidelines-*` — minus `avoid-do-while`, `avoid-magic-numbers`, `non-private-member-variables-in-classes`, `pro-bounds-pointer-arithmetic`, `pro-type-reinterpret-cast`, `special-member-functions`
- `google-*` — selected Google checks: `build-explicit-make-pair`, `build-namespaces`, `default-arguments`, `global-names-in-headers`, `readability-avoid-underscore-in-googletest-name`, `readability-casting`, `runtime-int`, `runtime-operator`, `upgrade-googletest-case`
- `hicpp-multiway-paths-covered`
- `misc-*` — minus `include-cleaner`, `non-private-member-variables-in-classes`
- `modernize-*` — minus `use-trailing-return-type`
- `performance-*`
- `portability-*`
- `readability-*` — minus `identifier-length`, `magic-numbers`, `math-missing-parentheses`, `named-parameter`

**Naming conventions enforced by clang-tidy (`readability-identifier-naming`):**
- Enum cases: `CamelCase` (`.clang-tidy:62`)
- Enum constants: `UPPER_CASE` (`.clang-tidy:63`)
- Type aliases (using/typedef): `CamelCase` (`.clang-tidy:65-66`)
- Class member prefix: `m_` (`.clang-tidy:68`)

**NOLINT exceptions in source:**
- `src/GeniusSDK.h:17` — Disables `modernize-use-using`, `modernize-deprecated-headers`, `cppcoreguidelines-avoid-c-arrays`, `performance-enum-size` for the entire header. This is intentional because the header targets C interop (uses `typedef` and C arrays).

**Format style:** `FormatStyle: file` — clang-tidy uses `.clang-format` for fix-it formatting.

**Target-specific exceptions:** Tests are exempted from clang-tidy via `disable_clang_tidy()` in `cmake/functions.cmake:1-6`, called by `addtest()` at line 26.

## Compiler Flags

**Enabled warnings** (`cmake/CompilationFlags.cmake:9-21`):
- `-Wall`, `-Wextra`, `-Woverloaded-virtual`, `-Wformat=2`, `-Wmisleading-indentation`, `-Wduplicated-cond`, `-Wduplicated-branches`, `-Wnull-dereference`, `-Wsign-compare`, `-Wtype-limits`, `-Wnon-virtual-dtor`

**Suppressed warnings** (`cmake/CompilationFlags.cmake:24-38`):
- `-Wno-unused-command-line-argument`, `-Wno-unused-variable`, `-Wno-double-promotion`, `-Wno-unused-parameter`, `-Wno-unused-function`, `-Wno-format-nonliteral`, `-Wno-gnu-zero-variadic-macro-arguments`, `-Wno-unused-result`, `-Wno-pessimizing-move`, `-Wno-unused-but-set-variable`, `-Wno-macro-redefined`, `-Wno-deprecated-copy-with-user-provided-copy`

**Promoted to errors** (`cmake/CompilationFlags.cmake:40-44`):
- `-Werror=unused-lambda-capture`, `-Werror=sign-compare`, `-Werror=type-limits`

**Sanitizer support** — opt-in via `-DSANITIZE_CODE=<sanitizer>` (`build/CommonCompilerOptions.cmake:28-42`).

## Naming Patterns

**Files:**
- PascalCase for source files: `GeniusSDK.cpp`, `GeniusSDK.h`, `SDKExample.cpp`
- kebab-case for CMake/platform directories: `build/OSX/`, `cmake/functions.cmake`, `cmake/CommonBuildParameters.cmake`
- UPPER_CASE for config/CI files: `CMakeLists.txt`, `.clang-format`, `.clang-tidy`
- snake_case for service/example reference files: `dev_config.json`, `genius-full-node.service`

**Functions (public API — C-compatible):**
- PascalCase, prefixed with `GeniusSDK`: `GeniusSDKInit()`, `GeniusSDKGetBalance()`, `GeniusSDKTransfer()`, `GeniusSDKShutdown()`
- Location: `src/GeniusSDK.h` (declarations), `src/GeniusSDK.cpp` (definitions)

**Functions (internal/anonymous namespace):**
- PascalCase: `ParseTokenID()`, `ReadDevConfigFromJSON()`, `SDKInitHelper()`, `matrix_from_vector_of_vector()`
- One exception: `matrix_from_buffer()` and `matrix_from_vector_of_vector()` use snake_case (likely legacy). Location: `src/GeniusSDK.cpp:150-180`

**Variables:**
- snake_case for locals and parameters: `base_path`, `eth_private_key`, `load_config_ret`
- PascalCase for global/static/namespace-level: `GeniusNodeInstance` (anonymous namespace at `src/GeniusSDK.cpp:182`)

**Types (structs, enums, typedefs):**
- PascalCase: `GeniusArray`, `GeniusMatrix`, `GeniusAddress`, `GeniusTokenValue`, `GeniusTokenID`
- Standard C/C++ typedefs use `_t` suffix: `GeniusNodeReturnValue_t`, `GeniusNodeState_t`, `GeniusTransactionManagerState_t`, `PayAmount_t`
  - Note: `JsonData_t` is a `char[2048]` typedef (`src/GeniusSDK.h:85`)
- Enum values: `UPPER_CASE` with hierarchical prefix — `GENIUS_NODE_RET_OK`, `GENIUS_NODE_ERROR_NOT_INITIALIZED`, `GENIUS_TX_STATUS_CREATED`, `GENIUS_PR_STATUS_IDLE`

**Macros:**
- `UPPER_CASE`: `GNUS_EXPORT_BEGIN`, `GNUS_EXPORT_END`, `GNUS_VISIBILITY_DEFAULT`, `GENIUS_SDK_ADDRESS_SIZE`, `GENIUS_SDK_MAX_MNEMONIC_SIZE`, `SUPPRESS_OUTPUT`

**Class member prefix:** `m_` enforced by clang-tidy (`.clang-tidy:68`). This applies to internal `SuperGenius` classes used by the SDK, not the C API itself. The SDK's own header (`GeniusSDK.h`) uses only structs and typedefs (no classes with private members).

## Comments and Documentation

**Doxygen** is used for public API documentation:
- `@brief`, `@param[in]`, `@param[out]`, `@returns`, `@ref` tags throughout `src/GeniusSDK.h`
- File-level comments: `@file`, `@brief`, `@date`, `@author` (e.g., `src/GeniusSDK.cpp:1-6`, `example/SDKExample.cpp:1-7`)
- Inline comments use `///<` for trailing documentation of struct members and enums

**When to comment:**
- All public API functions are documented with Doxygen
- Internal helper functions use Doxygen-style comments (`example/SDKExample.cpp:596-601`, `src/GeniusSDK.cpp:89-90`)
- Complex logic includes inline explanations (e.g., `src/GeniusSDK.h:164-165` explains mnemonic size calculation)
- Magic numbers are explained in comments rather than suppressed (clang-tidy `-bugprone-avoid-magic-numbers` is disabled intentionally)

## Error Handling

**Public API pattern:** Functions return status codes via `GeniusNodeReturnValue_t` (an `int32_t` enum). The caller checks the return value.

```cpp
// src/GeniusSDK.cpp:299-320 — typical pattern using do { } while(0) for early exit
GeniusNodeReturnValue_t GeniusSDKProcess(const JsonData_t jsondata) {
    GeniusNodeReturnValue ret = GENIUS_NODE_ERROR_NOT_INITIALIZED;
    do {
        if (!GeniusNodeInstance) { break; }
        auto result = GeniusNodeInstance->ProcessImage(std::string{jsondata});
        if (!result.has_value()) {
            ret = GENIUS_NODE_ERROR_PROCESS_IMAGE;
            std::cerr << "Error processing image: " << result.error() << std::endl;
            break;
        }
        ret = GENIUS_NODE_RET_OK;
    } while (0);
    return ret;
}
```

**Key patterns:**
1. `do { ... } while (0)` block for early-exit-with-cleanup — used in `GeniusSDKProcess()`, `GeniusSDKMint()`, `GeniusSDKTransfer()`, `GeniusSDKTransferGNUS()`, `GeniusSDKPayDev()`, `GeniusSDKMintGNUS()`
2. `outcome::result<T, Error>` for internal operations — `ParseDevConfig()`, `ParseTokenID()`, `ReadDevConfigFromJSON()` return `outcome::result<DevConfig_st, JsonError>`
3. `std::optional` pattern for nullable returns: node pointer checks (`if (!GeniusNodeInstance)`) return default/error values
4. Null-termination safety: all string buffers are explicitly null-terminated: `tv.value[sizeof(tv.value) - 1] = '\0'` (`src/GeniusSDK.cpp:370, 394`)
5. Null pointer guards: `if (base_path == nullptr)` / `if (amount == nullptr)` / `if (dest == nullptr)` at function entry (`src/GeniusSDK.cpp:189-193, 256-260, 545-554`)
6. `static` local buffers for C-string return values (documented as not thread-safe: `src/GeniusSDK.cpp:378`)
7. Memory ownership is explicit: the caller must free results with `GeniusSDKFree()` or `GeniusSDKFreeTransactions()`

## Logging

**Framework:** spdlog (included as `#include <spdlog/spdlog.h>`, configured in `cmake/CommonBuildParameters.cmake:143-146`).
- Uses `SPDLOG_ERROR()` macro in internal code (`src/GeniusSDK.cpp:191, 252`) — not exposed in the C API header
- Also uses `std::cerr` for error output (`src/GeniusSDK.cpp:313`, `src/GeniusSDK.cpp:438`)
- `std::cout` for informational messages (`src/GeniusSDK.cpp:636`)
- Log config reload available at runtime via `GeniusSDKLoadLogConfig()` (`src/GeniusSDK.cpp:641-647`)

## Module Design

**Architecture:** Thin C wrapper over C++ internals. The `.cpp` file (`src/GeniusSDK.cpp`) includes headers from `account/`, `blockchain/`, `processing/`, `base/` and contains an anonymous namespace with internal helpers.

**Exports:**
- All public API functions are declared with `GNUS_VISIBILITY_DEFAULT` in the header
- Internal helpers live in anonymous namespace `namespace { ... }` (`src/GeniusSDK.cpp:35-214`)
- The only module-level state is `static std::shared_ptr<sgns::GeniusNode> GeniusNodeInstance` (`src/GeniusSDK.cpp:182`)

**CMake structure:**
- `src/CMakeLists.txt` builds both a static library (`libGeniusSDK.a`) and a shared library (`libGeniusSDK_shared.dylib/.dll/.so`)
- On Apple platforms, a bundle and framework targets are also generated
- Platform-specific debug symbol handling (dSYM on macOS, PDB on Windows, separate debug info on Linux) is in `src/CMakeLists.txt:29-72`
- Custom install function `geniussdk_install()` in `cmake/functions.cmake:58-68`

## Import/Include Organization

**Include order in implementation files** (from `src/GeniusSDK.cpp`):
1. Own header: `#include "GeniusSDK.h"`
2. Project headers (`account/`, `blockchain/`, `processing/`, `base/`)
3. Standard library (`<algorithm>`, `<cstdint>`, `<memory>`, `<string>`, `<cstring>`, `<fstream>`)
4. Third-party: Boost (`<boost/...>`), RapidJSON (`<rapidjson/...>`), spdlog

**Clang-format `SortIncludes: Never`** means manual include ordering is preserved and not auto-reordered.

**Header guard style:** Traditional `#ifndef` / `#define` / `#endif` pattern: `#ifndef _GENIUSSDK_H` / `#define _GENIUSSDK_H` (`src/GeniusSDK.h:19-20, 549`)

## Cross-Platform Patterns

- `#ifdef _WIN32` / `#else` blocks for platform-specific code (`src/GeniusSDK.h:37-41`, `example/SDKExample.cpp:8-15`)
- `#if defined(__cplusplus)` guard for C/C++ dual compilation (`src/GeniusSDK.h:26-35`)
- `GNUS_VISIBILITY_DEFAULT` macro abstracts `__declspec(dllexport)` (Windows) vs `__attribute__((visibility("default")))` (others) (`src/GeniusSDK.h:37-41`)
- `uint64_t` formatted with `PRId64` / `PRIu64` macros from `<inttypes.h>` for cross-platform portability (`example/SDKExample.cpp:363`)

## Function Design

**Size:** Most public SDK functions are short (<40 lines) and delegate to `GeniusNodeInstance`. The `.cpp` file is 935 lines but each function is self-contained.

**Parameter order:** Input parameters first, output parameters last. In `GeniusSDK.h`, pointer parameters use `const` for inputs, non-const for outputs. Boolean flags follow the "required then optional" convention.

**Return values:** The C API uses three patterns:
1. `GeniusNodeReturnValue_t` for operations that can fail (most functions)
2. Direct value return for simple getters (`uint64_t`, `double`, `GeniusAddress`)
3. Pointer return for heap-allocated strings (caller must free)

**Template usage:** Internal only — `SDKInitHelper()` is a function template in the anonymous namespace (`src/GeniusSDK.cpp:184-213`) that accepts a callable `Creator`. Not exposed in the C API.

---

*Convention analysis: 2026-07-03*
