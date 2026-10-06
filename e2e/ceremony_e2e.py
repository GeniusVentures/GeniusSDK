#!/usr/bin/env python3
"""
ceremony_e2e.py — the single e2e entry command (D-16) for the local ceremony
harness.

--boot-only (this plan's proof mode): boot N real node processes through the
one GeniusSDKService runner with generated identities, unique >300-spaced
ports, and an isolated reserved-band network id. Node 1 boots twice by design:
a bare identity boot first (its SDK account address B_1 lands in
secure_storage_id and becomes the authorized_full_node that every
sgns_config.json needs — without that key blockchain start defers forever),
then the final boot with full trust wiring; its PubSub multiaddr (scraped from
captured output — staged bootstrap, Pitfall 5) lands in every later config.
Every node must reach WAITING_FOR_TRUST_GENESIS through public output only
(STATUS lines and the captured logs — no RPC, no shared DBs, D-10/D-11);
teardown always runs (SIGTERM -> SIGKILL escalation) and the run dir is
deleted on success, preserved for diagnosis on failure (D-17). Harness output
carries addresses, paths, ids, and states only — never key material.

Usage:
  python3 e2e/ceremony_e2e.py --nodes 5 --boot-only --runner /abs/GeniusSDKService
Env fallbacks: SGNS_RUNNER_BIN, SGNS_TRUST_BIN (sgns-trust is driven by
plan 02-03; its path is taken here and validated lazily when first used).
"""

import argparse
import os
import re
import shutil
import signal
import sys
import tempfile
import time

import supervisor
import topology  # imports secp256k1_address: pinned vector asserts at import

# The staged bootstrap gate: the bootstrapper's live PubSub address, including
# the /p2p/<peer-id> suffix (GeniusNode.cpp L1671, observed in sgnslog2.log —
# the node logger file-sinks there; topology's log_config promotes it to info).
PUBSUB_LINE = r"PubSub started at address: (\S+)"
# Per-node boot gate: the ceremony waiting state, with the loud fail-fast.
WAITING_OR_FATAL = r"STATUS node_state=(WAITING_FOR_TRUST_GENESIS|FATAL_TRUST_MISMATCH)"
ACCOUNT_ADDRESS_RE = r"^([0-9a-f]{128})$"


def _spawn_node(name, runner, entry, run_dir, spawn_times):
    supervisor.spawn(
        name,
        [runner, entry["base_dir"], "--key-file", entry["key_file"]],
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


def _boot_only_run(runner, run_dir, nodes, timeout):
    """SETUP -> IDENTITY -> START -> per-node WAITING gates -> teardown;
    raises on any failure."""
    top = topology.build_topology(run_dir, nodes)
    print("SETUP: run dir %s" % run_dir)
    print("SETUP: network_id %d (reserved band %d-%d)"
          % (top["network_id"], *topology.NETWORK_ID_RESERVED_RANGE))
    print("SETUP: bootstrapper %s" % top["bootstrapper"])
    print("SETUP: peer_set %s" % ", ".join(top["peer_set"]))

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

    for entry in top["nodes"]:
        print("OBSERVED: %s node_state=%s"
              % (entry["name"], supervisor.state_of(entry["name"]) or "NONE"))

    print("TEARDOWN: SIGTERM all nodes (SIGKILL escalation on timeout)")
    exit_codes = supervisor.teardown(timeout=60)
    for name in sorted(exit_codes):
        print("EXIT: %s code %s" % (name, exit_codes[name]))
    unclean = {n: c for n, c in exit_codes.items() if c != 0}
    if unclean:
        raise RuntimeError("unclean node exits (a SIGKILLed node is a loud "
                           "failure): %s" % unclean)
    return top, timings


def main():
    parser = argparse.ArgumentParser(
        description="Local ceremony e2e harness: boot N SDK-booted nodes and "
                    "(plan 02-03) drive the genesis ceremony, asserting only "
                    "observable outcomes.")
    parser.add_argument("--nodes", type=int, default=5,
                        help="ceremony node count (default 5)")
    parser.add_argument("--runner", default=os.environ.get("SGNS_RUNNER_BIN"),
                        help="path to the GeniusSDKService binary "
                             "(env fallback SGNS_RUNNER_BIN)")
    parser.add_argument("--sgns-trust", default=os.environ.get("SGNS_TRUST_BIN"),
                        help="path to the sgns-trust binary, driven by plan "
                             "02-03 (env fallback SGNS_TRUST_BIN; validated "
                             "lazily when first used)")
    parser.add_argument("--boot-only", action="store_true",
                        help="boot all nodes to WAITING_FOR_TRUST_GENESIS and "
                             "tear down (this plan's proof mode)")
    parser.add_argument("--keep-dir", action="store_true",
                        help="keep the run dir even on success (debugging)")
    parser.add_argument("--timeout", type=int, default=300,
                        help="bounded wait per output gate, seconds")
    args = parser.parse_args()

    runner = args.runner
    if not runner or not os.path.isfile(runner) or not os.access(runner, os.X_OK):
        sys.exit("FAIL: runner not executable: %r (pass --runner PATH or set "
                 "SGNS_RUNNER_BIN)" % (runner,))
    if not args.boot_only:
        sys.exit("ceremony stages arrive in plan 02-03; this plan proves "
                 "--boot-only")

    # A SIGTERM to the harness itself must still run the teardown in finally.
    signal.signal(signal.SIGTERM,
                  lambda *_: sys.exit(143))

    run_dir = tempfile.mkdtemp(prefix="geniussdk-e2e-")  # 0700
    success = False
    failure = None
    try:
        _boot_only_run(runner, run_dir, args.nodes, args.timeout)
        success = True
    except BaseException as caught:  # broad by design: D-16/D-17 failure boundary
        failure = caught
    finally:
        supervisor.teardown(timeout=60)  # no-op when the run tore down already
        if success and not args.keep_dir:
            shutil.rmtree(run_dir, ignore_errors=True)
        elif success:
            print("KEEP: run dir preserved (--keep-dir): %s" % run_dir)
        else:
            print("FAIL: run dir preserved: %s" % run_dir, file=sys.stderr)
    if failure is not None:
        print("BOOT-ONLY FAIL: %s" % failure, file=sys.stderr)
        return 1
    print("BOOT-ONLY PASS: %d nodes reached WAITING_FOR_TRUST_GENESIS and "
          "exited 0" % args.nodes)
    return 0


if __name__ == "__main__":
    sys.exit(main())
