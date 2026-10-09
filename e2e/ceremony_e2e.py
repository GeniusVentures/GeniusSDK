#!/usr/bin/env python3
"""
ceremony_e2e.py — the single e2e entry command (D-16) for the local ceremony
harness.

--boot-only: boot N real node processes through the one GeniusSDKService
runner with generated identities, unique >300-spaced ports, and an isolated
reserved-band network id (the plan 02-02 proof, unchanged). Default mode: the
full Variant B ceremony on top of that boot — make-manifest (determinism +
sha256 verified) -> bootstrapper genesis (fingerprint echoed on stdin) ->
per-peer list + approve to the burn floor -> every node must leave
WAITING_FOR_BURN_GENESIS inside the serve window + grace -> per-actor list
readback -> teardown -> D-07 key-absence scan -> D-17 keep/delete. All
assertions ride the D-11 surfaces only: CLI exit codes, pinned stdout
strings, STATUS lines, and file hashes — no RPC, no shared DBs (D-10).

Exit-code contract:
  0  full success (run dir deleted; or preserved with --keep-dir)
  2  stranded genesis detected — ALWAYS accompanied by the distinct
     STRANDED-GENESIS-DETECTED marker line as the first line of the stranded
     diagnostics (argparse usage errors also exit 2; the marker is what
     disambiguates a genuine detection from a usage/parse error)
  1  any other failure (gate timeouts, FATAL_TRUST_MISMATCH, key leakage,
     crashed steps — and SCENARIO failures; scenarios never invent new codes,
     the stranded negative control alone stays exit 2)

Scenario surface (Phase 3, D-09 + Phase 4 dht-health): --scenario
restart-mid-ceremony | restart-mid-sync | quorum-loss | window-edge |
dht-health; default None is the Phase 2 happy path, completely unchanged.
Mapping note (D-09 locks the four-token surface): restart-mid-ceremony
covers BOTH SCEN-03 kill point 1 (SIGKILL right after the 2nd of 4 peer
approves returns — below the parsed burn floor, mid-ceremony) and kill
point 2 (SIGKILL right after the FINAL approve returns — post-quorum,
pre-start); restart-mid-sync is SCEN-03 kill point 3 (SIGKILL a real
6th-process joiner the instant its own Blockchain-logger sync anchor
prints, then recover to READY at the network head); quorum-loss is SCEN-04
(2-of-5 SIGKILL post-READY: survivors must hold the frozen head with
stable READY states and no FATAL, then all 5 respawned nodes converge
back to the SAME head); window-edge is SCEN-02 (a 45s serve window on
every ceremony step: joiner1 joins while the window is open and its sync
anchor must beat the expiry line — the WINDOW-EDGE-ORDERING assertion;
joiner2 joins strictly after expiry and must sync via network state, the
documented D-01/D-04 contract asserted against the ceremony owner's help
text; the genesis + approvals run in worker threads so both joins happen
while the actors still serve). dht-health (04-01, D-01 amended) proves
the fleet-risky assumptions on ONE machine before any box is touched: a
5-node Variant-B ceremony with a SPLIT bootstrap (nodes 2-3 -> node1,
nodes 4-5 -> node2) where node2 dials node1 through a /dns4/ multiaddr
derived from the live capture (the dns4 dial proof), node3 must learn
node4-or-node5 via the per-net DHT CID (discovery beyond the configured
bootstrap), every node must log the byte-identical "CID Test::" CID, and
a co-located foreign-net node (own base dir, own registry net id, EMPTY
bootstrap) must stay alive with a DIFFERENT CID and zero cross-
contamination — the AWS-box co-location pattern at zero spend. Flags:
--network-id (default: reserved-band draw) and --coexist-net-id (the
foreign node's net registry id; default 144 beside an explicit 333 run,
else 963 — the C++ registry rejects the whole reserved band for net_id,
so a band draw cannot be the foreign net). NET-05 integrity: the ONLY
sanctioned non-reserved run is the explicit "--scenario dht-health
--network-id 333" pre-provision proof; every default draw stays in the
reserved band, and CI never pins 333.
Standing assertion:
scenario builds NEVER define SGNS_USE_MEMORY_SECURE_STORAGE — durable
secure storage is the whole point of the restart proof (identity and
approvals must survive SIGKILL via durable state; the memory backend would
make identity ephemeral per process and silently void every restart
proof).

Usage:
  python3 e2e/ceremony_e2e.py --nodes 5 --runner /abs/GeniusSDKService --sgns-trust /abs/sgns-trust
  python3 e2e/ceremony_e2e.py --nodes 5 --boot-only --runner /abs/GeniusSDKService
  python3 e2e/ceremony_e2e.py --nodes 5 --approve-peers 2 --runner ... --sgns-trust ...
      (below-floor negative control: must exit 2 with the marker, artifacts
       preserved scrubbed)
Env fallbacks: SGNS_RUNNER_BIN, SGNS_TRUST_BIN.
"""

import argparse
import os
import re
import shutil
import signal
import sys
import tempfile
import threading
import time

import ceremony
import supervisor
import topology  # imports secp256k1_address: pinned vector asserts at import
import secp256k1_address

# The staged bootstrap gate: the bootstrapper's live PubSub address, including
# the /p2p/<peer-id> suffix (GeniusNode.cpp L1671, observed in sgnslog2.log —
# the node logger file-sinks there; topology's log_config promotes it to info).
PUBSUB_LINE = r"PubSub started at address: (\S+)"
# Per-node boot gate: the ceremony waiting state, with the loud fail-fast.
WAITING_OR_FATAL = r"STATUS node_state=(WAITING_FOR_TRUST_GENESIS|FATAL_TRUST_MISMATCH)"
ACCOUNT_ADDRESS_RE = r"^([0-9a-f]{128})$"

# Post-burn economic-readiness states (02-RESEARCH open question 4 resolution:
# the phase stops at economic readiness, full READY is not required).
POST_BURN_STATES = ("INITIALIZING_TRANSACTIONS", "INITIALIZING_PROCESSING", "READY")
# Bounded post-quorum wait grace (02-02 observed boot-to-WAITING at 12-14s per
# node on a 10s STATUS cadence — generous headroom). Added to the serve value
# in use by _post_burn_window — the one home for that arithmetic.
POST_BURN_GRACE_SECONDS = 120
# Bounded head-sampling window after the strict gate: head= rides the SAME
# STATUS line as READY, but a node still in INITIALIZING_TRANSACTIONS when the
# gate passes needs its READY line first — 60s covers the 10s cadence plus one
# transition (Pitfall 7 headroom discipline).
HEAD_SAMPLE_SECONDS = 60
# SCEN-03 kill point 3 anchor: the joiner's own Blockchain-logger line
# (Blockchain.cpp L703, info — observable only because topology's log_config
# promotes the Blockchain logger; Pitfall 2). 120s gate = the 12-14s boot
# plus registry fetch and sync, with Pitfall 7 headroom.
JOINER_SYNC_ANCHOR = "Request succeeded for Genesis"
JOINER_GATE_SECONDS = 120
# SCEN-04 quorum-loss windows: the bounded outage observation (sample every
# supervisor POLL_INTERVAL across 60s — polling, never sleeping) and the
# recovery gate (respawned nodes re-READY from durable state; 180s = the
# 12-14s boot plus registry re-fetch, with Pitfall 7 headroom).
QUORUM_OBSERVATION_SECONDS = 60
QUORUM_RECOVERY_SECONDS = 180
# SCEN-02 window-edge (D-01..D-04): the explicit short serve window every
# ceremony step passes (D-03 band 30-60s; 45 chosen for headroom on BOTH
# boundary sides — the tool's 600s default is untouched). Pinned
# observables: the tool buffers its stdout milestone lines (only the
# interactive prompts flush — source-verified GenesisCeremony.cpp), so the
# in-window spawn anchor is node1's WAITING_FOR_BURN_GENESIS STATUS
# transition (node1 fetched the genesis DAG: the window is open and
# actively serving) and the expiry line becomes visible exactly at actor1's
# exit flush — the moment the window closed.
WINDOW_EDGE_SERVE_SECONDS = 45
GENESIS_WINDOW_EXPIRY_LINE = "Genesis serving window complete."
NODE_BURN_WAIT_STATE = "WAITING_FOR_BURN_GENESIS"

# dht-health (D-01 amended) observables — verified against the built
# libraries: "CID Test:: <cid>" rides the node logger (SuperGeniusNode,
# libgenius_node.a), "DHT: New Peer: <pid>" rides the GossipPubSub logger
# (libipfs-pubsub.a — observable only because topology's INFO_LOGGERS
# promotes it to info).
CID_TEST_LINE = r"CID Test:: (\S+)"
DHT_NEW_PEER_PREFIX = "DHT: New Peer: "
# The dns4 dial proof hostname: dns4 resolves A records only, and the node
# binds 0.0.0.0, so localhost -> 127.0.0.1 reaches node1's listener.
DNS4_PROBE_HOSTNAME = "localhost"
# The foreign node's default net: beside an explicit 333 run the pairing is
# DEV (144 — the plan's proof pair); beside a default reserved-band run the
# staging nodes sit on the silent DEV default, so the foreign pins TEST
# (963). Both are registry-accepted in every binary (pre- and post-D-06) —
# a reserved-band draw is NOT usable as a foreign net (the C++ registry
# rejects the whole band for net_id, the same accept-list fact the RED run
# proves live; recorded as the 04-01 plan deviation).
FOREIGN_NET_BESIDE_STAGING = 144
FOREIGN_NET_BESIDE_DEFAULT = 963

