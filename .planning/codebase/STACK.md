# Technology Stack

**Analysis Date:** 2026-06-08

## Languages

**Primary:**
- C++17 - All SDK source code (`src/GeniusSDK.cpp`, `src/GeniusSDK.h`)
- C (extern "C" interface) - Public API surface in `src/GeniusSDK.h`

**Secondary:**
- CMake 3.22+ - Build system (all `CMakeLists.txt` files, cmake module files)
- Objective-C/C++ - Apple platform toolchain support (`build/apple.toolchain.cmake`)

## Runtime

**Standard:**
- C++17 (ISO C++17) enforced via `CMAKE_CXX_STANDARD 17` with `CMAKE_CXX_STANDARD_REQUIRED ON` and `CMAKE_CXX_EXTENSIONS OFF` in `build/CommonCompilerOptions.cmake`
- C17 via `CMAKE_C_STANDARD 17` with `CMAKE_C_EXTENSIONS OFF`

**Compiler Requirements:**
- Apple: AppleClang via Xcode (macOS deployment target 13.0, iOS 15.0)
- Linux: GCC or Clang
- Windows: MSVC (Visual Studio 17 2022)
- Android: NDK r25b, Clang toolchain, API level 28

## Build System

**Generator:**
- Ninja (default/preferred for macOS/Linux)
- Visual Studio 17 2022 (Windows)
- Xcode (Apple combined/universal builds)

**Build Config:**
- CMake minimum 3.22
- CMake module files in two locations:
  - `build/cmake/` — build infrastructure (functions, definitions, install, print, toolchain)
  - `cmake/` — project-level (CommonBuildParameters, CompilationFlags, functions, config)
- Package generation: `GeniusSDKConfig.cmake` generated from `cmake/config.cmake.in`
- Version file: `GeniusSDKConfigVersion.cmake` with `AnyNewerVersion` compatibility

## Package Manager

- No package manager used at build time. All dependencies are pre-built in a separate `thirdparty/` repository.
- The `build/` directory is a git submodule pointing to `GeniusVentures/cmaketemplate`.
- Thirdparty discovery: `get_third_party_dir()` function in `build/cmake/functions.cmake` walks up the directory tree to locate the `thirdparty/` sibling directory, verified by matching the git remote `GeniusVentures/thirdparty`.
- If thirdparty is not present locally, `build/CommonCompilerOptions.cmake` auto-downloads it from GitHub releases.

## Frameworks

**Core:**
- SuperGenius (sibling project `../SuperGenius`) - Genesis node, blockchain, transaction management, processing services
  - Included via `find_package(SuperGenius CONFIG REQUIRED)` from `cmake/CommonBuildParameters.cmake`
  - Target namespace: `sgns::`

**Testing:**
- Google Test (GTest) - Test runner and assertions
  - Configured in `cmake/CommonBuildParameters.cmake` via `find_package(GTest CONFIG REQUIRED)`
  - Test helper: `addtest()` function in `cmake/functions.cmake` (also duplicated in `build/cmake/functions.cmake`)
  - Output: xUnit XML at `${CMAKE_BINARY_DIR}/xunit/`
  - Enabled by `TESTING` option (default `OFF` in project, default `ON` in `CommonCompilerOptions.cmake`)

**Dependency Injection:**
- Boost.DI (header-only compile-time DI framework) - Configured but no SDK source uses it directly; used by SuperGenius internals

**Zero-Knowledge Proofs:**
- zkLLVM + crypto3 (algebra, block, blueprint, codec, math, multiprecision, pkpad, pubkey, random, zk) - Interface targets
- LLVM - Required for zkLLVM
- Both auto-downloaded from GitHub releases if not present locally

## Key Dependencies

The full dependency map is defined in `cmake/CommonBuildParameters.cmake`. All are found via `CONFIG REQUIRED` from `${THIRDPARTY_BUILD_DIR}`.

### Core Libraries

| Package | Version | Purpose | Config Path Pattern |
|---------|---------|---------|-------------------|
| Boost | 1.85.0 | Multi-purpose (container, date_time, filesystem, json, random, regex, system, thread, log, log_setup, program_options, unit_test_framework) | `${_THIRDPARTY_BUILD_DIR}/boost/build/lib/cmake/Boost-1.85.0` |
| Protobuf | (thirdparty) | Serialization, protoc code generation | `${THIRDPARTY_BUILD_DIR}/protobuf/lib/cmake/protobuf` |
| OpenSSL | (thirdparty) | Cryptography, TLS | `${THIRDPARTY_BUILD_DIR}/openssl/build` |
| libsecp256k1 | (thirdparty) | Elliptic curve cryptography (secp256k1) | `${THIRDPARTY_BUILD_DIR}/libsecp256k1/lib/cmake/libsecp256k1` |
| ed25519 | (thirdparty) | Ed25519 signatures | `${THIRDPARTY_BUILD_DIR}/ed25519/lib/cmake/ed25519` |

