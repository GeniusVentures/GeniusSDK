---
last_mapped_commit: 35507476cbaa482c1c9a6e3582f8475a9f39304c
---

# External Integrations

**Analysis Date:** 2026-07-03

## APIs & External Services

**Blockchain & Token System:**
- SuperGenius Node (`sgns::genius_node`) - Core blockchain node dependency
  - CMake target: `sgns::genius_node`
  - Location: Resolved from `SUPERGENIUS_DIR` / `SUPERGENIUS_BUILD_DIR` CMake variables
  - Sub-libraries: `evmrelay`, `ProofSystem`, `SGProcessingManager` (`cmake/CommonBuildParameters.cmake:423-433`)
  - Provides: Token minting, transfers, balances, transaction management, processing orchestration, IPFS integration, blockchain state
  - Source: Pre-built releases from `github.com/GeniusVentures/SuperGenius`, or built locally in sibling directory

**Ethereum Compatibility:**
- Ethereum-compatible private key authentication
  - SDK accepts `eth_private_key` (hex string with optional `0x` prefix) for wallet derivation (`src/GeniusSDK.h:219-223`)
  - Mnemonic-based key derivation supported (`src/GeniusSDK.h:225-230`)
  - Implementation: TrustWalletCore (`cmake/CommonBuildParameters.cmake:310-325`) — TrezorCrypto, wallet_core_rs
  - secp256k1 for elliptic curve operations (`cmake/CommonBuildParameters.cmake:263-267`)
  - ed25519 signatures (`cmake/CommonBuildParameters.cmake:248-251`)
  - Address format: `0x` prefix + 128 hex chars (64 bytes) (`src/GeniusSDK.h:57-64`)

**Minting Integration:**
- Token minting requires `transaction_hash` and `chain_id` parameters indicating a bridging/minting event from an external chain (`src/GeniusSDK.h:432-435`)
- `GeniusSDKMint()` and `GeniusSDKMintGNUS()` are the mint entry points (`src/GeniusSDK.cpp:417-473`)

**Price Oracle:**
- `GeniusSDKGetGNUSPrice()` - Returns current GNUS token price in USD (`src/GeniusSDK.cpp:333-343`)
  - Implementation: Delegates to `GeniusNodeInstance->GetGNUSPrice()`
  - Data source: Internal to SuperGenius node (no external API call visible in SDK layer)

**Dev Payment:**
- `GeniusSDKPayDev()` - Pays the developer cut for in-game transactions (`src/GeniusSDK.cpp:570-590`)
  - Uses dev configuration from `dev_config.json` (`Cut` and `Address` fields)

## Data Storage

**Databases:**
- RocksDB - Embedded key-value store for blockchain state and transactions
  - Client: `RocksDB` via CMake CONFIG package (`cmake/CommonBuildParameters.cmake:126-128`)
  - Used by: SuperGenius node internally (no direct SDK API)
- SQLite3 - Local structured data storage
  - Client: `SQLiteModernCpp` (modern C++ wrapper) + raw `sqlite3` (`cmake/CommonBuildParameters.cmake:204-215`)
  - Used by: SuperGenius node for wallet, account, and configuration storage
  - Migration step: `GENIUS_NODE_MIGRATING_DATABASE` and `GENIUS_NODE_INITIALIZING_DATABASE` node states (`src/GeniusSDK.h:111-112`)

**File Storage:**
- IPFS (InterPlanetary File System) - Decentralized content-addressed storage for processing results and task data
  - Client: `ipfs-lite-cpp` (embedded IPFS node), `ipfs-pubsub` (pub/sub messaging), `ipfs-bitswap-cpp` (data exchange) (`cmake/CommonBuildParameters.cmake:232-246`)
  - Task results: Serialized protobuf returned as bytes via `GeniusSDKGetTaskResult()` (`src/GeniusSDK.cpp:912-935`)
  - Task IDs are `ipfs_block_id` values (`src/GeniusSDK.h:539`)
