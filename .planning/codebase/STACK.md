---
last_mapped_commit: 35507476cbaa482c1c9a6e3582f8475a9f39304c
---

# Technology Stack

**Analysis Date:** 2026-07-03

## Languages

**Primary:**
- C++17 - All source code (SDK, services, examples, tests)
  - Standard: `CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_STANDARD_REQUIRED ON` (`build/CommonCompilerOptions.cmake`)
  - Compiler families: Clang/AppleClang/GCC (POSIX), MSVC (Windows)
  - C standard: C17 (`CMAKE_C_STANDARD 17`)

**Secondary:**
- Rust - Android cross-compilation toolchain targets (`rustup target add` in CI: `aarch64-linux-android`, `armv7-linux-androideabi`)
- CMake (build configuration language) - All build scripts and dependency management

## Runtime

**Environment:**
- Native C++ binaries (no managed runtime)
- `libc++` or `libstdc++` depending on platform/compiler
- Clang is the preferred compiler (CI configurations all default to `/usr/bin/clang++` on Linux)

**Package Manager:**
- CMake `find_package` with CONFIG mode — dependencies resolved via pre-built artifacts from GitHub releases
- No traditional package manager (no vcpkg, Conan, or system package manager for SDK deps)
- Lockfile: Not applicable (dependencies are versioned via GitHub release tags)

## Frameworks

**Core:**
- Boost 1.85.0 - Primary general-purpose library (container, date_time, filesystem, json, random, regex, system, thread, log, log_setup, program_options, coroutine, context, unit_test_framework) (`cmake/CommonBuildParameters.cmake:2-4`, `cmake/CommonBuildParameters.cmake:202`)
- Boost.DI - Dependency injection framework
- zkLLVM / crypto3 - Zero-knowledge cryptography framework (algebra, block, blueprint, codec, math, multiprecision, pkpad, pubkey, random, zk) (`cmake/CommonBuildParameters.cmake:336-399`)
- LLVM - Compiler infrastructure (used via zkLLVM) (`cmake/CommonBuildParameters.cmake:398-399`)

**Networking & P2P:**
- libp2p - Peer-to-peer networking stack (`cmake/CommonBuildParameters.cmake:221-224`)
- c-ares - Async DNS resolution (`cmake/CommonBuildParameters.cmake:57-58`)
- jsonrpc-lean - Lightweight JSON-RPC framework (`cmake/CommonBuildParameters.cmake:260-261`)

**Storage:**
- RocksDB - Embedded key-value store for blockchain state (`cmake/CommonBuildParameters.cmake:126-128`)
- SQLite3 + SQLiteModernCpp - Local database storage (`cmake/CommonBuildParameters.cmake:204-215`)
- IPFS-lite-cpp + ipfs-pubsub + ipfs-bitswap-cpp - Decentralized content-addressed storage (`cmake/CommonBuildParameters.cmake:232-246`)

**Serialization & Data:**
- Protocol Buffers (protobuf) - Binary serialization for processing messages (`cmake/CommonBuildParameters.cmake:76-99`)
- RapidJSON - JSON parsing/writing (`src/GeniusSDK.cpp:26-29`, `cmake/CommonBuildParameters.cmake:253-257`)
- nlohmann/json - Alternative JSON library (`cmake/CommonBuildParameters.cmake:401-402`)
- YAML-cpp - YAML configuration parsing (`cmake/CommonBuildParameters.cmake:61-63`)

**Cryptography & Identity:**
- OpenSSL - TLS/crypto primitives (`cmake/CommonBuildParameters.cmake:113-119`)
- libsecp256k1 - Elliptic curve cryptography (Bitcoin/Ethereum key handling) (`cmake/CommonBuildParameters.cmake:263-267`)
- ed25519 - Ed25519 signature scheme (`cmake/CommonBuildParameters.cmake:248-251`)
- TrustWalletCore - Ethereum wallet functionality (TrezorCrypto, wallet_core_rs, TrustWalletCore) (`cmake/CommonBuildParameters.cmake:310-325`)
- xxHash - Fast non-cryptographic hash (`cmake/CommonBuildParameters.cmake:269-273`)

**Compute & ML:**
- MNN (Mobile Neural Network) - AI inference engine (`cmake/CommonBuildParameters.cmake:46-49`)
- Vulkan - GPU compute API, with MoltenVK on Apple platforms (`cmake/CommonBuildParameters.cmake:10-30`)

**Networking Utilities:**
- libssh2 - SSH protocol library (`cmake/CommonBuildParameters.cmake:286-288`)
- gnus_upnp - UPnP NAT traversal (`cmake/CommonBuildParameters.cmake:304-307`)
- AsyncIOManager - Async I/O management (`cmake/CommonBuildParameters.cmake:298-301`)

