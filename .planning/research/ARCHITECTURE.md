# Architecture Patterns

**Domain:** C SDK Init Interface — Facade over C++ Engine
**Researched:** 2026-07-03
**Confidence:** HIGH — source code verified

## Recommended Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│ C Consumers (Unity, Unreal, C apps)                                  │
│   GeniusSDKInitWithKey("./data", "0xDEAD...")                       │
└──────────────────────────┬───────────────────────────────────────────┘
                           │ C ABI (extern "C")
┌──────────────────────────▼───────────────────────────────────────────┐
│ GeniusSDK.h / GeniusSDK.cpp (Facade Layer)                           │
│                                                                      │
│ SDKInitHelper<Creator>(base_path, creator)                           │
│   ├── ReadDevConfigFromJSON(base_path) → DevConfig_st               │
│   ├── [Optional: WriteNetworkConfig if file missing]                 │
│   ├── [Optional: WriteSgnsConfig if file missing]                    │
│   └── creator(config) → shared_ptr<GeniusNode>                      │
│                                                                      │
│ Each init function provides a lambda:                                │
│   GeniusSDKInit()          → NewAccount{}                            │
│   GeniusSDKInitWithKey()   → FromPrivateKey{key}                     │
│   GeniusSDKInitWithMnemonic() → FromMnemonic{mnemonic}               │
│   GeniusSDKInitWithKeyAndDevConfig() → inline JSON + FromPrivateKey  │
│   GeniusSDKInitMinimal()   → delegates to InitWithKey()              │
└──────────────────────────┬───────────────────────────────────────────┘
                           │ C++ shared_ptr
┌──────────────────────────▼───────────────────────────────────────────┐
│ SuperGenius Engine (sgns::GeniusNode)                                │
│                                                                      │
│ GeniusNode::New(DevConfig_st config, AccountSource source)           │
│   ├── LoadSgnsConfig(base_path) → node_type, is_processor, net_id   │
│   ├── InitNetwork(port_seed, is_full_node)                          │
│   ├── Create account via source variant                              │
│   │     NewAccount   → GeniusAccount::New()                          │
│   │     FromPrivateKey → GeniusAccount::NewFromPrivateKey()          │
│   │     FromMnemonic   → GeniusAccount::NewFromMnemonic()            │
│   │     FromPublicKey  → GeniusAccount::NewFromPublicKey()           │
│   └── Start async init (DB, blockchain, transactions, processing)   │
│                                                                      │
│ Config files at base_path:                                           │
│   dev_config.json     → DevConfig_st (Addr, Cut, TokenValue, TokenID)
│   network_config.json → port_seed, auto_dht, pubsub_port            │
│   sgns_config.json    → node_type, is_processor, net_id             │
└──────────────────────────────────────────────────────────────────────┘
```

### Component Boundaries

| Component | Responsibility | Communicates With |
|-----------|---------------|-------------------|
| `GeniusSDK.h` | Public C ABI declarations — 5 init functions, 50+ runtime functions | C consumers (static/dynamic linking) |
| `GeniusSDK.cpp` `SDKInitHelper` | Config loading, node creation, error handling, optional config bootstrapping | `GeniusSDK.h`, `GeniusNode.hpp` |
| Init function lambdas | Build `AccountSource` variant from C params, call `GeniusNode::New()` | `SDKInitHelper`, `GeniusNode::New()` |
| `sgns::GeniusNode::New()` | Unified factory: config validation, account creation, async subsystem init | `GeniusAccount`, network layer, blockchain, processing |
| `GeniusNode::WriteNetworkConfig()` | Write minimal `network_config.json` | Filesystem at `base_path` |
| `GeniusNode::WriteSgnsConfig()` | Write minimal `sgns_config.json` | Filesystem at `base_path` |

### Data Flow

```
1. C caller: GeniusSDKInitWithKey("./data", "0xDEAD...")
2. SDKInitHelper called with:
   - base_path = "./data"
   - creator = lambda that builds FromPrivateKey{"0xDEAD..."}
3. ReadDevConfigFromJSON("./data") → parses dev_config.json → DevConfig_st
4. [Optional] WriteNetworkConfig("./data", 40001, true) if file missing
5. [Optional] WriteSgnsConfig("./data", "Light", true) if file missing
6. creator(config) → GeniusNode::New(config, AccountSource{FromPrivateKey{"0xDEAD..."}})
7. GeniusNode internally:
   - LoadSgnsConfig() → reads node_type, is_processor
   - InitNetwork() → reads port_seed, auto_dht
   - GeniusAccount::NewFromPrivateKey(key, is_full_node) → creates/restores wallet
   - Async init → DB, blockchain, transactions, processing