STRANDED_MARKER = "STRANDED-GENESIS-DETECTED"


class _Tee:
    """Console + <run_dir>/harness.out — the harness's own output is in D-07
    scan scope, so it is materialized in the run dir as it is printed."""

    def __init__(self, console, file_handle):
        self.console = console
        self.file = file_handle

    def write(self, data):
        self.console.write(data)
        self.file.write(data.encode("utf-8"))

    def flush(self):
        self.console.flush()
        self.file.flush()


def _spawn_node(name, runner, entry, run_dir, spawn_times):
    supervisor.spawn(
        name,
        [runner, entry["base_dir"], "--key-file", entry["key_file"]],
        run_dir,
        extra_logs=[entry["node_log"]],
    )
    spawn_times[name] = time.monotonic()


def _spawn_joiner(name, runner, entry, run_dir, spawn_times):
    """Joiner argv shape: base dir only — NO --key-file. A late joiner boots
    a fresh account (exactly the Phase 5 shape); no joiner key exists to
    scrub, so the D-07 scan scope stays the topology node keys."""
    supervisor.spawn(
        name,
        [runner, entry["base_dir"]],
        run_dir,
        extra_logs=[entry["node_log"]],
    )
    spawn_times[name] = time.monotonic()


def _read_account_address(node_entry):
    """B_1: the bootstrapper's SDK account address from secure_storage_id
    (KDF-derived, never equal to A_1 — research Fact 5; do not derive it)."""
    path = os.path.join(node_entry["base_dir"], "secure_storage_id")
    with open(path) as handle:
        for line in handle:
            match = re.match(ACCOUNT_ADDRESS_RE, line.strip())
            if match:
                return match.group(1)
    raise RuntimeError("no 128-hex account address in %s" % path)


def _wait_for_file(path, timeout):
    deadline = time.monotonic() + timeout
    while not os.path.exists(path):
        if time.monotonic() >= deadline:
            raise RuntimeError("timeout after %ss waiting for %s"
                               % (timeout, path))
        time.sleep(supervisor.POLL_INTERVAL)


# The tail of an observed PubSub multiaddr: /tcp/<port>/ipfs/<peer id> (the
# built binary emits the /ipfs/ suffix form; /p2p/ is equivalent input).
_MULTIADDR_TAIL_RE = re.compile(r"/tcp/(\d+)/(?:ipfs|p2p)/([^/\s]+)\s*$")


def _multiaddr_tcp_peer(multiaddr):
    """(tcp port, peer id) parsed from a live PubSub multiaddr capture — the
    dns4 gate DERIVES its entry from the observation, never hand-transcribes
    (the same parse-don't-recompute discipline as every other gate)."""
    match = _MULTIADDR_TAIL_RE.search(multiaddr)
    if not match:
        raise RuntimeError("unparseable multiaddr (expected .../tcp/<port>/"
                           "ipfs|p2p/<peer id>): %r" % multiaddr)
    return int(match.group(1)), match.group(2)


def _start_thread(name, work):
    """Run work() in a named worker thread; its return value lands in
    box["result"] and its first exception in box["error"], for the main
    thread to collect at join time (a worker failure must fail the scenario
    exactly like an inline one). Non-daemon: an interrupted main flow still
    waits behind the bounded actor drivers — a running sgns-trust process is
    never silently orphaned."""
    box = {"error": None, "result": None, "thread": None}

    def guarded():
        try:
            box["result"] = work()
        except BaseException as error:  # same boundary discipline as main
            box["error"] = error

    thread = threading.Thread(target=guarded, name=name)
    thread.start()
    box["thread"] = thread
    return box


def _join_box(box, what):
    """Join a _start_thread worker; re-raise its captured failure and return
    its captured result."""
    box["thread"].join()
    if box["error"] is not None:
        raise RuntimeError("%s failed in its worker thread: %s"
                           % (what, box["error"])) from box["error"]
    return box["result"]


def _wait_actor_line(out_path, line, timeout, source):
    """Bounded gate on one sgns-trust actor's stdout capture file. Actors are
    not supervisor processes (their drivers own the lifecycle); the sink is a
    plain file the tool flushes only at process exit, so this gate suits
    exit-time anchors like the serve-window expiry line."""
    deadline = time.monotonic() + timeout
    while True:
        if os.path.exists(out_path):
            with open(out_path, encoding="utf-8", errors="replace") as handle:
                if line in handle.read():
                    return
        if time.monotonic() >= deadline:
            raise RuntimeError("timeout after %ss waiting for %s's stdout "
                               "capture to contain %r (%s)"
                               % (timeout, source, line, out_path))
        time.sleep(supervisor.POLL_INTERVAL)


def _post_burn_window(serve_seconds):
    """The strict post-burn gate deadline: the actors' serve window plus
    catch-up grace. One home for the arithmetic — every caller derives its
    window from the serve value actually passed to the ceremony drivers, so a
    short-window scenario (window-edge) never carries a second constant."""
    return serve_seconds + POST_BURN_GRACE_SECONDS


def _sample_heads(names, window_seconds, what):
    """Bounded head sampling (Pitfall 7 headroom discipline): poll head_of per
    name until every name has shown a STATUS head=, or the window elapses.
    Returns {name: head}; raises naming the missing names otherwise."""
    deadline = time.monotonic() + window_seconds
    while True:
        heads = {name: supervisor.head_of(name) for name in names}
        if all(heads.values()) or time.monotonic() >= deadline:
            break
        time.sleep(1.0)
    missing = sorted(name for name, head in heads.items() if not head)
    if missing:
        raise RuntimeError("no STATUS head= observed within %ds for %s (%s)"
                           % (window_seconds, missing, what))
    return heads


def _gate_joiner_ready_at_head(name, network_head, context):
    """Gate one joiner: READY within JOINER_GATE_SECONDS, then a STATUS head=
    sampled within HEAD_SAMPLE_SECONDS that must equal the network head — a
    joiner diverging from the net head is always a loud failure. Returns the
    joiner's head."""
    supervisor.wait_for(name, r"STATUS node_state=READY\b", JOINER_GATE_SECONDS)
    head = _sample_heads([name], HEAD_SAMPLE_SECONDS, context)[name]
    if supervisor.state_of(name) != "READY":
        raise RuntimeError("%s latest state %s (expected READY) (%s)"
                           % (name, supervisor.state_of(name), context))
    if head != network_head:
        raise RuntimeError("%s head %s != network head %s (%s)"
                           % (name, head, network_head, context))
    return head


def _identity_boot(runner, node1, run_dir, timeout):
    """Boot node 1 bare once so B_1 lands in secure_storage_id, then SIGTERM.

    sgns_config.json needs authorized_full_node == B_1 (the account address
    that writes the genesis validator registry — without it blockchain start
    defers forever and the node never reaches WAITING_FOR_TRUST_GENESIS), and
    B_1 exists only after the account is created. Account and libp2p identity
    persist under base_dir, so the final boot keeps B_1 and a stable
    multiaddr (research Variant A identity facts).
    """
    _spawn_node("node1-identity", runner, node1, run_dir, {})
    supervisor.wait_for("node1-identity", PUBSUB_LINE, timeout)
    _wait_for_file(os.path.join(node1["base_dir"], "secure_storage_id"), timeout)
    account_address = _read_account_address(node1)
    print("IDENTITY: node1 account address %s" % account_address)
    exit_code = supervisor.terminate("node1-identity", timeout=60)
    if exit_code != 0:
        raise RuntimeError("node1 identity boot exited %s on SIGTERM"
                           % exit_code)
    return account_address


def _check_fatal(top):
    """FATAL_TRUST_MISMATCH anywhere = instant loud failure (trust wiring
    conflict is never the stranded class — it is an unexpected failure)."""
    for entry in top["nodes"]:
        state = supervisor.state_of(entry["name"])
        if state == "FATAL_TRUST_MISMATCH":
            raise RuntimeError("network_id=%d: %s reached FATAL_TRUST_MISMATCH "
                               "(trust wiring conflict — see %s)"
                               % (top["network_id"], entry["name"],
                                  entry["node_log"]))


def _boot_node1(runner, run_dir, top, timeout):
    """IDENTITY -> trust configs -> node1 spawn + PubSub gate. The shared
    prefix of every boot shape (extracted 04-01, behavior-preserving):
    returns (spawn_times, node1 multiaddr) with the manifest stashes
    (node1_multiaddr, authorized_full_node) already set."""
    node1 = top["nodes"][0]
    # Staging runs pin the identity boot to the SAME net-scoped path the
    # final boot will use (the libp2p keypair persists there — without the
    # pin the final boot answers as a different peer id than the one this
    # boot's captured multiaddr advertises; no-op for reserved-band runs).
    topology.write_identity_config(top)
    account_address = _identity_boot(runner, node1, run_dir, timeout)
    topology.write_trust_configs(top, account_address)
    print("IDENTITY: trust configs written (authorized_full_node = node1 "
          "account address)")

    spawn_times = {}
    _spawn_node(node1["name"], runner, node1, run_dir, spawn_times)
    print("START: %s spawned (base %s, empty bootstrap list)"
          % (node1["name"], node1["base_dir"]))
    multiaddr = supervisor.wait_for(node1["name"], PUBSUB_LINE, timeout).group(1)
    print("GATE: %s PubSub at %s (%.1fs)"
          % (node1["name"], multiaddr, time.monotonic() - spawn_times[node1["name"]]))
    # Only now is the multiaddr knowable: peers' and actors' configs get it.
    # Stashed on the manifest (the single source every later stage reads) so
    # joiner configs can consume the same facts (write_joiner_configs):
    # B_1 too — a joiner needs the authorized creator to accept the genesis.
    top["node1_multiaddr"] = multiaddr
    top["authorized_full_node"] = account_address
    return spawn_times, multiaddr