**Logging:**
- spdlog - High-performance logging library (uses external fmt) (`src/GeniusSDK.cpp:15`, `cmake/CommonBuildParameters.cmake:143-146`)
- soralog - Structured logging library (`cmake/CommonBuildParameters.cmake:52-54`)

**Formatting & Utilities:**
- fmt - Modern string formatting library (`cmake/CommonBuildParameters.cmake:41-43`)
- Microsoft GSL (Guidelines Support Library) (`cmake/CommonBuildParameters.cmake:134-135`)
- tsl::hat_trie - Hat-trie data structure (`cmake/CommonBuildParameters.cmake:153-156`)
- Snappy - Google compression library (`cmake/CommonBuildParameters.cmake:122-123`)
- stb - Single-file header libraries (image loading) (`cmake/CommonBuildParameters.cmake:131`)
- zlib - Compression (`cmake/CommonBuildParameters.cmake:32-33`)

**Testing:**
- Google Test (GTest) + Google Mock (GMock) - Unit testing framework (`cmake/CommonBuildParameters.cmake:36-38`, `cmake/functions.cmake:11-14`)
- Boost.UnitTestFramework - Alternative test framework (included in Boost components)

**Build/Dev:**
- CMake - Build system (`cmake/CommonCompilerOptions.cmake`)
- Ninja - Preferred build tool on POSIX (`build/README.md`)
- ccache - Compiler cache (used in CI) (`.github/workflows/cmake.yml`)
- clang-format - Code formatter (`.clang-format` - Microsoft-based style, C++17 standard)
- clang-tidy - Static analysis (`.clang-tidy` - extensive rule set with boost, bugprone, cert, concurrency, cppcoreguidelines, modernize, performance, readability checks)

## Configuration

**Environment:**
- No `.env` file used at build time
- CI secrets via GitHub Actions secrets: `GNUS_TOKEN_1` (for GitHub API, GHCR container registry auth)
- Service environment files: `/etc/geniussdk/common.conf`, `/etc/geniussdk/full-node.conf` (optional, read at runtime by Linux systemd services)

**Build:**
- `build/CommonBuildParameters.cmake` - Master dependency configuration (delegates to `cmake/CommonBuildParameters.cmake`)
- `build/CommonCompilerOptions.cmake` - C++ standard, build options, toolchain auto-download, sanitizer support
- `cmake/CompilationFlags.cmake` - Compiler warning/error flags
- `cmake/functions.cmake` - Custom CMake functions (`addtest`, `geniussdk_install`, etc.)
- `build/CompilationFlags.cmake.example` - Example compilation flags template
- `build/CommonBuildParameters.cmake.example` - Example build parameters template
- Build type: `Debug`, `Release`, `RelWithDebInfo` (CMake `CMAKE_BUILD_TYPE`)
- Platform subdirectories: `build/Android/`, `build/Linux/`, `build/OSX/`, `build/Windows/`, `build/iOS/`
- Key CMake variables: `THIRDPARTY_DIR`, `THIRDPARTY_BUILD_DIR`, `SUPERGENIUS_DIR`, `SUPERGENIUS_BUILD_DIR`, `ZKLLVM_BUILD_DIR`, `SGNS_ENABLE_RELEASE_SYMBOLS`

**Runtime Configuration:**
- `dev_config.json` - Developer configuration file (Address, Cut, TokenValue, TokenID) expected at node base path (`src/GeniusSDK.cpp:139-148`, `example/dev_config.json`)
- `log_config.json` - Optional log level overrides loaded via `GeniusSDKLoadLogConfig()` (`src/GeniusSDK.cpp:641-647`)
- Service systemd units with environment file support (`services/genius-*.service`)

## Platform Requirements

**Development:**
- CMake 3.x+
- Clang or GCC with C++17 support, or Visual Studio 2022 on Windows
- Ninja (preferred) or Make
- Android NDK r27b for Android builds
- Rust toolchain (for Android cross-compilation targets)
- Git with submodules support
- ccache (optional, for faster rebuilds)

**Production:**
- Linux: systemd-based services (Debian Bullseye base), three node roles: full-node, archive-node, job-poster (deprecated)
- macOS: Native bundle or framework (universal binary)
- iOS: Framework (arm64)
- Android: armeabi-v7a and arm64-v8a ABIs
- Windows: x64 DLL
- Service user: `geniussdk`
- Install prefix: `${CMAKE_INSTALL_PREFIX}` (configurable)

---

*Stack analysis: 2026-07-03*