- Local filesystem - Node data stored at `base_path` (configurable, passed as first argument to all init functions)
  - `dev_config.json` read from base path (`src/GeniusSDK.cpp:139`)
  - Default service paths: `/var/lib/geniussdk/full/`, `/var/lib/geniussdk/archive/`, `/var/lib/geniussdk/jobposter/`

**Caching:**
- Not explicitly configured at SDK layer (handled internally by RocksDB and node runtime)

## Authentication & Identity

**Auth Provider:**
- Custom wallet-based authentication (no external identity provider)
  - Ethereum-compatible private key or mnemonic (BIP39) → derives public address
  - Multiple accounts per node: `GeniusSDKAddAccountWithPrivateKey()`, `GeniusSDKAddAccountWithMnemonic()`, `GeniusSDKAddAccountWithRandomMnemonic()` (`src/GeniusSDK.cpp:700-740`)
  - Account management: `GeniusSDKSelectGeniusAccount()`, `GeniusSDKGetAvailableAccounts()`, `GeniusSDKDeleteAccount()`, `GeniusSDKSetPayoutAddress()` (`src/GeniusSDK.cpp:666-810`)
  - Active account used for all subsequent operations (process, transfer, mint)
  - Linux keyring integration: `libsecret-1` dependency on Linux for secure key storage (`cmake/CommonBuildParameters.cmake:330-333`)
  - Android: `securestorage-release.aar` installed alongside SDK (`cmake/CommonBuildParameters.cmake:496-501`)

**Key Management:**
- Private keys stored encrypted (via OS keychain mechanisms)
- Mnemonics returned once on creation, never stored by SDK

## Monitoring & Observability

**Error Tracking:**
- No external error tracking service detected
- Errors returned via `GeniusNodeReturnValue_t` enum values (`src/GeniusSDK.h:93-102`)
- Boost.Exception used for structured error propagation (`src/GeniusSDK.cpp:38-50`)

**Logs:**
- spdlog - Structured logging used throughout SDK code (`src/GeniusSDK.cpp:15`, `SPDLOG_ERROR`, `SPDLOG_INFO` macros)
- soralog - Additional structured logging used by SuperGenius internals
- Runtime log configuration via `log_config.json` (`GeniusSDKLoadLogConfig()`, `src/GeniusSDK.cpp:641-647`)
- Systemd journal output for Linux services (`StandardOutput=journal`, `StandardError=journal` in service files)
- Debug symbol generation: dSYM (macOS), PDB (Windows), debuglink (Linux/Android) when `SGNS_ENABLE_RELEASE_SYMBOLS` is ON

## CI/CD & Deployment

**Hosting:**
- Self-hosted GitHub Actions runners: `sg-ubuntu-linux`, `sg-arm-linux`, `SG-WIN11`, `gv-OSX-Large`
- Fallback to GitHub-hosted runners: `ubuntu-latest`, `ubuntu-24.04-arm`, `windows-2022`, `macos-latest` (`.github/workflows/cmake.yml`)
- GitHub Releases for artifact distribution (`.github/workflows/cmake.yml`, `.github/workflows/build-release-tag.yml`)

**CI Pipeline:**
- GitHub Actions (`.github/workflows/cmake.yml`) - Triggered on push/PR to `develop` and `main`, plus `workflow_dispatch`
- GitHub Actions (`.github/workflows/build-release-tag.yml`) - Manual tag-based release builds with separate dependency tag selection
- Matrix builds: 5 platforms × 2 build types × 2-4 ABIs → 12 build configurations per run
- Docker container: `ghcr.io/geniusventures/debian-bullseye:latest` for Linux builds
- Dependency CDN: Dependencies (thirdparty, zkLLVM, SuperGenius) pre-built and fetched from GitHub releases on `GeniusVentures/thirdparty`, `GeniusVentures/zkLLVM`, `GeniusVentures/SuperGenius`
- ccache enabled across all CI builds (`GRPC_BUILD_ENABLE_CCACHE: "ON"`)

**Deployment:**
- Linux: systemd service units shipped with SDK (`services/genius-*.service`)
  - `genius-full-node.service` - Full node with processing (`genius-full-node.service`)
  - `genius-archive-node.service` - Archive node without processing (`genius-archive-node.service`)
  - `genius-job-poster.service` - Job poster (deprecated, merging into full node) (`genius-job-poster.service`)
  - Install path: `/usr/bin/GeniusSDKService`, config in `/etc/geniussdk/`, data in `/var/lib/geniussdk/`