def _gate_nodes_waiting(top, spawn_times, timeout):
    """Per-node WAITING_FOR_TRUST_GENESIS gates (the shared suffix of every
    boot shape): returns per-node spawn->gate timings; FATAL_TRUST_MISMATCH
    anywhere is the instant loud failure."""
    timings = {}
    for entry in top["nodes"]:
        match = supervisor.wait_for(entry["name"], WAITING_OR_FATAL, timeout)
        state = match.group(1)
        if state == "FATAL_TRUST_MISMATCH":
            raise RuntimeError("%s reached FATAL_TRUST_MISMATCH (trust wiring "
                               "conflict — see %s)"
                               % (entry["name"], entry["node_log"]))
        timings[entry["name"]] = time.monotonic() - spawn_times[entry["name"]]
        print("GATE: %s node_state=%s after %.1fs"
              % (entry["name"], state, timings[entry["name"]]))
    return timings


def _boot_stage(runner, run_dir, top, timeout):
    """IDENTITY -> START -> per-node WAITING_FOR_TRUST_GENESIS gates — the
    02-02 boot path, unchanged; returns per-node spawn->gate timings."""
    spawn_times, multiaddr = _boot_node1(runner, run_dir, top, timeout)
    topology.write_peer_configs(top, multiaddr)
    for entry in top["nodes"][1:]:
        _spawn_node(entry["name"], runner, entry, run_dir, spawn_times)
        print("START: %s spawned (bootstrap %s)" % (entry["name"], multiaddr))
    return _gate_nodes_waiting(top, spawn_times, timeout)


def _dht_health_boot_stage(runner, run_dir, top, timeout):
    """The dht-health boot (D-01 amended): the proven pipeline with a SPLIT
    bootstrap in two waves and the co-located foreign-net node beside the
    mesh. Wave 1: node1, then node2 whose ONLY bootstrap entry is the
    /dns4/ form of node1's observed multiaddr (port + peer id parsed from
    the capture — never hand-transcribed); node2 meshing proves the dns4
    form resolves at dial time in this binary (research A1, closed
    locally). Wave 2: node3 -> node1, nodes 4-5 -> node2's observed
    multiaddr — node3 is never told about node4/node5, which is what the
    discovery gate proves. The foreign node spawns between the waves (max
    overlap with the mesh + ceremony) under its own base dir, registry
    net, and EMPTY bootstrap. Gates never sleep. Returns the foreign node
    record for the coexistence gate."""
    spawn_times, multiaddr1 = _boot_node1(runner, run_dir, top, timeout)
    top["node_multiaddrs"] = {top["nodes"][0]["name"]: multiaddr1}

    # Wave 1: node2 dials the dns4 form of node1.
    port1, peer1 = _multiaddr_tcp_peer(multiaddr1)
    dns4_entry = topology.make_dns4_multiaddr(DNS4_PROBE_HOSTNAME, port1, peer1)
    node2 = top["nodes"][1]
    topology.write_peer_configs(top, multiaddr1,
                                node_multiaddr_map={node2["name"]: dns4_entry})
    _spawn_node(node2["name"], runner, node2, run_dir, spawn_times)
    print("START: %s spawned (bootstrap %s — the dns4 dial proof)"
          % (node2["name"], dns4_entry))
    multiaddr2 = supervisor.wait_for(node2["name"], PUBSUB_LINE,
                                     timeout).group(1)
    top["node_multiaddrs"][node2["name"]] = multiaddr2

    # The co-located foreign-net node: different registry net, empty
    # bootstrap, next port band slot, no --key-file (a fresh foreign
    # account — it is not a ceremony participant).
    foreign_entry = topology.write_foreign_net_configs(run_dir,
                                                       top["coexist_net_id"])
    foreign_proc = supervisor.spawn(
        foreign_entry["name"], [runner, foreign_entry["base_dir"]], run_dir,
        extra_logs=[foreign_entry["node_log"]])
    foreign_multiaddr = supervisor.wait_for(foreign_entry["name"], PUBSUB_LINE,
                                            timeout).group(1)
    _, foreign_peer_id = _multiaddr_tcp_peer(foreign_multiaddr)
    print("START: foreign-net spawned (net_id %d, empty bootstrap, base %s) — "
          "the co-location proof's other tenant"
          % (foreign_entry["net_id"], foreign_entry["base_dir"]))

    # Wave 2: node3 -> node1; nodes 4-5 -> node2. Node2's dns4 entry rides
    # the map again (write_peer_configs rewrites every peer config; its
    # boot-time read already happened, the rewrite is inert for it).
    node3, node4, node5 = top["nodes"][2], top["nodes"][3], top["nodes"][4]
    topology.write_peer_configs(top, multiaddr1, node_multiaddr_map={
        node2["name"]: dns4_entry,
        node4["name"]: multiaddr2,
        node5["name"]: multiaddr2,
    })
    bootstrap_of = {node3["name"]: multiaddr1,
                    node4["name"]: multiaddr2,
                    node5["name"]: multiaddr2}
    for entry in (node3, node4, node5):
        _spawn_node(entry["name"], runner, entry, run_dir, spawn_times)
        print("START: %s spawned (bootstrap %s)"
              % (entry["name"], bootstrap_of[entry["name"]]))
    for entry in (node3, node4, node5):
        top["node_multiaddrs"][entry["name"]] = supervisor.wait_for(
            entry["name"], PUBSUB_LINE, timeout).group(1)

    timings = _gate_nodes_waiting(top, spawn_times, timeout)
    print("DHT-GATE: dns4-dial ok (%s meshed via %s — WAITING_FOR_TRUST_"
          "GENESIS reached through the /dns4/ bootstrap)"
          % (node2["name"], dns4_entry))
    return {"entry": foreign_entry, "proc": foreign_proc,
            "peer_id": foreign_peer_id, "boot_timings": timings}


def _dht_health_mesh_gates(top, timeout):
    """Post-boot DHT gates over the per-net CID: every staging node must
    have logged a byte-identical "CID Test::" value (same net-scoped
    provide key), and node3 — which was only ever told about node1 — must
    have learned node4 or node5 through "DHT: New Peer:" (discovery beyond
    the configured bootstrap, the D-05 fact this gate exercises)."""
    cids = {}
    for entry in top["nodes"]:
        cids[entry["name"]] = supervisor.wait_for(
            entry["name"], CID_TEST_LINE, timeout).group(1)
    distinct = set(cids.values())
    if len(distinct) != 1:
        raise RuntimeError("DHT CID divergence across staging nodes (expected "
                           "one identical per-net CID): %s" % cids)
    staging_cid = next(iter(distinct))
    print("DHT-GATE: identical-cid ok (%s)" % staging_cid)
    top["dht_staging_cid"] = staging_cid

    node3 = top["nodes"][2]
    discoverable = []
    for candidate in top["nodes"][3:5]:  # node4, node5 — never in node3's config
        _, peer_id = _multiaddr_tcp_peer(top["node_multiaddrs"][candidate["name"]])
        discoverable.append(peer_id)
    pattern = "%s(%s)" % (re.escape(DHT_NEW_PEER_PREFIX),
                          "|".join(re.escape(pid) for pid in discoverable))
    learned = supervisor.wait_for(node3["name"], pattern, timeout).group(1)
    print("DHT-GATE: discovery-beyond-bootstrap ok (node3 learned %s)"
          % learned)


def _dht_health_coexistence_gate(top, foreign, timeout):
    """The coexistence verdict (run after the ceremony): the foreign node
    logged a DIFFERENT per-net CID (isolation, not silence — it runs its own
    DHT), is still alive (co-location means co-existing), and NO staging
    node ever learned its peer id through "DHT: New Peer:" (zero
    cross-contamination — the AWS-box pattern's whole claim)."""
    entry = foreign["entry"]
    foreign_cid = supervisor.wait_for(entry["name"], CID_TEST_LINE,
                                      timeout).group(1)
    staging_cid = top["dht_staging_cid"]
    if foreign_cid == staging_cid:
        raise RuntimeError("foreign-net node (net_id %d) logged the SAME CID "
                           "%s as the staging net — per-net CID isolation "
                           "failed (net ids collided?)"
                           % (entry["net_id"], foreign_cid))
    if foreign["proc"].poll() is not None:
        raise RuntimeError("foreign-net node exited %s before the ceremony "
                           "completed (co-location means co-existing; see %s)"
                           % (foreign["proc"].returncode, entry["node_log"]))
    contamination = []
    needle = "%s%s" % (DHT_NEW_PEER_PREFIX, foreign["peer_id"])
    for node_entry in top["nodes"]:
        with open(node_entry["node_log"], encoding="utf-8",
                  errors="replace") as handle:
            if needle in handle.read():
                contamination.append(node_entry["name"])
    if contamination:
        raise RuntimeError("cross-net contamination: %s learned the foreign "
                           "node's peer id via DHT (see their sgnslog2.log)"
                           % ", ".join(contamination))
    print("DHT-GATE: coexistence-isolation ok (foreign cid %s)" % foreign_cid)


