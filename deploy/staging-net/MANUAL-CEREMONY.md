# The manual ceremony recipe (running sgns-trust by hand)

The harness (`e2e/ceremony_e2e.py --fleet`) drives the fleet ceremony
end-to-end, but the by-hand path is first-class: a mainnet operator with the
same `sgns-trust` binary can run the identical sequence without the harness.
This document maps one-to-one to the argv the harness actually ran for the
staging net (net id 333) — including the two flags that are easiest to get
wrong and whose absence fails silently: the net pin split and the topic.

## Why `--net-id` on every networked operation (the 04-01 finding)

Every pubsub topic carries the network-and-version appendix
(`GetNetAndVersionAppendix`): `.3.7.333` for the staging net, `.3.7.144` for
the DEV default. An actor started WITHOUT `--net-id 333` silently joins the
DEV topics — it lists no candidates, approves nothing, and the ceremony
starves with no error line pointing at the cause. Every NETWORKED operation
below (genesis, list, approve) carries the pin.

`make-manifest` is OFFLINE — it takes `--network-id` (the manifest's net
fact), never `--net-id` (the actor's topic pin). The two flags are not
interchangeable; the split below is exact.

## Prerequisites

- The five fleet node identities (key files, 0600, held offline) and their
  ceremony addresses: one bootstrapper A_1 + four ordered peers
- The runner (`GeniusSDKService`) deployed per `deploy/systemd/`, nodes
  booted to `STATUS node_state=WAITING_FOR_TRUST_GENESIS` with the genesis
  node's published multiaddr observed from its boot log
  (`PubSub started at address: ...`)
- `sgns-trust` built from the same SuperGenius tree as the runners

## Step 0 — the actor side-car files

Each actor (the bootstrapper's actor + one per peer) needs a network config
pointing at the genesis entry, and its own database dir. The shape the tool's
parser expects (`pubsub_port` is a string):

```bash
mkdir -p actor1db actor1
cat > actor1-network.json <<'EOF'
{
  "pubsub_port": "41051",
  "pubsub_bind_address": "0.0.0.0",
  "bootstrap_addresses": ["/ip4/208.113.209.106/tcp/41273/ipfs/12D3KooWMvXKGwt2mDcgusWrjALbLMwfYCgTvWJxMSmwLi633txx"]
}
EOF
```

(Distinct `pubsub_port` per actor; the `bootstrap_addresses` entry is the
genesis node's OBSERVED multiaddr — never a recomputed guess.)

Stage a 0600 COPY of each key file for the actors to consume: the tools
unlink the key file on success, and that assert must consume the copy, never
the offline original.

## Step 1 — make-manifest (offline)

One invocation, plain values. `--network-id`, not `--net-id`:

```bash
sgns-trust make-manifest \
  --network-id 333 \
  --bootstrapper [A_1_128_HEX_ADDRESS] \
  --peers [PEER_1],[PEER_2],[PEER_3],[PEER_4] \
  --out manifest.bin
```

The stdout prints the membership and burn thresholds (3 and 3 for five
nodes) and `fingerprint: <64 hex>`. Verify both ways before anything
networked happens:

```bash
shasum -a 256 manifest.bin   # must equal the printed fingerprint
# run make-manifest a second time to a different --out: the bytes must be
# byte-identical (the determinism contract)
```

## Step 2 — genesis (bootstrapper's actor, NETWORKED — pin the net)

```bash
sgns-trust genesis \
  --manifest manifest.bin \
  --network-config actor1-network.json \
  --database actor1db \
  --topic SuperGNUSNode.TestNet.FullNode \
  --net-id 333 \
  --key-file actor1.key \
  --serve-seconds 180 \
  --timeout-seconds 180
```

The tool prompts for the fingerprint — type the exact 64-hex string from
step 1 (it is read before the prompt renders). Success is the pair of lines
`Serving genesis to peers for ...` and `Genesis durably confirmed.`; the
actor's key COPY is unlinked on success (the offline original stays intact).

## Step 3 — list (each peer's actor, NETWORKED — pin the net)

Run inside the genesis serve window, once per peer actor (each with its own
network config, database, and key copy):

```bash
sgns-trust list \
  --manifest manifest.bin \
  --network-config actor2-network.json \
  --database actor2db \
  --topic SuperGNUSNode.TestNet.FullNode \
  --net-id 333 \
  --timeout-seconds 180
```

`list` exits 0 even when empty — the content is the gate: wait for the line
`burn burn-config:1:<64 hex>` (the burn candidate id). No candidate after the
catch-up window means the actor is on the wrong net or topic; check the
`--net-id 333` pin before anything else.

## Step 4 — approve (each peer's actor, NETWORKED — pin the net)

Approve the exact candidate id observed in step 3, once per peer until the
burn threshold (3 of 4 for five nodes; the staging ceremony approves all 4):

```bash
sgns-trust approve \
  --manifest manifest.bin \
  --network-config actor2-network.json \
  --database actor2db \
  --topic SuperGNUSNode.TestNet.FullNode \
  --net-id 333 \
  --key-file actor2.key \
  --candidate-id burn-config:1:[64_HEX_FROM_LIST] \
  --serve-seconds 180 \
  --timeout-seconds 180
```

Success echoes the candidate id and prints `Serving updated trust state to
peers for ...`; the key copy is unlinked.

## Step 5 — observe the burn and record the facts

Each node's supervision surface (journald for the systemd fleet) crosses
`WAITING_FOR_BURN_GENESIS` and settles; the observable proof of the
ceremony's outcome, all consumed values (never recomputed):

1. Every node's latest `STATUS node_state=` is beyond
   `WAITING_FOR_BURN_GENESIS`.
2. Every node's `head=` is the SAME value across all nodes — that value is
   the genesis CID; record it.
3. `sha256(manifest.bin)` == the step-1 fingerprint; record both.
4. Each node logged the staging topic appendix `.3.7.333` and never the DEV
   `.3.7.144` — the net pin proof.

Those observed values (fingerprint, genesis CID, the genesis multiaddr, the
trust addresses) are exactly what `staging_artifact.py` publishes as
`deploy/staging-net/` — the artifact set is the record of THIS ceremony, and
a re-ceremony is a documented reset, never a silent retry.
