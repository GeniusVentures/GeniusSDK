#!/usr/bin/env python3
"""
gen_local_fleet_fixture.py — build the all-local fleet.json parity fixture
(04-03): the file ceremony_e2e.py --fleet consumes for the local-parity proof
("FLEET: local-parity ok").

Mirrors the real Wave 4 -> Wave 5 flow on ONE machine: generate the
persistent identities offline, provision the static-fleet bundles (the 04-02
one-home writer), boot the genesis identity ONCE to capture its live port +
peer id (observed values, never hand-transcribed), fill the published /dns4/
multiaddr from the capture, rewrite the bundles so every peer's
bootstrap_addresses carries the real entry address, and emit the
all-local-transport fleet.json.

The fixture is CONSUMED by a --fleet run: a successful ceremony pushes the
full trust config into the base dirs, so a second --fleet over the same
fleet.json aborts loud at preflight (the dirty-state gate) — regenerate the
fixture for each parity run (fresh base dir + fresh identities, exactly the
real fleet's fresh-provisioning semantics).
"""

import argparse
import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import supervisor
import topology
from ceremony_e2e import PUBSUB_LINE, _multiaddr_tcp_peer


def main():
    parser = argparse.ArgumentParser(
        description="Generate the all-local fleet.json for the --fleet "
                    "local-parity proof (one fresh fixture per parity run).")
    parser.add_argument("--runner",
                        default=os.environ.get("SGNS_RUNNER_BIN"),
                        help="path to the GeniusSDKService binary (env "
                             "fallback SGNS_RUNNER_BIN)")
    parser.add_argument("--out", default="/tmp/fleet-local.json",
                        help="fleet.json path to write (default "
                             "/tmp/fleet-local.json)")
    args = parser.parse_args()
    if not args.runner or not os.path.isfile(args.runner) \
            or not os.access(args.runner, os.X_OK):
        sys.exit("FAIL: runner not executable: %r (pass --runner PATH or set "
                 "SGNS_RUNNER_BIN)" % (args.runner,))

    base = tempfile.mkdtemp(prefix="fleet-parity-base-")
    keys_dir = os.path.join(base, "offline-keys")
    fleet = topology.gen_fleet_keys(keys_dir,
                                    os.path.join(base, "fleet-raw.json"))

    # Provision round 1 with a fillable (non-PLACEHOLDER) dummy peer id so
    # the bundle writer accepts the manifest; the real id lands after the
    # capture.
    genesis = fleet["nodes"][0]
    genesis["published_multiaddr"] = topology.make_dns4_multiaddr(
        "localhost", genesis["port_seed"], "QmDummyFillAfterCapture")
    topology.write_fleet_configs(fleet, base)

    # Identity boot: capture the live PubSub multiaddr through the same
    # supervisor gate idiom the harness uses.
    capture_dir = os.path.join(base, "captures")
    os.makedirs(capture_dir)
    node1_dir = os.path.join(base, "node1") + "/"
    node_log = os.path.join(node1_dir, "sgnslog2.log")
    supervisor.spawn("fixture-identity",
                     [args.runner, node1_dir, "--key-file", genesis["key_file"]],
                     capture_dir, extra_logs=[node_log])
    multiaddr = supervisor.wait_for("fixture-identity", PUBSUB_LINE,
                                    300).group(1)
    port, peer = _multiaddr_tcp_peer(multiaddr)
    code = supervisor.terminate("fixture-identity", timeout=60)
    assert code == 0, "fixture identity boot exited %s on SIGTERM" % code
    print("captured genesis identity: tcp/%s/ipfs/%s" % (port, peer))

    # Fill the real entry address + local transports, rewrite the bundles so
    # the peers' bootstrap_addresses carry the observed port + peer id.
    for index, node in enumerate(fleet["nodes"], start=1):
        node["transport"] = {"kind": "local"}
        node["base_dir"] = os.path.join(base, "node%d" % index) + "/"
        node.pop("bundle_dir", None)
    genesis["published_multiaddr"] = topology.make_dns4_multiaddr("localhost",
                                                                  port, peer)
    topology.write_fleet_configs(fleet, base)
    for node in fleet["nodes"]:
        node.pop("bundle_dir", None)

    with open(args.out, "w") as handle:
        json.dump(fleet, handle, indent=2)
        handle.write("\n")
    print("wrote %s (%d local nodes, base %s)" % (args.out,
                                                  len(fleet["nodes"]), base))
    print("NEXT: python3 e2e/ceremony_e2e.py --fleet %s --runner <runner> "
          "--sgns-trust <sgns-trust>" % args.out)


if __name__ == "__main__":
    main()