def _teardown_nodes(expect_clean):
    """SIGTERM every node (SIGKILL escalation on timeout); returns exit codes.
    expect_clean: success paths require exit 0 everywhere; the stranded path
    prints a warning instead (the stranded outcome is already the verdict)."""
    print("TEARDOWN: SIGTERM all nodes (SIGKILL escalation on timeout)")
    exit_codes = supervisor.teardown(timeout=60)
    for name in sorted(exit_codes):
        print("EXIT: %s code %s" % (name, exit_codes[name]))
    unclean = {n: c for n, c in exit_codes.items() if c != 0}
    if unclean and expect_clean:
        raise RuntimeError("unclean node exits (a SIGKILLed node is a loud "
                           "failure): %s" % unclean)
    if unclean:
        print("WARNING: unclean node exits on the stranded path: %s" % unclean)


def _kill_and_respawn(runner, entry, run_dir):
    """D-06 real-crash injection at an event anchor: SIGKILL the node, then
    respawn it under a DISTINCT supervision name (Pattern 2: spawn opens
    captures "wb" and the node truncates sgnslog2.log per process open, so a
    same-name respawn would erase the pre-kill evidence — post-restart gates
    anchor only on post-restart captures). entry["name"] becomes the latest
    name, so every later gate (FATAL checks, the strict post-burn gate, the
    head assertion, and teardown via the supervisor's own records) follows
    the respawned process."""
    old_name = entry["name"]
    supervisor.kill(old_name)
    new_name = old_name + "-restart"
    _spawn_node(new_name, runner, entry, run_dir, {})
    entry["name"] = new_name
    print("SCENARIO: %s SIGKILLed at the approve anchor, respawned as %s "
          "(same argv + base dir — durable-state recovery is the proof)"
          % (old_name, new_name))


def _approve_sequence(sgns_trust, runner, run_dir, top, manifest,
                      approve_count, scenario, serve_seconds):
    """The Variant B approval loop: per-peer list -> first burn candidate ->
    approve, with the restart-mid-ceremony kill points riding the same loop
    (event-anchored on each approve driver RETURNING — zero sleeps). Returns
    (approved addresses, killed_actor_name, killed_candidate_id). Runs inline
    on the blocking-genesis path; window-edge runs it in a worker thread
    concurrent with actor1's serve window (the burn must land inside the
    window for the in-window joiner to sync before expiry)."""
    peer_actors = top["actors"][1:]
    peer_addresses = top["peer_set"]  # peer_actors[i] signs as peer_set[i]
    approved = []
    killed_actor_name = None
    killed_candidate_id = None
    for actor, address in zip(peer_actors[:approve_count], peer_addresses):
        candidates = ceremony.list_candidates(sgns_trust, actor,
                                              manifest["manifest"], run_dir)
        burn_ids = [cid for kind, cid in candidates if kind == "burn"]
        if not burn_ids:
            raise ceremony.CeremonyError(
                "%s listed %d candidate(s) but no burn candidate (content-"
                "gated): %s" % (actor["name"], len(candidates), candidates))
        candidate_id = burn_ids[0]
        print("CEREMONY: %s listed burn candidate %s" % (actor["name"], candidate_id))
        ceremony.approve(sgns_trust, actor, manifest["manifest"], candidate_id,
                         run_dir, serve_seconds=serve_seconds)
        approved.append(address)
        print("CEREMONY: %s (%s) approved %s (%d of %d peers; burn threshold %d)"
              % (actor["name"], address, candidate_id, len(approved),
                 len(peer_actors), manifest["burn_threshold"]))
        if scenario == "restart-mid-ceremony":
            # peer_actors[i]'s node is top["nodes"][i+1]: the just-approving
            # actor's own node. Event-anchored (D-07): the approve driver
            # RETURNING is the anchor — no sleeps anywhere.
            if len(approved) == manifest["burn_threshold"] - 1:
                # SCEN-03 kill point 1 (mid-ceremony): this approve returned
                # with k below the parsed burn floor. SIGKILL the approver's
                # node; the remaining serve windows (>= 3 min) are its rejoin
                # cover.
                killed_actor_name = actor["name"]
                killed_candidate_id = candidate_id
                _kill_and_respawn(runner, top["nodes"][len(approved)], run_dir)
            elif len(approved) == len(peer_actors) and killed_actor_name:
                # SCEN-03 kill point 2 (post-quorum pre-start): the FINAL
                # approve returned; the network is starting. SIGKILL an
                # untouched survivor (node2, the first peer).
                _kill_and_respawn(runner, top["nodes"][1], run_dir)
        _check_fatal(top)
    return approved, killed_actor_name, killed_candidate_id


def _restart_mid_sync_stage(runner, run_dir, top, network_head):
    """SCEN-03 kill point 3: SIGKILL a real syncing joiner mid-sync, then
    prove it recovers to the network head.

    The joiner is a genuine 6th process (never exercised in Phase 2 — A3):
    configs from topology's one config-writer home, spawned WITHOUT
    --key-file, gated on its OWN Blockchain-logger sync anchor (the log_config
    promotion is what makes the line observable — Pitfall 2). The kill fires
    the instant the anchor matches (event-anchored, D-07); the respawn gets a
    distinct name (Pattern 2 — fresh captures, post-restart gates anchor only
    on post-restart content). The ceremony net must be unaffected: all N
    nodes still READY at the final gate."""
    if network_head is None:
        raise RuntimeError("restart-mid-sync requires the strict pass (no "
                           "identical network head was asserted)")
    joiner = topology.write_joiner_configs(
        top, top["node1_multiaddr"], top["authorized_full_node"], count=1)[0]
    print("SCENARIO: joiner configs written (base %s, subnet_id == network_id, "
          "bootstrap node1 of THIS run — T-03-05)" % joiner["base_dir"])
    _spawn_joiner("joiner", runner, joiner, run_dir, {})
    print("SCENARIO: joiner spawned (no --key-file — Phase 5 joiner shape)")
    supervisor.wait_for("joiner", JOINER_SYNC_ANCHOR, JOINER_GATE_SECONDS)
    print("SCENARIO: joiner sync anchor observed ('%s' — the Blockchain-"
          "logger line)" % JOINER_SYNC_ANCHOR)
    supervisor.kill("joiner")
    _spawn_joiner("joiner-restart", runner, joiner, run_dir, {})
    joiner["name"] = "joiner-restart"
    print("SCENARIO: joiner SIGKILLed at the sync anchor, respawned as "
          "joiner-restart (durable-state recovery is the proof)")
    joiner_head = _gate_joiner_ready_at_head(
        "joiner-restart", network_head, "the respawned joiner diverged")
    print("SCENARIO: joiner-restart READY with head %s (== network head — "
          "mid-sync SIGKILL recovered)" % joiner_head)
    for entry in top["nodes"]:
        state = supervisor.state_of(entry["name"])
        if state != "READY":
            raise RuntimeError("%s state %s after the joiner crash (the "
                               "ceremony net must be unaffected — see %s)"
                               % (entry["name"], state, entry["node_log"]))
    print("SCENARIO: all %d ceremony nodes still READY (net unaffected by the "
          "joiner crash)" % len(top["nodes"]))


def _quorum_loss_stage(runner, run_dir, top, network_head):
    """SCEN-04: 2-of-5 quorum loss post-READY — halt with no divergence among
    survivors, then recovery converging every node back to the SAME head.

    Ratified assertion strength (03-RESEARCH Open Question 3, adopted): on a
    quiescent net halt is evidenced as frozen head + stable READY states + no
    FATAL across survivors — the limitation line is printed in the OUTPUT, not
    buried in comments. The kills are event-anchored on the identical-head
    assertion RETURNING (D-07); the observation window polls the supervisor's
    cadence; any violation fails loudly naming node, sample, and value."""
    if network_head is None:
        raise RuntimeError("quorum-loss requires the strict pass (no "
                           "identical network head was asserted)")
    baseline = network_head
    # Kill node3 + node5 — two of the four trusted peers; survivors are node1
    # (the bootstrapper), node2, node4. 3 live of 5 < every quorum floor.
    killed_indexes = (2, 4)  # 0-based positions of node3, node5
    for index in killed_indexes:
        entry = top["nodes"][index]
        supervisor.kill(entry["name"])
        print("SCENARIO: %s SIGKILLed post-READY at the head-assertion anchor "
              "(2-of-5 quorum loss)" % entry["name"])
    survivors = [entry for index, entry in enumerate(top["nodes"])
                 if index not in killed_indexes]
    survivor_names = [entry["name"] for entry in survivors]

    samples = 0
    deadline = time.monotonic() + QUORUM_OBSERVATION_SECONDS
    while True:
        samples += 1
        for entry in survivors:
            name = entry["name"]
            state = supervisor.state_of(name)
            head = supervisor.head_of(name)
            if state is not None and "FATAL" in state:
                raise RuntimeError("outage sample %d: %s reached %s — no FATAL "
                                   "is allowed across survivors (see %s)"
                                   % (samples, name, state, entry["node_log"]))
            if state != "READY":
                raise RuntimeError("outage sample %d: %s state=%s — halt means "
                                   "STABLE states, expected READY (see %s)"
                                   % (samples, name, state, entry["node_log"]))
            if head != baseline:
                raise RuntimeError("outage sample %d: %s head=%s != frozen "
                                   "baseline %s — divergence during the outage"
                                   % (samples, name, head, baseline))
        if time.monotonic() >= deadline:
            break
        time.sleep(supervisor.POLL_INTERVAL)
    if samples < 3:
        raise RuntimeError("observation window produced only %d samples "
                           "(needs >= 3 spread across the window)" % samples)
    print("SCENARIO: outage window %ds observed — %d samples across %d "
          "survivors (%s): head frozen, states stable, no FATAL"
          % (QUORUM_OBSERVATION_SECONDS, samples, len(survivors),
             ", ".join(survivor_names)))
    print("halt assertion: quiescent net — head == genesis CID; halt evidenced "
          "as frozen head + stable READY + no FATAL across survivors; "
          "transaction-driven fork detection is out of scope this phase "
          "(phase decisions)")
    print("HEAD-FROZEN %s across survivors" % baseline)

    # Recovery: respawn both killed nodes under DISTINCT names (Pattern 2),
    # same argv/base dir — durable-state recovery is the proof.
    for index in killed_indexes:
        entry = top["nodes"][index]
        new_name = entry["name"] + "-restart"
        _spawn_node(new_name, runner, entry, run_dir, {})
        print("SCENARIO: %s respawned as %s (same argv + base dir — durable-"
              "state recovery)" % (entry["name"], new_name))
        entry["name"] = new_name

    deadline = time.monotonic() + QUORUM_RECOVERY_SECONDS
    while True:
        states = {entry["name"]: supervisor.state_of(entry["name"])
                  for entry in top["nodes"]}
        if all(state == "READY" for state in states.values()):
            break
        fatal = sorted(name for name, state in states.items()
                       if state is not None and "FATAL" in state)
        if fatal:
            raise RuntimeError("recovery gate: %s reached FATAL (see the "
                               "respawned captures)" % ", ".join(fatal))
        if time.monotonic() >= deadline:
            raise RuntimeError("recovery gate: not all %d nodes READY within "
                              "%ds: %s" % (len(top["nodes"]),
                                           QUORUM_RECOVERY_SECONDS, states))
        time.sleep(1.0)

    heads = _sample_heads([entry["name"] for entry in top["nodes"]],
                          HEAD_SAMPLE_SECONDS, "the recovery gate")
    diverged = {name: head for name, head in heads.items() if head != baseline}
    if diverged:
        raise RuntimeError("recovery diverged from the pre-kill head %s: %s "
                           "(durable state must converge to the SAME head — "
                           "no fork across the restart)" % (baseline, diverged))
    print("HEAD-CONVERGED %s across all %d nodes" % (baseline,
                                                     len(top["nodes"])))


