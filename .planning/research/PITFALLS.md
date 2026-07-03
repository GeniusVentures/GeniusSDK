# Domain Pitfalls

**Domain:** C SDK Init Interface Refactoring (parameter stripping + engine realignment)
**Researched:** 2026-07-03
**Confidence:** HIGH — source code verified, existing patterns analyzed

## Critical Pitfalls

Mistakes that cause runtime crashes, silent misbehavior, or require reverting the refactoring.

### Pitfall 1: Config Files Missing → Engine Starts with Wrong Defaults

**What goes wrong:** The new `GeniusNode::New()` reads `sgns_config.json` and `network_config.json` from `base_path`. If these files don't exist, the engine uses hardcoded defaults (Light node, is_processor=true, port_seed=40001, auto_dht=true). Old callers who were passing `autodht=false` or `is_full_node=true` will silently get different behavior.

**Why it happens:** The old API let callers control these values per-call. The new engine reads from JSON files that may not exist yet.

**Consequences:** A caller who was running as a full node with autodht disabled suddenly runs as a light node with DHT enabled. No error — just silently wrong behavior.

**Prevention:** Bootstrap default config files before calling `GeniusNode::New()` (Pattern 3 in ARCHITECTURE.md). For callers who need non-default values, provide clear documentation on editing the JSON files. Alternatively, accept the engine defaults as the new defaults and document the change.

**Detection:** Compare `GeniusNode::IsFullNode()` and `GeniusNode::IsAutodhtEnabled()` output before and after migration. Any caller relying on old param values will diverge.

### Pitfall 2: `GeniusSDKInitMinimal` Signature Mismatch After Stripping

**What goes wrong:** The current `GeniusSDKInitMinimal(base_path, key, baseport)` delegates to `GeniusSDKInitWithKey(base_path, key, true, true, baseport, false)`. After stripping the 4 bool params, `GeniusSDKInitWithKey` becomes `(base_path, key)`. But `GeniusSDKInitMinimal` still has `baseport` in its signature. Should `baseport` also be stripped?

**Why it happens:** `baseport` is one of the 4 stripped params. But `InitMinimal` was already "minimal" — it had fewer params than the full functions. After stripping, it becomes unclear what makes it different from `InitWithKey`.

**Consequences:** Two functions with identical signatures (`const char*, const char*`) that do different things — confusing API.

**Prevention:** Strip `baseport` from `InitMinimal` too. Make it a pure alias: `GeniusSDKInitMinimal(base_path, key)` → `GeniusSDKInitWithKey(base_path, key)`. If the functions become identical, deprecate `InitMinimal` or document it as an alias.

**Detection:** Check if the two functions have identical signatures post-refactoring. If yes, either merge or document the alias.

### Pitfall 3: `static std::string ret_val` in `SDKInitHelper` — Dangling Pointer Across Inits

