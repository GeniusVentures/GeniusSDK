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

Scenario surface (Phase 3, D-09): --scenario restart-mid-ceremony |
restart-mid-sync | quorum-loss | window-edge; default None is the Phase 2
happy path, completely unchanged. Mapping note (D-09 locks the four-token
surface): restart-mid-ceremony covers BOTH SCEN-03 kill point 1 (SIGKILL
right after the 2nd of 4 peer approves returns — below the parsed burn
floor, mid-ceremony) and kill point 2 (SIGKILL right after the FINAL
approve returns — post-quorum, pre-start); restart-mid-sync is SCEN-03 kill
point 3 (SIGKILL a real 6th-process joiner the instant its own
Blockchain-logger sync anchor prints, then recover to READY at the network
head); quorum-loss is SCEN-04 (2-of-5 SIGKILL post-READY: survivors must
hold the frozen head with stable READY states and no FATAL, then all 5
respawned nodes converge back to the SAME head). Standing assertion:
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
# Bounded post-quorum wait: the actors' serve window + grace (02-02 observed
# boot-to-WAITING at 12-14s per node on a 10s STATUS cadence — generous headroom).
POST_BURN_GRACE_SECONDS = 120
POST_BURN_WINDOW = ceremony.SERVE_SECONDS + POST_BURN_GRACE_SECONDS
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


def _boot_stage(runner, run_dir, top, timeout):
    """IDENTITY -> START -> per-node WAITING_FOR_TRUST_GENESIS gates — the
    02-02 boot path, unchanged; returns per-node spawn->gate timings."""
    node1 = top["nodes"][0]
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
    topology.write_peer_configs(top, multiaddr)
    for entry in top["nodes"][1:]:
        _spawn_node(entry["name"], runner, entry, run_dir, spawn_times)
        print("START: %s spawned (bootstrap %s)" % (entry["name"], multiaddr))

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
    supervisor.wait_for("joiner-restart", r"STATUS node_state=READY\b",
                        JOINER_GATE_SECONDS)
    deadline = time.monotonic() + HEAD_SAMPLE_SECONDS
    joiner_head = None
    while True:
        joiner_head = supervisor.head_of("joiner-restart")
        if joiner_head or time.monotonic() >= deadline:
            break
        time.sleep(1.0)
    if not joiner_head:
        raise RuntimeError("joiner-restart READY but no STATUS head= observed "
                           "within %ds" % HEAD_SAMPLE_SECONDS)
    if supervisor.state_of("joiner-restart") != "READY":
        raise RuntimeError("joiner-restart latest state %s (expected READY)"
                           % supervisor.state_of("joiner-restart"))
    if joiner_head != network_head:
        raise RuntimeError("joiner-restart head %s != network head %s (the "
                           "respawned joiner diverged)" % (joiner_head,
                                                           network_head))
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
        print("SCENARIO: %s — event-anchored SIGKILL points (D-06/D-07, zero "
              "sleeps); failures exit 1" % scenario)
    print("SETUP: run dir %s" % run_dir)
    print("SETUP: network_id %d (reserved band %d-%d)"
          % (network_id, *topology.NETWORK_ID_RESERVED_RANGE))
    print("SETUP: bootstrapper %s" % top["bootstrapper"])
    print("SETUP: peer_set %s" % ", ".join(top["peer_set"]))

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
    ceremony.genesis(sgns_trust, actor1, manifest["manifest"],
                     manifest["fingerprint"], run_dir)
    print("CEREMONY: actor1 Genesis durably confirmed. (key file unlinked by "
          "the tool, serve window %ds)" % ceremony.SERVE_SECONDS)
    _check_fatal(top)

    peer_actors = top["actors"][1:]
    peer_addresses = top["peer_set"]  # peer_actors[i] signs as peer_set[i]
    approved = []
    killed_actor_name = None
    killed_candidate_id = None
    network_head = None  # set by the identical-head assertion below
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
                         run_dir)
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

    print("ASSERT: approved %d of %d peers, burn threshold %d — bounded "
          "post-burn window %ds (serve %ds + grace %ds)"
          % (len(approved), len(peer_actors), manifest["burn_threshold"],
             POST_BURN_WINDOW, ceremony.SERVE_SECONDS,
             POST_BURN_GRACE_SECONDS))
    all_advanced, ladder, last = _wait_post_burn(top, POST_BURN_WINDOW)
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
            _print_stranded(top, manifest, approved, peer_actors, ladder, last)
            _teardown_nodes(expect_clean=False)
            return {"approved": list(approved),
                    "burn_threshold": manifest["burn_threshold"]}

    if all_advanced:
        # D-08 head assertion (ratifies A1 at runtime): on a quiescent young
        # net head == the genesis CID and is network-wide identical. Sampled
        # AFTER the strict gate — every node must have printed a READY line
        # carrying head= within the bounded window; any divergence is a loud
        # failure, never a warning.
        deadline = time.monotonic() + HEAD_SAMPLE_SECONDS
        heads = {}
        while True:
            heads = {entry["name"]: supervisor.head_of(entry["name"])
                     for entry in top["nodes"]}
            if all(heads.values()) or time.monotonic() >= deadline:
                break
            time.sleep(1.0)
        missing = sorted(name for name, head in heads.items() if not head)
        if missing:
            raise RuntimeError("no STATUS head= observed within %ds after the "
                               "post-burn gate (accessor failing?): %s"
                               % (HEAD_SAMPLE_SECONDS, missing))
        distinct = set(heads.values())
        if len(distinct) != 1:
            raise RuntimeError("diverged heads across nodes (expected one "
                               "identical genesis CID): %s" % heads)
        network_head = next(iter(distinct))
        print("HEAD %s identical across %d nodes"
              % (network_head, len(heads)))

    if scenario == "restart-mid-sync":
        _restart_mid_sync_stage(runner, run_dir, top, network_head)

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