def _window_edge_prelude(runner, run_dir, top, sgns_trust, actor1, manifest,
                         approve_count, timeout):
    """SCEN-02 in-window side, armed while actor1's serve window is OPEN.

    Worker thread A runs actor1's genesis (serve WINDOW_EDGE_SERVE_SECONDS)
    to completion; worker thread B runs the approve sequence starting
    immediately — approvals are independent sgns-trust processes whose own
    serve windows overlap actor1's, so the whole ceremony completes while
    the joiners prove both boundary sides. The in-window spawn anchor is
    node1's WAITING_FOR_BURN_GENESIS STATUS transition: node1 fetched the
    genesis DAG, so the window is open and actively serving (actor1's own
    serve lines are stdout-buffered until its exit — not observable
    mid-flight; see the WINDOW_EDGE constants note). joiner1 spawns the
    instant that anchor matches and its Blockchain-logger sync anchor
    ('Request succeeded for Genesis') is gated by watcher thread C, which
    makes the WINDOW-EDGE-ORDERING check AT MATCH TIME: actor1's expiry
    line already visible in its captures then is the violation (both event
    sources named). Observed live: the fresh joiner's blockchain dispatches
    its genesis request well inside the window, before the approve sequence
    even completes."""
    serve_seconds = WINDOW_EDGE_SERVE_SECONDS
    genesis_box = _start_thread(
        "window-edge-genesis",
        lambda: ceremony.genesis(sgns_trust, actor1, manifest["manifest"],
                                 manifest["fingerprint"], run_dir,
                                 serve_seconds=serve_seconds))
    approve_box = _start_thread(
        "window-edge-approves",
        lambda: _approve_sequence(sgns_trust, runner, run_dir, top, manifest,
                                  approve_count, "window-edge", serve_seconds))

    node1 = top["nodes"][0]
    deadline = time.monotonic() + timeout
    while supervisor.state_of(node1["name"]) != NODE_BURN_WAIT_STATE:
        _check_fatal(top)
        if genesis_box["error"] is not None:
            raise RuntimeError("actor1 genesis failed before %s fetched the "
                               "genesis: %s" % (node1["name"],
                                                genesis_box["error"]))
        if approve_box["error"] is not None:
            raise RuntimeError("an approve failed before %s fetched the "
                               "genesis: %s" % (node1["name"],
                                                approve_box["error"]))
        if time.monotonic() >= deadline:
            raise RuntimeError("timeout after %ss waiting for %s to reach %s "
                               "(the genesis DAG never reached node1 — see %s)"
                               % (timeout, node1["name"], NODE_BURN_WAIT_STATE,
                                  node1["node_log"]))
        time.sleep(supervisor.POLL_INTERVAL)
    print("SCENARIO: window open — %s reached %s (the genesis DAG was served; "
          "anchored on node1's STATUS because the tool buffers its own serve "
          "lines until exit)" % (node1["name"], NODE_BURN_WAIT_STATE))

    joiner1, joiner2 = topology.write_joiner_configs(
        top, top["node1_multiaddr"], top["authorized_full_node"], count=2)
    _spawn_joiner(joiner1["name"], runner, joiner1, run_dir, {})
    print("SCENARIO: %s spawned in-window (no --key-file — Phase 5 joiner "
          "shape; bootstrap %s)" % (joiner1["name"], top["node1_multiaddr"]))
    actor1_out = os.path.join(run_dir, actor1["name"] + ".out")
    matched = {"in_window": False}

    def _watch_joiner1_sync():
        supervisor.wait_for(joiner1["name"], JOINER_SYNC_ANCHOR,
                            JOINER_GATE_SECONDS)
        with open(actor1_out, encoding="utf-8", errors="replace") as handle:
            if GENESIS_WINDOW_EXPIRY_LINE in handle.read():
                raise RuntimeError(
                    "WINDOW-EDGE-ORDERING: %s's sync anchor %r matched after "
                    "%s had already printed %r — the in-window joiner missed "
                    "the serve window (event sources: %s and %s; serve %ds)"
                    % (joiner1["name"], JOINER_SYNC_ANCHOR, actor1["name"],
                       GENESIS_WINDOW_EXPIRY_LINE, joiner1["node_log"],
                       actor1_out, serve_seconds))
        matched["in_window"] = True

    watch_box = _start_thread("window-edge-joiner1-watch", _watch_joiner1_sync)
    return {"genesis_box": genesis_box, "approve_box": approve_box,
            "watch_box": watch_box, "matched": matched, "joiner1": joiner1,
            "joiner2": joiner2, "actor1_out": actor1_out,
            "actor1_name": actor1["name"]}


def _window_edge_boundary(window, run_dir, runner, serve_seconds):
    """SCEN-02 boundary crossing: join the ceremony workers, assert the
    expiry anchor, print the in-window verdict, then arm the post-expiry
    side — joiner2, spawned strictly after the expiry line with every
    ceremony actor already exited (no serve transport remains; per the
    documented contract the genesis DAG is now ordinary network state).
    Returns the approve sequence's results for the shared pipeline."""
    _join_box(window["genesis_box"], "actor1 window-edge genesis")
    _wait_actor_line(window["actor1_out"], GENESIS_WINDOW_EXPIRY_LINE,
                     JOINER_GATE_SECONDS, window["actor1_name"])
    _join_box(window["watch_box"], "the joiner1 in-window ordering watch")
    if not window["matched"]["in_window"]:
        raise RuntimeError("joiner1 ordering watch finished without a verdict")
    print("WINDOW-EDGE: in-window joiner synced before expiry")
    approved, killed_actor_name, killed_candidate_id = _join_box(
        window["approve_box"], "the window-edge approve sequence")
    print("SCENARIO: actor1 serve window expired (%r after serve %ds; every "
          "ceremony actor has exited — no serve transport remains)"
          % (GENESIS_WINDOW_EXPIRY_LINE, serve_seconds))
    joiner2 = window["joiner2"]
    _spawn_joiner(joiner2["name"], runner, joiner2, run_dir, {})
    print("SCENARIO: %s spawned strictly after the expiry anchor — per the "
          "documented contract (sgns-trust --help), it must sync from any "
          "holder via network state" % joiner2["name"])
    return approved, killed_actor_name, killed_candidate_id


def _window_edge_joiner_gates(top, window, network_head):
    """SCEN-02 final assertions: joiner1 (in-window) and joiner2 (post-
    expiry) both READY at the network head, then the 7-process convergence
    line — 5 ceremony nodes + both joiners share the identical head."""
    joiner1 = window["joiner1"]
    joiner2 = window["joiner2"]
    _gate_joiner_ready_at_head(joiner1["name"], network_head,
                               "the in-window joiner diverged after sync")
    _gate_joiner_ready_at_head(joiner2["name"], network_head,
                               "the post-expiry joiner never synced via "
                               "network state")
    print("WINDOW-EDGE: post-expiry joiner synced via network state")
    names = [entry["name"] for entry in top["nodes"]]
    names += [joiner1["name"], joiner2["name"]]
    heads = _sample_heads(names, HEAD_SAMPLE_SECONDS,
                          "the 7-process window-edge convergence")
    diverged = {name: head for name, head in heads.items()
                if head != network_head}
    if diverged:
        raise RuntimeError("WINDOW-EDGE divergence from the network head %s: "
                           "%s (all %d processes must converge)"
                           % (network_head, diverged, len(names)))
    print("HEAD %s identical across %d processes (%d ceremony nodes + %s + %s)"
          % (network_head, len(names), len(top["nodes"]), joiner1["name"],
             joiner2["name"]))