8. Return shared_ptr<GeniusNode> → stored in GeniusNodeInstance
9. Return "Initialized on ./data" to C caller
```

## Patterns to Follow

### Pattern 1: Template-based Creator Injection (Existing)

**What:** `SDKInitHelper<Creator>` takes a callable that builds a `shared_ptr<GeniusNode>` from a `DevConfig_st`. Each init function provides its own lambda.

**When:** All init functions except `InitWithKeyAndDevConfig` (which has its own inline config parsing).

**Example (post-refactoring):**
```cpp
// GeniusSDKInitWithKey — after stripping params
const char *GeniusSDKInitWithKey(const char *base_path, const char *eth_private_key) {
    return SDKInitHelper(base_path,
        [&](const auto &config) {
            return sgns::GeniusNode::New(config,
                sgns::AccountSource{sgns::FromPrivateKey{eth_private_key}});
        });
}
```

### Pattern 2: Variant-based Account Dispatch (New Engine)

**What:** `GeniusNode::New()` uses `std::visit` or `if constexpr` to dispatch on `AccountSource`. The C facade layer must construct the right variant type without exposing it in the header.

**When:** Every init call.

**Example:**
```cpp
// Internal to GeniusNode.cpp (not in GeniusSDK.cpp — this is the engine)
std::shared_ptr<GeniusNode> GeniusNode::New(const DevConfig_st &dev_config, AccountSource source) {
    return std::visit([&](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, NewAccount>) {
            // generate new identity
        } else if constexpr (std::is_same_v<T, FromPrivateKey>) {
            // restore from key
        } // ...
    }, source);
}
```

### Pattern 3: Config Bootstrapping (New)

**What:** Before calling `GeniusNode::New()`, check if `network_config.json` and `sgns_config.json` exist at `base_path`. If not, write defaults using `GeniusNode::WriteNetworkConfig()` and `GeniusNode::WriteSgnsConfig()`.

**When:** First-time init for a given `base_path`. Existing configs are never overwritten.

**Example:**
```cpp
template <typename Creator>
const char *SDKInitHelper(const char *base_path, Creator create_node) {
    // ... existing validation ...

    // Bootstrap network config if missing
    std::string network_cfg = std::string(base_path) + "network_config.json";
    if (!std::ifstream(network_cfg).good()) {
        sgns::GeniusNode::WriteNetworkConfig(base_path, 40001, true);
    }

    // Bootstrap sgns config if missing
    std::string sgns_cfg = std::string(base_path) + "sgns_config.json";
    if (!std::ifstream(sgns_cfg).good()) {
        sgns::GeniusNode::WriteSgnsConfig(base_path, "Light", true);
    }

    // ... existing node creation ...
}
```

## Anti-Patterns to Avoid

### Anti-Pattern 1: Hardcoded Config in C Facade

**What:** Writing C defaults like `autodht = true; baseport = 40001;` directly in the SDK and passing them to the engine without JSON files.

**Why bad:** The new engine reads from JSON files. If the facade passes values inline but the engine reads JSON, they'll be out of sync. Config changes require SDK recompilation instead of JSON edits.

**Instead:** Always write config to JSON files (Pattern 3) and let the engine read them. The JSON file is the single source of truth.

### Anti-Pattern 2: Leaking C++ Types Through the C Header

**What:** Adding `#include <variant>` or `sgns::AccountSource` to `GeniusSDK.h`.

**Why bad:** Breaks C compilation. Unity, Unreal, and C consumers can't compile with C++14 types.

**Instead:** Keep the header C89. The C functions take primitive types (`const char*`). Internal `.cpp` constructs the variants.

### Anti-Pattern 3: Inline AccountSource Construction in Every Init Function

**What:** Each `GeniusSDKInit*` function writing the full `std::visit` dispatch logic.

**Why bad:** Duplication. The dispatch lives in `GeniusNode::New()` — the facade just needs to build the right variant.

**Instead:** Each init function builds exactly one `AccountSource` alternative and passes it to `GeniusNode::New()`. One-liner lambdas.

## Scalability Considerations

| Concern | Current (5 init functions) | +10 init functions | Notes |
|---------|---------------------------|-------------------|-------|
| New init function addition | Add one function + one lambda | Same — O(1) per function | Template pattern scales linearly |
| Config drift | 5 functions, same config source | All functions share JSON files | JSON files are the scaling limit — no code change needed |
| `AccountSource` variants | 3 used (NewAccount, FromPrivateKey, FromMnemonic) | +FromPublicKey when needed | Add one C function when ready — no refactoring of existing |
| `SDKInitHelper` template | Generic lambda acceptor | No changes needed | Template is variant-agnostic |

## Sources

- **GeniusSDK.cpp (184-213)**: `SDKInitHelper` template — loads config, calls lambda creator, stores result. [HIGH — source code]
- **SuperGenius GeniusNode.hpp (106-107)**: `New(DevConfig_st, AccountSource)` — unified factory signature. [HIGH — source code]
- **SuperGenius GeniusNode.hpp (82-86)**: AccountSource variant definition with 4 alternatives. [HIGH — source code]
- **SuperGenius GeniusNode.cpp (283-355)**: Constructor reads JSON configs, derives `is_full_node_` from `node_type`, calls account factory. [HIGH — source code]
