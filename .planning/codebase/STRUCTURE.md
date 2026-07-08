---
last_mapped_commit: 35507476cbaa482c1c9a6e3582f8475a9f39304c
refreshed: 2026-07-03
---

# Codebase Structure

**Analysis Date:** 2026-07-03

## Directory Layout

```
GeniusSDK/
├── src/                          # SDK source (public API + implementation)
│   ├── GeniusSDK.h               # Public C ABI header (~50 functions, all types)
│   ├── GeniusSDK.cpp             # Implementation (935 lines — facade to sgns::GeniusNode)
│   └── CMakeLists.txt            # Build: static lib, shared lib, macOS bundle, iOS framework
│
├── example/                      # Example consumer applications
│   ├── SDKExample.cpp            # Interactive menu-driven demo (710 lines)
│   ├── SDKIdleExample.cpp        # Minimal init + idle loop (25 lines)
│   ├── SDKExampleCredentials.cpp # (stub — not compiled)
│   ├── dev_config.json           # Example dev config (Address, Cut, TokenValue, TokenID)
│   └── CMakeLists.txt            # Build examples linked whole-archive against GeniusSDK static lib
│
├── test/                         # Unit tests (sources not committed — CMakeLists only)
│   └── CMakeLists.txt            # Registers 3 tests: TransactionDataTest, TransactionBlocksTest, GeniusSDKTest
│
├── services/                     # Headless daemon and systemd unit files
│   ├── service.cpp               # CLI server: parses args, GeniusSDKInit, loops forever
│   ├── CMakeLists.txt            # Builds GeniusSDKService executable + installs .service files
│   ├── genius-archive-node.service # systemd unit for archive node
│   ├── genius-full-node.service  # systemd unit for full node (with processing)
│   └── genius-job-poster.service # DEPRECATED systemd unit (legacy job poster)
│
├── cmake/                        # Project CMake modules
│   ├── CommonBuildParameters.cmake  # All dependency find_package() calls and thirdparty paths (501 lines)
│   ├── CompilationFlags.cmake       # C++ compiler warnings/errors configuration
│   ├── functions.cmake              # addtest(), geniussdk_install(), disable_clang_tidy(), add_flag()
│   ├── config.cmake.in              # Template for generated GeniusSDKConfig.cmake
│   ├── GeniusSDK_Info_ios.plist     # iOS framework Info.plist
│   └── GeniusSDK_Info_Mac.plist     # macOS bundle Info.plist
│
├── build/                        # Git submodule → GeniusVentures/cmaketemplate
│   ├── CMakeLists.txt            # (top-level build entry point — NOT present)
│   ├── README.md                 # Build instructions for all platforms
│   ├── Android/CMakeLists.txt    # Android NDK cross-compile entry
│   ├── iOS/CMakeLists.txt        # iOS cross-compile entry
│   ├── Linux/CMakeLists.txt      # Linux native build entry (x86_64, aarch64)
│   ├── OSX/CMakeLists.txt        # macOS native build entry (universal binary)
│   ├── Windows/CMakeLists.txt    # Windows MSVC build entry
│   ├── CommonCompilerOptions.cmake  # C++17 standard, dependency auto-download, compiler config
│   ├── CommonBuildParameters.cmake.example  # Example/override for CommonBuildParameters.cmake
│   ├── CompilationFlags.cmake       # Delegates to cmake/CompilationFlags.cmake
│   ├── apple.toolchain.cmake        # Apple platform toolchain (macOS + iOS, from ios-cmake)
│   ├── .gitignore                   # Git ignore for build directory
│   └── cmake/                       # Build infrastructure
│       ├── functions.cmake          # TARGET_LINK_LIBRARIES_WHOLE_ARCHIVE, add_proto_library, compile_proto_to_cpp, get_third_party_dir
│       ├── definition.cmake         # CMake definitions
│       ├── install.cmake            # Install helpers
│       ├── print.cmake              # Message helpers
│       ├── compile_option_by_platform/  # Per-OS compiler flags
│       └── toolchain/               # Toolchain files
│
├── .github/workflows/            # CI/CD
│   ├── cmake.yml                 # Main CI: push/PR on develop/main triggers matrix build (593 lines)
│   └── build-release-tag.yml     # Tag-based release build with override deps (518 lines)
│
├── .clang-format                 # Microsoft-based, 120 cols, Allman braces, C++17
├── .clang-tidy                   # bugprone, cert, modernize, performance, readability checks
├── .clangd                       # Language server config
├── .gitignore                    # Compiled objects, IDEs, CMake artifacts, build outputs, .env
├── .gitmodules                   # Submodule: build → ../cmaketemplate
├── Readme.md                     # Build instructions, folder structure diagram, cmake --install
├── AGENTS.md                     # Agent instructions (ponytail/lazy mode)
└── AgentDocs/CLAUDE.md           # Comprehensive C++ coding standards, testing practices (298 lines)
```