- macOS: Bundle (`GeniusSDK_bundle`) and Framework (`GeniusSDK_framework`) targets (`src/CMakeLists.txt:75-118`)
- iOS: Framework with `Security` framework linking (`src/CMakeLists.txt:75-100`)
- Android: Standard shared library with secure storage AAR
- Windows: Shared library (DLL) with PDB symbols

## Environment Configuration

**Required env vars (CI):**
- `GNUS_TOKEN_1` - GitHub personal access token for API calls and GHCR authentication (`.github/workflows/cmake.yml:31,137`)
- `ANDROID_NDK` / `ANDROID_NDK_HOME` - Android NDK path for Android builds
- `VULKAN_SDK` - Vulkan SDK path (optional, falls back to thirdparty build)

**Required env vars (Runtime - Linux services):**
- `GENIUSSDK_BIN` - Path to `GeniusSDKService` binary (default: `/usr/bin/GeniusSDKService`)
- `GENIUSSDK_FULL_PATH` / `GENIUSSDK_ARCHIVE_PATH` / `GENIUSSDK_JOBPOSTER_PATH` - Node data directory
- `GENIUS_PROCESS` - Enable/disable processing flag
- `/etc/geniussdk/common.conf` - Optional shared configuration file (`EnvironmentFile=-`)

**Secrets location:**
- CI: GitHub Actions secrets (`GNUS_TOKEN_1`)
- Runtime: OS keychain (`libsecret-1` on Linux, `Security` framework on Apple, `securestorage-release.aar` on Android)
- No `.env` files present in repository (`.env` and `.env.*` are gitignored via `.gitignore:89-94`)

## Webhooks & Callbacks

**Incoming:**
- None - This is a C SDK (static/shared library), not an HTTP server
- JSON-RPC protocol support via `jsonrpc-lean` for internal node communication only (`cmake/CommonBuildParameters.cmake:260-261`)

**Outgoing:**
- DHT peer discovery via libp2p (`autodht` parameter controls auto-discovery, `src/GeniusSDK.h:196-198`)
- IPFS pubsub messaging for distributed task distribution and results (`ipfs-pubsub`)
- IPFS bitswap for block exchange (`ipfs-bitswap-cpp`)
- UPnP port mapping for NAT traversal (`gnus_upnp`)
- GNUS price fetching - Internal to SuperGenius node (details at `GeniusSDKGetGNUSPrice()`, `src/GeniusSDK.cpp:333-343`)

**Network Ports:**
- Configurable `baseport` parameter (default: 40001 for full/archive nodes, 40002 for job poster, `src/GeniusSDK.cpp:295-297`)
- Ports managed by libp2p DHT and UPnP for P2P communication

## Processing Pipeline

**Job Processing:**
- `GeniusSDKProcess()` - Submits JSON job data for AI processing via SuperGenius node (`src/GeniusSDK.cpp:299-320`)
- `GeniusSDKCheckJobValidity()` - Validates job JSON before submission (`src/GeniusSDK.cpp:322-331`)
- `GeniusSDKGetCost()` / `GeniusSDKGetCostGNUS()` - Estimates processing cost (`src/GeniusSDK.cpp:592-628`)
- `GeniusSDKGetTaskResult()` - Retrieves completed task results as serialized protobuf (`src/GeniusSDK.cpp:912-935`)
- `GeniusSDKGetMyTaskIds()` - Lists submitted task IDs (`src/GeniusSDK.cpp:877-910`)
- `GeniusSDKGetProcessingStatus()` - Returns processing state and progress percentage (`src/GeniusSDK.cpp:842-875`)
- Processing powered by MNN (AI inference) and Vulkan (GPU compute)
- Processing service: `SGProcessingManager` from SuperGenius, accessed via `sgns::sgprocessing::ProcessingManager` (`src/GeniusSDK.cpp:328,594`)

---

*Integration audit: 2026-07-03*