### Logging

| Package | Purpose |
|---------|---------|
| fmt | Formatting backend (SPDLOG_FMT_EXTERNAL defined to use external fmt) |
| spdlog | Structured logging |
| soralog | Custom logging layer |

### Storage

| Package | Purpose |
|---------|---------|
| RocksDB | Key-value store | 
| Snappy | Compression for RocksDB |
| SQLite3 | Embedded relational database |
| SQLiteModernCpp | Modern C++ SQLite wrapper |

### P2P / Networking

| Package | Purpose |
|---------|---------|
| libp2p | Peer-to-peer networking |
| ipfs-lite-cpp | IPFS storage layer |
| ipfs-pubsub | IPFS pub/sub |
| ipfs-bitswap-cpp | IPFS bitswap |
| c-ares | Async DNS resolution |
| libssh2 | SSH2 protocol |
| AsyncIOManager | Async I/O management |
| gnus_upnp | UPnP/NAT traversal |

### Data Formats & Parsing

| Package | Purpose |
|---------|---------|
| RapidJSON | JSON parsing (used directly in GeniusSDK.cpp) |
| nlohmann_json | JSON for modern C++ |
| yaml-cpp | YAML parsing |
| protobuf + absl + utf8_range | Protocol Buffers serialization |

### Blockchain / Wallet

| Package | Purpose |
|---------|---------|
| TrustWallet Core (wallet-core) | Multi-currency wallet implementation |
| TrezorCrypto | Low-level crypto for wallet |

### Hashing & Data Structures

| Package | Purpose |
|---------|---------|
| xxHash | Fast non-cryptographic hash |
| tsl_hat_trie | HAT-trie (cache-friendly trie) |
| Microsoft.GSL | Guidelines Support Library (span, not_null, etc.) |
| stb | Single-file public domain libraries (image I/O) |

### UI / Graphics

| Package | Purpose |
|---------|---------|
| Vulkan / MoltenVK | Graphics API (Apple platforms only, via `.xcframework`) |
| MNN | Neural network inference |

## Configuration

**Environment Variables (build-time):**
- `ANDROID_NDK` / `ANDROID_NDK_HOME` - Path to Android NDK (Android builds)
- `VULKAN_SDK` - Path to Vulkan SDK (fallback if MoltenVK not configured)

**CMake Cache Variables (user-configurable):**
- `THIRDPARTY_DIR` - Path to thirdparty repository root
- `SUPERGENIUS_DIR` - Path to SuperGenius project root
- `GENIUS_DEPENDENCY_BRANCH` - Branch name for downloading dependencies from GitHub releases
- `BRANCH_IS_TAG` - Set ON when GENIUS_DEPENDENCY_BRANCH points to a tagged release
- `SANITIZE_CODE` - Sanitizer type (address, thread, undefined, etc.)
- `SGNS_STACKTRACE_BACKTRACE` - Enable Boost stacktrace with backtrace (POSIX)

**Build Options:**
- `BUILD_SHARED_LIBS` - Build shared libraries (`OFF` by default)
- `TESTING` - Build tests (`OFF` by default in project, `ON` in CommonCompilerOptions.cmake)
- `BUILD_EXAMPLES` - Build examples (`OFF` by default in project, `ON` in CommonCompilerOptions.cmake)

**Build Configs:**
- `.clang-format` - Formatting rules (BasedOnStyle: Microsoft, 120 cols, Allman braces, C++17)
- `.clang-tidy` - Static analysis checks (bugprone, cert, modernize, performance, readability)
- `.clangd` - Language server config

## Platform Requirements

**Development:**
- macOS: Xcode with command-line tools, Ninja (recommended)
- Linux: GCC/Clang, CMake 3.22+, Ninja (recommended)
- Windows: Visual Studio 17 2022, CMake 3.22+
- Android: NDK r25b, host CMake 3.22+

**Production:**
- Deployment targets: macOS 13.0+, iOS 15.0+, Android API 28+
- All platforms build via `cmake --build .` from `build/<Platform>/<BuildType>/`
- Install via `cmake --install .` from the build directory

**Build Output:**
- Static library: `libGeniusSDK.a`
- Shared library: `libGeniusSDK_shared.{dylib|so|dll}`
- macOS bundle: `GeniusSDK.bundle` (MODULE type)
- iOS framework: `GeniusSDK.framework`
- Export header: `GeniusSDKExport.h` (generated)
- Package config: `GeniusSDKConfig.cmake`, `GeniusSDKConfigVersion.cmake`

---

*Stack analysis: 2026-06-08*