**What goes wrong:** `SDKInitHelper` declares `static std::string ret_val = "Initialized on ";`. When `Shutdown()` + re-`Init()` is called with a new `base_path`, `ret_val.append()` grows the string (it's never cleared). The returned `const char*` points into a growing string.

**Why it happens:** The `static` string accumulates across init cycles. It's not reset to `"Initialized on "` on re-init.

**Consequences:** After shutdown and re-init, the return string is `"Initialized on /old/path/new/path"` — concatenated garbage. Confuses callers who parse the path.

**Prevention:** Reset `ret_val` at the start of `SDKInitHelper`:
```cpp
ret_val = "Initialized on ";
```
This is technically a pre-existing bug, but the refactoring touches these functions and provides an opportunity to fix it.

**Detection:** Init → Shutdown → Init with different base_path → check returned string.

### Pitfall 4: Header Changes Without Updating All Callers → Silent ABI Break

**What goes wrong:** Changing function signatures in `GeniusSDK.h` without updating all callers (examples, services) causes compilation failures. But if callers are in separate repos or linked as pre-compiled binaries, the ABI silently breaks — wrong number of arguments pushed on the stack.

**Why it happens:** C calling conventions depend on exact parameter counts and types. Removing 4 params changes the stack frame.

**Consequences:** At best, crashes. At worst, silent data corruption because the callee reads garbage values from the stack for the now-removed params.

**Prevention:** This is a compile-time error for C callers (type mismatch). For binary-only consumers (dynamic linking), bump the SO version (SONAME) to prevent linking against old binaries.

**Detection:** `nm -D libGeniusSDK.so | grep GeniusSDKInit` — verify exported symbol signatures match header.

## Moderate Pitfalls

### Pitfall 5: `GeniusSDKInitWithCredentials` Implicit Declaration

**What goes wrong:** `SDKExampleCredentials.cpp` calls `GeniusSDKInitWithCredentials()` which is NOT declared in `GeniusSDK.h`. It relies on C89 implicit function declaration (returns `int`). After the refactoring, this function may not exist or may have changed.

**Prevention:** Either add the function to the public header or (preferably) migrate the example to `GeniusSDKInitWithKey()`. The credentials pattern isn't part of the documented API surface.

### Pitfall 6: `SDKExample.cpp` `getSDKConfig()` Still Prompts for Old Params

**What goes wrong:** The example's `getSDKConfig()` function (line 467) prompts the user for `autodht`, `process`, and `baseport`. After stripping, these prompts are pointless. The config values come from JSON files, not interactive input.

**Prevention:** Remove `getSDKConfig()` entirely or replace with prompts for `base_path` and `eth_private_key` only.

### Pitfall 7: `service.cpp` CLI Argument Shift

**What goes wrong:** `service.cpp` parses `argv[2]` as `autodht`, `argv[3]` as `process`, `argv[4]` as `baseport`, `argv[5]` as `is_full_node`. After stripping, the positional args shift — or all config args are removed.

**Prevention:** Simplify `service.cpp` to only take `base_path`. All other config comes from JSON files at that path. Document the change in the header comment.

## Minor Pitfalls

### Pitfall 8: `DevConfig_st` Memory in `GeniusSDKInitWithKeyAndDevConfig`

**What goes wrong:** `InitWithKeyAndDevConfig` parses `dev_config` inline and stores the result locally. If the `DevConfig_st` copy in the lambda captures by reference, it could dangle.

**Prevention:** Lambda should capture `load_config_ret.value()` by value (move it). The current code already does this correctly — just verify during the refactoring.

### Pitfall 9: `process` Param in `GeniusSDKInit` Was Already Unused

**What goes wrong:** Looking at the current implementation, `GeniusSDKInit(base_path, autodht, process, baseport, is_full_node)` calls `GeniusNode::New(config, autodht, baseport, is_full_node)` — note no `process` in the engine call. The `process` param was already a dead param even in the old code for this specific function.

**Prevention:** Document this finding. The stripping makes things consistent but this particular param was already cosmetic. No behavior change for `GeniusSDKInit()`.

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Signature stripping (T1-T6) | Pitfall 4: Header change without full caller audit | Grep for all `GeniusSDKInit` call sites before changing signatures |
| Config bootstrapping (D1-D2) | Pitfall 1: Missing config files → wrong defaults | Always write defaults before calling `New()`; never rely on engine defaults |
| Caller update (T8) | Pitfall 5/6/7: Examples and service carry old logic | Audit all files; `getSDKConfig()` in example, CLI args in service |
| Re-init cycle | Pitfall 3: `static std::string` accumulation | Pre-existing bug; fix during refactoring (one-liner reset) |
| `InitMinimal` cleanup (T6, D4) | Pitfall 2: Identical signatures with `InitWithKey` | Either merge or document as alias |
| ABI verification (T9-T10) | Pitfall 4: Binary consumers with stale signatures | SONAME bump if shared library is distributed |

## Sources

- **GeniusSDK.cpp (184-213)**: `SDKInitHelper` with `static std::string ret_val` — Pitfall 3 (no reset on re-init). [HIGH — source code]
- **GeniusSDK.cpp (216-220)**: `GeniusSDKInit` already doesn't pass `process` to `GeniusNode::New()` — Pitfall 9. [HIGH — source code]
- **GeniusSDK.cpp (241-279)**: `InitWithKeyAndDevConfig` inline config parsing — Pitfall 8. [HIGH — source code]
- **SuperGenius GeniusNode.cpp (374-455)**: `LoadSgnsConfig()` reads JSON, applies defaults on missing file — Pitfall 1 (engine defaults may not match caller intent). [HIGH — source code]
- **SDKExampleCredentials.cpp (9)**: Calls `GeniusSDKInitWithCredentials` not in public header — Pitfall 5. [HIGH — source code]
- **service.cpp (8-34)**: CLI argument parsing for old params — Pitfall 7. [HIGH — source code]
- **SDKExample.cpp (467-483)**: `getSDKConfig()` prompts for old params — Pitfall 6. [HIGH — source code]