def _boot_only_run(runner, run_dir, top, timeout):
    """SETUP -> boot stage -> OBSERVED -> clean teardown; raises on failure."""
    print("SETUP: run dir %s" % run_dir)
    print("SETUP: network_id %d (reserved band %d-%d)"
          % (top["network_id"], *topology.NETWORK_ID_RESERVED_RANGE))
    print("SETUP: bootstrapper %s" % top["bootstrapper"])
    print("SETUP: peer_set %s" % ", ".join(top["peer_set"]))

    _boot_stage(runner, run_dir, top, timeout)

    for entry in top["nodes"]:
        print("OBSERVED: %s node_state=%s"
              % (entry["name"], supervisor.state_of(entry["name"]) or "NONE"))
    _teardown_nodes(expect_clean=True)


def _format_ladder(ladder):
    return "; ".join("%s: %s" % (name, " -> ".join(states) or "(none observed)")
                     for name, states in sorted(ladder.items()))


def _wait_post_burn(top, window):
    """STRICT bounded wait: every node's latest STATUS state must be beyond
    WAITING_FOR_BURN_GENESIS (economic readiness) inside the window.

    Returns (all_advanced, ladder, last) — ladder records each node's observed
    states (in order) from gate start, last is the final per-node state. A
    FATAL_TRUST_MISMATCH observed here fails instantly (not stranded).
    """
    deadline = time.monotonic() + window
    ladder = {entry["name"]: [] for entry in top["nodes"]}
    last = {}
    while True:
        advanced = 0
        for entry in top["nodes"]:
            state = supervisor.state_of(entry["name"])
            if state is None:
                continue
            if state == "FATAL_TRUST_MISMATCH":
                raise RuntimeError("network_id=%d: %s reached "
                                   "FATAL_TRUST_MISMATCH during the post-burn "
                                   "window (see %s)"
                                   % (top["network_id"], entry["name"],
                                      entry["node_log"]))
            if not ladder[entry["name"]] or ladder[entry["name"]][-1] != state:
                ladder[entry["name"]].append(state)
            last[entry["name"]] = state
            if state in POST_BURN_STATES:
                advanced += 1
        if advanced == len(top["nodes"]):
            return True, ladder, last
        if time.monotonic() >= deadline:
            return False, ladder, last
        time.sleep(1.0)


def _ceremony_run(sgns_trust, runner, run_dir, top, timeout, approve_count,
                  allow_a6_fallback, scenario=None):
    """Boot -> CEREMONY -> ASSERT -> READBACK -> clean teardown (Variant B).

    Returns None on full success, or a diagnostics dict when stranded genesis
    is detected (the caller exits 2 — the marker block has been printed).
    A scenario (D-09) injects SIGKILL/respawn at event-anchored points inside
    the same pipeline; scenario failures are ordinary class-1 failures.
    """
    network_id = top["network_id"]
    if scenario:
        print("SCENARIO: %s — event-anchored scenario points (D-06/D-07, "
              "zero sleeps); failures exit 1" % scenario)
    print("SETUP: run dir %s" % run_dir)
    if network_id == topology.STAGING_NET_ID:
        print("SETUP: network_id %d (STAGING_NET_ID pin — the ONE sanctioned "
              "non-reserved run: the dht-health pre-provision proof; default "
              "draws stay in the reserved band, NET-05)" % network_id)
    else:
        print("SETUP: network_id %d (reserved band %d-%d)"
              % (network_id, *topology.NETWORK_ID_RESERVED_RANGE))
    print("SETUP: bootstrapper %s" % top["bootstrapper"])
    print("SETUP: peer_set %s" % ", ".join(top["peer_set"]))

    foreign = None
    if scenario == "dht-health":
        foreign = _dht_health_boot_stage(runner, run_dir, top, timeout)
        _dht_health_mesh_gates(top, timeout)
    else:
        _boot_stage(runner, run_dir, top, timeout)
    _check_fatal(top)

    print("CEREMONY: make-manifest (peers = topology peer_set — the same list "
          "every sgns_config trusted_peers carries)")
    manifest = ceremony.make_manifest(sgns_trust, run_dir, network_id,
                                      top["bootstrapper"], top["peer_set"])
    print("CEREMONY: fingerprint %s (sha256-verified against manifest.bin; "
          "double-make byte-identical)" % manifest["fingerprint"])
    print("CEREMONY: membership threshold %d, burn threshold %d (parsed from "
          "make-manifest stdout — never recomputed)"
          % (manifest["membership_threshold"], manifest["burn_threshold"]))

    actor1 = top["actors"][0]
    peer_actors = top["actors"][1:]
    network_head = None  # set by the identical-head assertion below
    serve_seconds = (WINDOW_EDGE_SERVE_SECONDS if scenario == "window-edge"
                     else ceremony.SERVE_SECONDS)
    window = None
    if scenario == "window-edge":
        window = _window_edge_prelude(runner, run_dir, top, sgns_trust, actor1,
                                      manifest, approve_count, timeout)
    else:
        ceremony.genesis(sgns_trust, actor1, manifest["manifest"],
                         manifest["fingerprint"], run_dir,
                         serve_seconds=serve_seconds)
        print("CEREMONY: actor1 Genesis durably confirmed. (key file unlinked "
              "by the tool, serve window %ds)" % serve_seconds)
        _check_fatal(top)
        approved, killed_actor_name, killed_candidate_id = _approve_sequence(
            sgns_trust, runner, run_dir, top, manifest, approve_count,
            scenario, serve_seconds)
    if scenario == "window-edge":
        approved, killed_actor_name, killed_candidate_id = (
            _window_edge_boundary(window, run_dir, runner, serve_seconds))

    post_burn_window = _post_burn_window(serve_seconds)
    print("ASSERT: approved %d of %d peers, burn threshold %d — bounded "
          "post-burn window %ds (serve %ds + grace %ds)"
          % (len(approved), len(peer_actors), manifest["burn_threshold"],
             post_burn_window, serve_seconds,
             POST_BURN_GRACE_SECONDS))
    all_advanced, ladder, last = _wait_post_burn(top, post_burn_window)
    for entry in top["nodes"]:
        print("ASSERT: %s node_state=%s" % (entry["name"], last.get(entry["name"]) or "NONE"))
    print("ASSERT: observed post-burn ladder: %s" % _format_ladder(ladder))

    a6_engaged = False
    if not all_advanced:
        partial = any(state in POST_BURN_STATES
                      for states in ladder.values() for state in states)
        if allow_a6_fallback and partial:
            # Guarded A6 escape: the strict gate is downgraded ONLY while the
            # distinct label + the observed ladder stand on stdout — a weakened
            # assertion may never stand silently, and exit 0 with this label
            # requires orchestrator sign-off on the SUMMARY (A6).
            a6_engaged = True
            print("A6-FALLBACK-ENGAGED: %s" % _format_ladder(ladder))
        else:
            _print_stranded(top, manifest, approved, peer_actors, ladder, last,
                            serve_seconds)
            _teardown_nodes(expect_clean=False)
            return {"approved": list(approved),
                    "burn_threshold": manifest["burn_threshold"]}

    if all_advanced:
        # D-08 head assertion (ratifies A1 at runtime): on a quiescent young
        # net head == the genesis CID and is network-wide identical. Sampled
        # AFTER the strict gate — every node must have printed a READY line
        # carrying head= within the bounded window; any divergence is a loud
        # failure, never a warning.
        heads = _sample_heads([entry["name"] for entry in top["nodes"]],
                              HEAD_SAMPLE_SECONDS,
                              "the post-burn head assertion")
        distinct = set(heads.values())
        if len(distinct) != 1:
            raise RuntimeError("diverged heads across nodes (expected one "
                               "identical genesis CID): %s" % heads)
        network_head = next(iter(distinct))
        print("HEAD %s identical across %d nodes"
              % (network_head, len(heads)))
        if scenario == "dht-health":
            # The quorum marker: the strict all-N post-burn gate plus the
            # identical-head assertion are the confirmed-quorum proof.
            print("DHT-GATE: quorum ok")

    if scenario == "restart-mid-sync":
        _restart_mid_sync_stage(runner, run_dir, top, network_head)
    elif scenario == "quorum-loss":
        _quorum_loss_stage(runner, run_dir, top, network_head)
    elif scenario == "window-edge":
        _window_edge_joiner_gates(top, window, network_head)
    elif scenario == "dht-health":
        _dht_health_coexistence_gate(top, foreign, timeout)

    used_actors = [actor1] + peer_actors[:approve_count]
    for actor in used_actors:
        readback = ceremony.list_candidates(sgns_trust, actor,
                                            manifest["manifest"], run_dir)
        if not any(kind == "burn" for kind, _ in readback):
            raise ceremony.CeremonyError(
                "%s readback shows no burn candidate after quorum: %s"
                % (actor["name"], readback))
        if actor["name"] == killed_actor_name:
            # SCEN-03 no-re-approval divergence evidence: the killed peer's
            # durable DB must still list the EXACT candidate it approved
            # pre-kill. The strict gate passing is already the recovery proof
            # (activation at k=3 needs this durable approval to have survived
            # the kill — a lost record would have stranded the net); this
            # readback pins the id itself.
            if not any(cid == killed_candidate_id for _, cid in readback):
                raise ceremony.CeremonyError(
                    "%s readback diverged from its pre-kill approval: "
                    "expected %s in %s"
                    % (actor["name"], killed_candidate_id, readback))
            print("SCENARIO: %s durable readback lists its pre-kill approval "
                  "%s (no re-approval divergence)"
                  % (actor["name"], killed_candidate_id))
        print("READBACK: %s %s" % (actor["name"],
              " ".join("%s %s" % pair for pair in readback)))

    _teardown_nodes(expect_clean=True)
    if a6_engaged:
        # WR-01: a fallback pass reports the OBSERVED advanced count — never
        # len(top["nodes"]), which would overstate a partial advance.
        advanced_count = sum(1 for state in last.values()
                             if state in POST_BURN_STATES)
        print("CEREMONY PASS (A6 fallback, %d of %d nodes advanced): "
              "network_id=%d — %d of %d peers approved (threshold %d)"
              % (advanced_count, len(top["nodes"]), network_id, len(approved),
                 len(peer_actors), manifest["burn_threshold"]))
    else:
        print("CEREMONY PASS: network_id=%d — %d nodes beyond "
              "WAITING_FOR_BURN_GENESIS, %d of %d peers approved (threshold %d)"
              % (network_id, len(top["nodes"]), len(approved), len(peer_actors),
                 manifest["burn_threshold"]))
    return None