## Directory Purposes

**src/:**
- Purpose: The SDK itself — public header and single implementation file
- Contains: `GeniusSDK.h` (C ABI declarations), `GeniusSDK.cpp` (facade implementation), `CMakeLists.txt` (4 build targets)
- Key files: `GeniusSDK.h` (API contract), `GeniusSDK.cpp` (all business logic)
- Note: The source is intentionally flat — no subdirectories, no modular files. All functionality channels through `sgns::GeniusNode`.

**example/:**
- Purpose: Demonstrate SDK usage for game developers integrating the library
- Contains: Two C++ consumer apps (`SDKExample` interactive, `SDKIdleExample` minimal), example dev_config
- Key files: `SDKExample.cpp` (full integration demo), `dev_config.json` (required config template)
- Build: Linked whole-archive against GeniusSDK static lib

**test/:**
- Purpose: Unit test definitions (test source files reside in SuperGenius repo, not committed here)
- Contains: `CMakeLists.txt` only (registers 3 test targets: `TransactionDataTest`, `TransactionBlocksTest`, `GeniusSDKTest`)
- Note: Test source files (`.cpp`) were not found in this repo — they likely live in the SuperGenius sibling project or are generated/templated elsewhere

**services/:**
- Purpose: Headless daemon for running GeniusSDK as a long-running node process on Linux servers
- Contains: One C++ CLI program (`service.cpp`), three systemd unit files
- Key files: `service.cpp` (minimal: init + infinite loop), `genius-full-node.service` (systemd unit with processing enabled)

**cmake/:**
- Purpose: Project-level CMake configuration shared across all platform builds
- Contains: Dependency declarations (`CommonBuildParameters.cmake`), compiler flags, install helpers, framework plists
- Key files: `CommonBuildParameters.cmake` (501 lines — the entire dependency graph), `CompilationFlags.cmake` (warning/error flags)

**build/:**
- Purpose: Platform-specific CMake entry points and toolchains (git submodule from `GeniusVentures/cmaketemplate`)
- Contains: Per-platform `CMakeLists.txt`, common compiler options, toolchain files, build helper functions
- Generated: No — source-controlled git submodule
- Committed: Yes — referenced by `.gitmodules`

**.github/workflows/:**
- Purpose: Automated CI/CD pipeline
- Contains: Matrix builds across 5 platforms × (Debug+Release), self-hosted runners, artifact publishing to GitHub releases
- Key files: `cmake.yml` (push/PR CI), `build-release-tag.yml` (manual tag-driven release)

## Key File Locations

**Entry Points:**
- `src/GeniusSDK.h`: Public API entry point — all 50+ `extern "C"` functions declared here
- `services/service.cpp`: Headless daemon entry point — `main()` that calls `GeniusSDKInit()` and loops
- `example/SDKExample.cpp`: Demo app entry point — menu-driven interactive main
- `example/SDKIdleExample.cpp`: Minimal demo entry point — init hardcoded key and idle

**Configuration:**
- `example/dev_config.json`: Required runtime config template (Address, Cut, TokenValue, TokenID)
- `.clang-format`: Code formatting rules (Microsoft-based, 120 cols, Allman braces)
- `.clang-tidy`: Static analysis rules (bugprone, cert, modernize, performance, readability, m_ prefix for members)
- `.clangd`: Language server settings
- `.gitignore`: Excludes build artifacts, IDE files, compiled objects, .env files

**Core Logic:**
- `src/GeniusSDK.cpp`: All 50+ function implementations, JSON config parsing, C↔C++ type conversions
- `src/CMakeLists.txt`: Build targets: GeniusSDK (static), GeniusSDK_shared (shared), GeniusSDK_bundle (macOS MODULE), GeniusSDK_framework (iOS)

**Build Infrastructure:**
- `build/OSX/CMakeLists.txt`: macOS CMake entry
- `build/Linux/CMakeLists.txt`: Linux CMake entry
- `build/Android/CMakeLists.txt`: Android NDK CMake entry
- `build/iOS/CMakeLists.txt`: iOS CMake entry
- `build/Windows/CMakeLists.txt`: Windows CMake entry
- `build/CommonCompilerOptions.cmake`: C++17 standard, auto-download logic, thirdparty discovery
- `cmake/CommonBuildParameters.cmake`: All ~40 dependency `find_package()` calls

**Testing:**
- `test/CMakeLists.txt`: Test registration (3 tests registered, source files not in this repo)

**Agent Instructions:**
- `AGENTS.md`: High-level agent behavior (ponytail/lazy mode)
- `AgentDocs/CLAUDE.md`: Comprehensive C++ coding standards (298 lines — Effective C++, Modern Effective C++, API Design principles, testing patterns)

## Naming Conventions