def _print_stranded(top, manifest, approved, peer_actors, ladder, last):
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
          "passed" % (POST_BURN_WINDOW, ceremony.SERVE_SECONDS,
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
    os.makedirs(os.path.dirname(artifacts), exist_ok=True)
    shutil.copytree(run_dir, artifacts)
    shutil.rmtree(run_dir, ignore_errors=True)
    print("PRESERVE: run dir copied (key files scrubbed) to %s" % artifacts)
    if not key_hexes:
        # WR-03: no key material ever existed (partial-topology failure) — a
        # scan over zero key forms would be vacuously CLEAN. The scan must
        # never run with an empty key set.
        print("key-absence scan: SKIPPED (no keys)")
        return []
    scanned, hits = scan_keys([artifacts], key_hexes)
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
                                 "quorum-loss", "window-edge"],
                        help="resilience scenario to run (D-09 token set, "
                             "validated here at the edge; default: the Phase "
                             "2 happy path, unchanged). Scenario failures are "
                             "exit 1; the stranded negative control stays "
                             "exit 2")
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
        if args.scenario not in ("restart-mid-ceremony", "restart-mid-sync"):
            parser.error("--scenario %s is implemented by a later Phase 3 "
                         "plan; this plan ships restart-mid-ceremony and "
                         "restart-mid-sync" % args.scenario)
        if args.nodes < 5:
            parser.error("%s needs the 5-node topology (bootstrapper + 4 "
                         "peers; the kill points anchor on the parsed 3-of-4 "
                         "burn floor and the joiner joins a 5-node net)"
                         % args.scenario)

    # Harness-start self-checks: the EC pinned vector asserts at import of
    # topology/secp256k1_address; the scan self-test proves non-vacuity here.
    _scan_selftest()

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
        top = topology.build_topology(run_dir, args.nodes)
        key_hexes = _read_key_hexes(top)
        # NET-05 belt-and-braces: the drawn id is asserted in-band before any
        # use (besides topology's own selftest); diagnostics carry it too.
        low, high = topology.NETWORK_ID_RESERVED_RANGE
        if not low <= top["network_id"] <= high:
            raise RuntimeError("network_id %d outside the reserved band "
                               "%d-%d" % (top["network_id"], low, high))
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