def _print_stranded(top, manifest, approved, peer_actors, ladder, last,
                    serve_seconds):
    """The stranded-genesis verdict. The marker is the FIRST line — it
    disambiguates this exit-2 from argparse's usage exit-2."""
    pending = top["peer_set"][len(approved):]
    print(STRANDED_MARKER)
    print("network_id=%d: quorum unreachable — %d of %d peers approved, burn "
          "threshold %d (parsed from make-manifest stdout)"
          % (top["network_id"], len(approved), len(peer_actors),
             manifest["burn_threshold"]))
    print("approved peers: %s" % (", ".join(approved) or "(none)"))
    print("peers that never approved: %s" % (", ".join(pending) or "(none)"))
    print("post-burn window elapsed: %ds (serve %ds + grace %ds) — deadline "
          "passed" % (_post_burn_window(serve_seconds), serve_seconds,
                      POST_BURN_GRACE_SECONDS))
    print("stuck nodes (last observed state):")
    for entry in top["nodes"]:
        print("  %s: %s" % (entry["name"], last.get(entry["name"]) or "NONE"))
    print("observed state ladder during the window: %s" % _format_ladder(ladder))


def scan_keys(paths, key_hexes):
    """D-07: walk every regular file under paths, searching raw bytes for each
    key's 32-byte binary form AND its 64-lowercase-hex ASCII form. Returns
    (files_scanned, [(path, key_index)])."""
    forms = [(index, bytes.fromhex(key_hex)) for index, key_hex in enumerate(key_hexes)]
    forms += [(index, key_hex.encode("ascii")) for index, key_hex in enumerate(key_hexes)]
    scanned = 0
    hits = []
    for root in paths:
        for dirpath, _dirnames, filenames in os.walk(root):
            for filename in sorted(filenames):
                path = os.path.join(dirpath, filename)
                if os.path.islink(path) or not os.path.isfile(path):
                    continue
                with open(path, "rb") as handle:
                    data = handle.read()
                scanned += 1
                for index, form in forms:
                    if form in data:
                        hits.append((path, index))
    return scanned, hits


def scrub_key_file(path):
    """Secure-overwrite then unlink one key file (returns True if it existed).
    genesis unlinks its own key on success; approve retains — scrub all
    unconditionally at teardown (D-17 order: teardown -> scrub -> preserve)."""
    if not os.path.exists(path):
        return False
    size = os.path.getsize(path)
    descriptor = os.open(path, os.O_RDWR)
    try:
        if size:
            os.write(descriptor, b"\x00" * size)
            os.fsync(descriptor)
    finally:
        os.close(descriptor)
    os.unlink(path)
    return True


def _scrub_file_contents(path):
    """Neutralize a leaking artifact: zero its whole content, keep the file
    (the path evidence survives; the key bytes do not)."""
    size = os.path.getsize(path)
    descriptor = os.open(path, os.O_RDWR)
    try:
        if size:
            os.write(descriptor, b"\x00" * size)
            os.fsync(descriptor)
    finally:
        os.close(descriptor)


def _scrub_run_keys(run_dir):
    keys_dir = os.path.join(run_dir, "keys")
    scrubbed = 0
    if os.path.isdir(keys_dir):
        for name in sorted(os.listdir(keys_dir)):
            if scrub_key_file(os.path.join(keys_dir, name)):
                scrubbed += 1
    return scrubbed


def _read_key_hexes(top):
    """Key bytes for the D-07 scan, read once into memory (never printed)."""
    hexes = []
    for entry in top["nodes"]:
        with open(entry["key_file"]) as handle:
            hexes.append(handle.read().strip())
    return hexes


def _read_existing_key_hexes(run_dir):
    """WR-03: whatever key material actually exists on disk after a partial
    failure. build_topology writes one key file per node in turn, so a failure
    partway through leaves fewer files than nodes (and a file caught mid-write
    may be partial). Called BEFORE the scrub, which unlinks them. Returns []
    when no key material exists."""
    hexes = []
    keys_dir = os.path.join(run_dir, "keys")
    if os.path.isdir(keys_dir):
        for name in sorted(os.listdir(keys_dir)):
            if not name.endswith(".key"):
                continue
            path = os.path.join(keys_dir, name)
            if os.path.isfile(path):
                with open(path) as handle:
                    hexes.append(handle.read().strip())
    return hexes


def _scan_selftest():
    """Non-vacuity guard (Phase 1 GTEST-04 discipline): a scan that finds
    nothing it was pointed at is itself a failure. Plants one known key and
    asserts scan_keys FINDS it — runs beside the EC pinned-vector assert."""
    key = secp256k1_address.generate_key_hex()
    with tempfile.TemporaryDirectory(prefix="scan-selftest-") as scratch:
        planted = os.path.join(scratch, "planted-leak.txt")
        with open(planted, "w") as handle:
            handle.write("planted leak %s\n" % key)
        scanned, hits = scan_keys([scratch], [key])
        assert scanned >= 1 and hits and hits[0][0] == planted, \
            "scan self-test failed to FIND the planted key (vacuous scan)"
    print("SELFTEST: key-absence scan finds a planted key (non-vacuous)")


def _preserve_selftest():
    """Collision guard (observed in CI run 37803667091): two preserves inside
    one second — back-to-back fast-fail cases under CI — must both land as
    distinct artifact dirs instead of crashing on the shared one-second
    stamp. Cleans up what it creates."""
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "artifacts")
    with tempfile.TemporaryDirectory(prefix="preserve-selftest-") as scratch:
        def _stub_run_dir():
            os.makedirs(run_dir)
            with open(os.path.join(run_dir, "node1.out"), "w") as handle:
                handle.write("stub\n")
        run_dir = os.path.join(scratch, "run")
        before = set(os.listdir(root)) if os.path.isdir(root) else set()
        _stub_run_dir()
        _preserve_and_scan(run_dir, [])
        _stub_run_dir()
        _preserve_and_scan(run_dir, [])
        new = set(os.listdir(root)) - before
        for name in new:
            shutil.rmtree(os.path.join(root, name), ignore_errors=True)
        assert len(new) == 2, \
            "same-second preserves did not land as two dirs: %s" % sorted(new)
    print("SELFTEST: same-second preserves get distinct artifact dirs")


def _report_scan(scanned, hits):
    if hits:
        print("key-absence scan: LEAK — %d hit(s): %s"
              % (len(hits), "; ".join("%s (key %d)" % (path, index + 1)
                                      for path, index in hits)))
    else:
        print("key-absence scan: CLEAN (%d files scanned)" % scanned)


def _preserve_and_scan(run_dir, key_hexes):
    """D-17 failure path: preserve the (scrubbed) run dir under
    e2e/artifacts/<UTC-timestamp>-run/, then prove the copy key-free."""
    here = os.path.dirname(os.path.abspath(__file__))
    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    artifacts = os.path.join(here, "artifacts", stamp + "-run")
    dest, n = artifacts, 2
    while os.path.exists(dest):
        # CI fast-fail paths preserve twice inside one second (observed in
        # run 37803667091): both copies must land, not crash on the stamp.
        dest = "%s-%d" % (artifacts, n)
        n += 1
    os.makedirs(os.path.dirname(artifacts), exist_ok=True)
    shutil.copytree(run_dir, dest)
    shutil.rmtree(run_dir, ignore_errors=True)
    print("PRESERVE: run dir copied (key files scrubbed) to %s" % dest)
    if not key_hexes:
        # WR-03: no key material ever existed (partial-topology failure) — a
        # scan over zero key forms would be vacuously CLEAN. The scan must
        # never run with an empty key set.
        print("key-absence scan: SKIPPED (no keys)")
        return []
    scanned, hits = scan_keys([dest], key_hexes)
    _report_scan(scanned, hits)
    for path, _index in hits:
        _scrub_file_contents(path)
    return hits