**Files:**
- PascalCase for SDK and example source files: `GeniusSDK.cpp`, `SDKExample.cpp`, `SDKIdleExample.cpp`
- snake_case for service files: `service.cpp`
- kebab-case for systemd unit files: `genius-archive-node.service`, `genius-full-node.service`
- PascalCase or snake_case for CMake modules: `CommonBuildParameters.cmake`, `CompilationFlags.cmake`, `functions.cmake`

**Directories:**
- Platform directories in `build/` match OS names: `Android`, `iOS`, `OSX`, `Linux`, `Windows`
- Lowercase for other directories: `src`, `example`, `test`, `services`, `cmake`

**Functions (public C API):**
- PascalCase prefixed with `GeniusSDK`: `GeniusSDKInit`, `GeniusSDKShutdown`, `GeniusSDKGetBalance`
- Verb-noun pattern: `GeniusSDKGetAddress`, `GeniusSDKProcess`, `GeniusSDKMint`, `GeniusSDKTransfer`

**Types:**
- PascalCase with suffix hints: `GeniusArray`, `GeniusMatrix`, `GeniusAddress`, `GeniusTokenValue`, `GeniusTokenID`
- Enum values: `UPPER_CASE` with `GENIUS_` prefix: `GENIUS_NODE_RET_OK`, `GENIUS_NODE_ERROR_NOT_INITIALIZED`
- Typedef aliases: PascalCase with `_t` suffix: `PayAmount_t`, `GeniusNodeReturnValue_t`, `GeniusNodeState_t`

**Constants/macros:**
- `UPPER_CASE` for defines: `GENIUS_SDK_ADDRESS_SIZE`, `GENIUS_SDK_MAX_MNEMONIC_SIZE`, `GNUS_VISIBILITY_DEFAULT`

**Member variables (C++ — per .clang-tidy):**
- `m_` prefix: class members use `m_memberName` convention

## Where to Add New Code

**New public API function:**
1. Declare in `src/GeniusSDK.h` inside the `GNUS_EXPORT_BEGIN`/`GNUS_EXPORT_END` block — use `GNUS_VISIBILITY_DEFAULT` and document with Doxygen `@brief`/`@param`/`@return`
2. Implement in `src/GeniusSDK.cpp` — delegate to `GeniusNodeInstance->SomeMethod(...)`, handle null-guard, convert types, return error code
3. If the function allocates memory for the caller, document that the caller must free with `GeniusSDKFree()`

**New C-compatible type:**
1. Define POD struct in `src/GeniusSDK.h` before the `GNUS_EXPORT_BEGIN` block
2. Keep all types stack-allocatable (no pointers to heap unless caller manages lifecycle)
3. Prefix with `Genius` to avoid namespace collisions

**New example:**
1. Create `example/NewExample.cpp` — include `GeniusSDK.h`, call init, exercise API
2. Add `add_executable(NewExample NewExample.cpp)` + `TARGET_LINK_LIBRARIES_WHOLE_ARCHIVE(NewExample GeniusSDK)` to `example/CMakeLists.txt`

**New test:**
1. Add test source file in `test/` (not currently present — tests may live in SuperGenius repo)
2. Register in `test/CMakeLists.txt` using the `addtest(TestName source.cpp)` macro
3. Link against `GeniusSDK` static lib

**New platform target:**
1. Create `build/NewPlatform/CMakeLists.txt` following the pattern of existing platforms
2. Add new matrix entries in `.github/workflows/cmake.yml` and `.github/workflows/build-release-tag.yml`

**New dependency:**
1. Add `find_package(NewDep CONFIG REQUIRED)` in `cmake/CommonBuildParameters.cmake`
2. Set `NewDep_DIR` and `NewDep_INCLUDE_DIR` to `${THIRDPARTY_BUILD_DIR}/newdep/...`
3. Ensure the dependency is pre-built and available in the `thirdparty/` sibling repo

**New CMake module:**
1. Add `.cmake` file in `cmake/` for project-level config, or `build/cmake/` for build infrastructure
2. Include from the appropriate `CMakeLists.txt` or `CommonBuildParameters.cmake`

## Special Directories

**build/:**
- Purpose: Cross-platform CMake scaffold and build output directory
- Generated: Output subdirectories (Debug, Release) are generated at build time
- Committed: The CMake files are committed (git submodule); build outputs are gitignored

**AgentDocs/:**
- Purpose: AI agent instructions and coding standards reference
- Generated: No — manually curated
- Committed: Yes — contains `CLAUDE.md` with 298 lines of coding rules

**.planning/:**
- Purpose: GSD planning artifacts (codebase maps, phase plans, todos)
- Generated: Yes — by GSD commands
- Committed: Yes — tracked for team visibility

**.github/:**
- Purpose: CI/CD workflows
- Generated: No — manually written
- Committed: Yes

---

*Structure analysis: 2026-07-03*
