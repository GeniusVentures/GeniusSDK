# Joining the staging net

One published artifact set, one documented procedure: point a fresh node at
the live staging net and prove it synced. Every fact the join consumes comes
from the artifact set beside this document — `staging-net.json` (the pinned
facts) and `manifest.bin` (the ceremony manifest bytes, byte-identical to the
ceremony's output). Nothing here recomputes a ceremony value; the artifact is
the contract.

The joiner shape (proven in Phase 3): a joiner boots WITHOUT a key file — a
fresh account — but needs the FULL trust payload (net pin + bootstrapper +
trusted peers + authorized full node), or it refuses the net's genesis and
fails closed. `topology.py --join` builds that payload from the artifact.

## 1. Prerequisites

- macOS or Linux, ~30 GB free disk (the SuperGenius build tree is large)
- CMake >= 3.20, a C++17 toolchain (clang recommended), Python 3
- Outbound TCP to the genesis entry (the `genesis_multiaddr` in the artifact
  carries the address and port)

## 2. Fetch the artifact set

The canonical copies live on the `develop` branch of this repository — fetch
them by raw URL so you always get the reviewed set:

```bash
mkdir -p staging-net
curl -fsSL -o staging-net/staging-net.json \
  https://raw.githubusercontent.com/GeniusVentures/GeniusSDK/develop/deploy/staging-net/staging-net.json
curl -fsSL -o staging-net/manifest.bin \
  https://raw.githubusercontent.com/GeniusVentures/GeniusSDK/develop/deploy/staging-net/manifest.bin
```

Sanity-check the pair (the manifest bytes are the published contract, never
regenerated): the artifact's `ceremony_fingerprint` must equal
`sha256(manifest.bin)`:

```bash
jq -r .ceremony_fingerprint staging-net/staging-net.json
shasum -a 256 staging-net/manifest.bin   # same value
```

The `node_version` / `supergenius_tarball_sha256` / `runner_sha256` fields pin
the exact binaries the fleet runs (CI-built Linux). If you build from source
on another platform your binary will differ — the sync gate in step 6, not a
byte match, proves your build joins the net.

## 3. Build the runner from source

Follow the repository Readme for the full SuperGenius + third-party layout.
The essential chain mirrors CI (`ceremony-e2e.yml`), with the Readme's
placeholder convention:

Linux (clang):

```bash
cd [ABSOLUTE_PATH_TO_SUPERGENIUS_PROJECT]
cmake -S build/Linux -B build/Linux/Release/x86_64 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_CXX_COMPILER=$(which clang++) \
  -DTHIRDPARTY_BUILD_DIR=[ABSOLUTE_PATH_TO_THIRDPARTY_BUILD] \
  -DZKLLVM_BUILD_DIR=[ABSOLUTE_PATH_TO_ZKLLVM_BUILD] \
  -DSUPERGENIUS_DIR=[ABSOLUTE_PATH_TO_SUPERGENIUS_PROJECT] \
  -DABI_SUBFOLDER_NAME='/x86_64'

cd [ABSOLUTE_PATH_TO_GENIUSSDK_PROJECT]
cmake -S build/Linux -B build/Linux/Release/x86_64 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_CXX_COMPILER=$(which clang++) \
  -DTHIRDPARTY_BUILD_DIR=[ABSOLUTE_PATH_TO_THIRDPARTY_BUILD] \
  -DZKLLVM_BUILD_DIR=[ABSOLUTE_PATH_TO_ZKLLVM_BUILD] \
  -DSUPERGENIUS_DIR=[ABSOLUTE_PATH_TO_SUPERGENIUS_PROJECT] \
  -DABI_SUBFOLDER_NAME='/x86_64'
cmake --build build/Linux/Release/x86_64 --config Release -j
# runner: build/Linux/Release/x86_64/services/GeniusSDKService
```

macOS (the developer-machine chain):

```bash
cd [ABSOLUTE_PATH_TO_GENIUSSDK_PROJECT]/build/OSX
mkdir -p Debug && cd Debug
cmake .. -DCMAKE_BUILD_TYPE=Debug \
  -DTHIRDPARTY_DIR=[ABSOLUTE_PATH_TO_THIRDPARTY_PROJECT] \
  -DSUPERGENIUS_DIR=[ABSOLUTE_PATH_TO_SUPERGENIUS_PROJECT]
cmake --build . --config Debug -j
# runner: build/OSX/Debug/services/GeniusSDKService
```

The runner version must accept net id 333 (the `--net-id` registry change);
any build from this repository's `develop` does.

## 4. Generate the joiner configs

`topology.py` (in `e2e/`) consumes the artifact set and writes a joiner base
dir — the full trust payload rebuilt from the artifact's pinned facts, with
`bootstrap_addresses` pointing at the artifact's `genesis_multiaddr`. No key
file is ever written:

```bash
cd [ABSOLUTE_PATH_TO_GENIUSSDK_PROJECT]/e2e
python3 topology.py --join [ABSOLUTE_PATH_TO_staging-net] \
  --out-dir [ABSOLUTE_PATH_TO_JOINER_BASE_PARENT] --count 1
```

This creates `joiner1/` containing `network_config.json`,
`sgns_config.json`, `dev_config.json`, and `log_config.json`. Verify the net
pin landed: `jq .net_id [ABSOLUTE_PATH_TO_JOINER_BASE_PARENT]/joiner1/sgns_config.json`
must print `333`.

## 5. Boot the joiner

The runner takes the base dir and NO `--key-file` — a joiner is a fresh
account:

```bash
[ABSOLUTE_PATH_TO_GENIUSSDK_SERVICE] [ABSOLUTE_PATH_TO_JOINER_BASE_PARENT]/joiner1/ 2>&1 | tee joiner.log
```

(On Linux the same dbus session wrapper the fleet uses applies — see
`deploy/systemd/` and `deploy/sgns-node-wrap.sh` in this repository for the
supervised shape.)

## 6. Verify sync (no RPC)

Watch the joiner's stdout log for the STATUS cadence. Expect to sit in
INITIALIZING_BLOCKCHAIN for ~60–90s while the genesis syncs — that is normal,
not stuck; READY typically lands within ~2 minutes of boot. The genesis-sync
evidence itself lands in the base dir's `sgnslog2.log`, not stdout
(`grep -i genesis [ABSOLUTE_PATH_TO_JOINER_BASE_PARENT]/joiner1/sgnslog2.log`
shows the request/verify lines; stdout carries only the STATUS cadence). After
the node reports READY, its head must equal the artifact's genesis CID — the
same value every fleet node settled on at ceremony time, consumed here from
the artifact and never recomputed:

```bash
jq -r .genesis_cid staging-net/staging-net.json
grep -oE "STATUS .*head=[A-Za-z0-9]+" joiner.log | tail -3
```

The join is proven when the sampled `head=` lines match the artifact's
`genesis_cid` exactly and the state stays READY. A mismatch or a
FATAL_TRUST_MISMATCH means the trust payload does not describe this net —
re-fetch the artifact set (step 2) before anything else.

## Troubleshooting

- `head=` never appears / INITIALIZING_BLOCKCHAIN well past the ~2 min norm:
  check outbound TCP to the `genesis_multiaddr` address and port first.
- `Error starting blockchain … Scheduling blockchain retry after failure`
  during INITIALIZING_BLOCKCHAIN: expected, not a failure — the node retries
  until the genesis syncs.
- `ChainRpcEndpointProvider: cannot open …/bridge_chains_config.json`:
  expected, not a failure for a joiner — the bridge chain config wires
  EVM-relay endpoints and plays no part in syncing the net.
- FATAL_TRUST_MISMATCH: the `sgns_config.json` facts and the live net
  disagree — stale artifact; re-fetch.
- The joiner must not pass `--key-file`: that shape belongs to fleet nodes,
  not joiners.