def main():
    parser = argparse.ArgumentParser(
        description="Local ceremony e2e harness: boot N SDK-booted nodes and "
                    "drive the genesis ceremony, asserting only observable "
                    "outcomes. Exit codes: 0 success, 2 stranded genesis "
                    "(STRANDED-GENESIS-DETECTED marker), 1 other failure.")
    parser.add_argument("--nodes", type=int, default=5,
                        help="ceremony node count (default 5; the full "
                             "ceremony needs >= 2)")
    parser.add_argument("--runner", default=os.environ.get("SGNS_RUNNER_BIN"),
                        help="path to the GeniusSDKService binary "
                             "(env fallback SGNS_RUNNER_BIN)")
    parser.add_argument("--sgns-trust", default=os.environ.get("SGNS_TRUST_BIN"),
                        help="path to the sgns-trust binary (env fallback "
                             "SGNS_TRUST_BIN; required unless --boot-only)")
    parser.add_argument("--boot-only", action="store_true",
                        help="boot all nodes to WAITING_FOR_TRUST_GENESIS and "
                             "tear down (skip the ceremony stages)")
    parser.add_argument("--approve-peers", type=int, default=None, metavar="K",
                        help="approve only the first K peers (default: all). "
                             "K below the parsed burn threshold is the "
                             "stranded-genesis negative control (exit 2)")
    parser.add_argument("--scenario", default=None,
                        choices=["restart-mid-ceremony", "restart-mid-sync",
                                 "quorum-loss", "window-edge", "dht-health"],
                        help="resilience scenario to run (D-09 token set + "
                             "the 04-01 dht-health gate, validated here at "
                             "the edge; default: the Phase 2 happy path, "
                             "unchanged). Scenario failures are exit 1; the "
                             "stranded negative control stays exit 2")
    parser.add_argument("--network-id", type=int, default=None, metavar="N",
                        help="pin the run's network id (dht-health only; the "
                             "reserved band or exactly STAGING_NET_ID 333 — "
                             "the ONE sanctioned non-reserved value, the "
                             "pre-provision proof. Default: reserved-band "
                             "draw, NET-05)")
    parser.add_argument("--coexist-net-id", type=int, default=None, metavar="N",
                        help="net registry id for the dht-health co-located "
                             "foreign node (must be one of 369/963/144/333 "
                             "and differ from the staging net; default 144 "
                             "beside an explicit 333 run, else 963 — the C++ "
                             "registry rejects the whole reserved band for "
                             "net_id)")
    parser.add_argument("--allow-a6-fallback", action="store_true",
                        help="downgrade the strict all-N post-burn gate ONLY "
                             "when at least one node advanced; prints the "
                             "distinct A6-FALLBACK-ENGAGED label with the "
                             "observed ladder (exit 0 then requires "
                             "orchestrator sign-off on the SUMMARY)")
    parser.add_argument("--keep-dir", action="store_true",
                        help="keep the run dir even on success (debugging)")
    parser.add_argument("--timeout", type=int, default=300,
                        help="bounded wait per output gate, seconds")
    args = parser.parse_args()

    runner = args.runner
    if not runner or not os.path.isfile(runner) or not os.access(runner, os.X_OK):
        sys.exit("FAIL: runner not executable: %r (pass --runner PATH or set "
                 "SGNS_RUNNER_BIN)" % (runner,))
    sgns_trust = args.sgns_trust
    if not args.boot_only:
        if not sgns_trust or not os.path.isfile(sgns_trust) \
                or not os.access(sgns_trust, os.X_OK):
            sys.exit("FAIL: sgns-trust not executable: %r (pass --sgns-trust "
                     "PATH or set SGNS_TRUST_BIN)" % (sgns_trust,))
        if args.nodes < 2:
            sys.exit("FAIL: the full ceremony needs at least 2 nodes "
                     "(bootstrapper + 1 peer)")
    peers_total = args.nodes - 1
    approve_count = args.approve_peers
    if approve_count is None:
        approve_count = peers_total
    elif not 0 <= approve_count <= peers_total:
        parser.error("--approve-peers must be 0..%d (peers = nodes - 1)"
                     % peers_total)
    if args.scenario:
        if args.boot_only:
            parser.error("--scenario runs the full ceremony (incompatible "
                         "with --boot-only)")
        if args.approve_peers is not None:
            parser.error("--scenario approves all peers (incompatible with "
                         "--approve-peers)")
        if args.nodes < 5 and args.scenario != "dht-health":
            parser.error("%s needs the 5-node topology (bootstrapper + 4 "
                         "peers; the kill points anchor on the parsed 3-of-4 "
                         "burn floor, the joiners join a 5-node net, "
                         "quorum-loss kills 2 of the 4 trusted peers, and "
                         "window-edge proves both boundary sides on it)"
                         % args.scenario)
    if args.network_id is not None or args.coexist_net_id is not None:
        if args.scenario != "dht-health":
            parser.error("--network-id/--coexist-net-id belong to the "
                         "dht-health scenario (NET-05: no other run may pin "
                         "its net id)")
    if args.scenario == "dht-health":
        if args.nodes != 5:
            parser.error("dht-health runs the exact 5-node topology (the "
                         "split bootstrap anchors node3/node4/node5 by "
                         "position and the foreign node takes the port band "
                         "slot after the 5th seed)")
        if args.network_id is not None:
            try:
                topology.validate_network_id(args.network_id)
            except ValueError as error:
                parser.error(str(error))
        # The staging nodes' effective C++ net: 333 when pinned (the only
        # value that pins — topology's payload escape), else the silent DEV
        # default every Phase 2-3 run rides.
        staging_net = (topology.STAGING_NET_ID
                       if args.network_id == topology.STAGING_NET_ID else 144)
        if args.coexist_net_id is None:
            # Registry-accepted default differing from the staging net (see
            # the FOREIGN_NET constants note for why a band draw cannot be
            # the foreign net).
            coexist_net_id = (FOREIGN_NET_BESIDE_STAGING
                              if staging_net == topology.STAGING_NET_ID
                              else FOREIGN_NET_BESIDE_DEFAULT)
        else:
            coexist_net_id = args.coexist_net_id
            if coexist_net_id not in topology.REGISTRY_NET_IDS:
                parser.error("--coexist-net-id %d is outside the C++ net "
                             "registry %s — the foreign node would die at "
                             "init (Pitfall 1)"
                             % (coexist_net_id, topology.REGISTRY_NET_IDS))
            if coexist_net_id == staging_net:
                parser.error("--coexist-net-id %d equals the staging net — "
                             "the coexistence proof needs a DIFFERENT net"
                             % coexist_net_id)
    else:
        coexist_net_id = None

    # Harness-start self-checks: the EC pinned vector asserts at import of
    # topology/secp256k1_address; the scan self-test proves non-vacuity and
    # the preserve self-test the same-second collision guard, here.
    _scan_selftest()
    _preserve_selftest()

    # A SIGTERM to the harness itself must still run the teardown in finally.
    signal.signal(signal.SIGTERM,
                  lambda *_: sys.exit(143))

    run_dir = tempfile.mkdtemp(prefix="geniussdk-e2e-")  # 0700
    harness_out = open(os.path.join(run_dir, "harness.out"), "wb")
    original_stdout = sys.stdout
    sys.stdout = _Tee(original_stdout, harness_out)

    stranded = None
    failure = None
    leak = None
    success = False
    key_hexes = []
    try:
        top = topology.build_topology(run_dir, args.nodes,
                                      network_id=args.network_id)
        key_hexes = _read_key_hexes(top)
        # NET-05 belt-and-braces: the id (drawn OR the sanctioned staging
        # pin) is re-validated in-band before any use — one home for the
        # policy since 04-01 (topology.validate_network_id); diagnostics
        # carry it too.
        topology.validate_network_id(top["network_id"])
        if coexist_net_id is not None:
            top["coexist_net_id"] = coexist_net_id
        if args.boot_only:
            _boot_only_run(runner, run_dir, top, args.timeout)
        else:
            stranded = _ceremony_run(sgns_trust, runner, run_dir, top,
                                     args.timeout, approve_count,
                                     args.allow_a6_fallback,
                                     scenario=args.scenario)
        success = stranded is None
    except BaseException as caught:  # broad by design: D-16/D-17 failure boundary
        failure = caught
    finally:
        supervisor.teardown(timeout=60)  # no-op when the run tore down already
        if not key_hexes:
            # WR-03: a partial-topology failure never reached _read_key_hexes;
            # recover whatever key material exists so the failure scan is
            # never vacuous. Must run BEFORE the scrub (it unlinks the files).
            key_hexes = _read_existing_key_hexes(run_dir)
        _scrub_run_keys(run_dir)  # D-17: scrub BEFORE preserve/scan/delete
        harness_out.flush()
        if success and not args.keep_dir:
            scanned, hits = scan_keys([run_dir], key_hexes)
            _report_scan(scanned, hits)
            if hits:
                leak = hits
            else:
                shutil.rmtree(run_dir, ignore_errors=True)
        elif success:
            print("KEEP: run dir preserved (--keep-dir): %s" % run_dir)
        else:
            leak = _preserve_and_scan(run_dir, key_hexes) or None
    harness_out.close()
    sys.stdout = original_stdout

    if failure is not None:
        print("FAIL: %s" % failure, file=sys.stderr)
    if leak is not None:
        print("FAIL: leaked key bytes into surviving artifacts (scrubbed; "
              "see the LEAK line above)", file=sys.stderr)
    if failure is not None or leak is not None:
        return 1
    if stranded is not None:
        print("STRANDED: run correctly refused to pass a below-quorum net "
              "(exit 2; %d of %d approvals vs threshold %d; artifacts "
              "preserved)"
              % (len(stranded["approved"]), args.nodes - 1,
                 stranded["burn_threshold"]))
        return 2
    if args.boot_only:
        print("BOOT-ONLY PASS: %d nodes reached WAITING_FOR_TRUST_GENESIS "
              "and exited 0" % args.nodes)
    else:
        print("CEREMONY E2E PASS: %d nodes, full Variant B ceremony to "
              "confirmed quorum (exit 0; run dir deleted)" % args.nodes)
    return 0


if __name__ == "__main__":
    sys.exit(main())
