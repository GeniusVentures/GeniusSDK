#!/usr/bin/env python3
"""
topology.py — run dir, identities, and config generation for the ceremony e2e
harness.

Phase A (build_topology): everything a run can know before the first process
boots — N fresh throwaway keys as 0600 files, derived ceremony addresses A_i
(pure Python, pinned-vector checked at import), per-node dev/network/log
configs, actor DB dirs, the port plan, a reserved-band network id, and the
topology manifest (D-08/D-09). Phase A2 (write_trust_configs): per-node
sgns_config.json, written once node 1's SDK account address B_1 is known (it
is the authorized_full_node that un-defers blockchain start — see
write_trust_configs). Phase B (write_peer_configs): the one fact that needs
node 1 alive on the final boot — peers' and actors' bootstrap_addresses from
the bootstrapper's live PubSub multiaddr.

The topology manifest is the single source every later stage reads; peer_set is
THE peer-set fact shared by sgns_config generation here and plan 02-03's
make-manifest --peers call (one home per rule, D-12b).

Usage:
  python3 topology.py --selftest
"""

import argparse
import json
import os
import secrets
import shutil
import tempfile

import secp256k1_address

# NET-05: ephemeral test nets draw their network id from a reserved band that
# no persistent (staging/dev) net will ever use.
NETWORK_ID_RESERVED_RANGE = (61440, 65535)  # 0xF000-0xFFFF

TOPIC = "SuperGNUSNode.TestNet.FullNode"  # GNUS_FULL_NODES_TOPIC constant (Pitfall 6)

NODE_PORT_BASE = 41001     # per-node port_seeds: base + (i-1)*500 (Pitfall 4:
NODE_PORT_STRIDE = 500     # resolution adds hash%301, so >300 spacing kills collisions)
ACTOR_PORT_BASE = 45501    # actor pubsub_port strings: disjoint band, base + (i-1)

NODE_LOGGER_NAME = "SuperGeniusNode"  # InitLoggers tag owning the PubSub line


def draw_network_id() -> int:
    # ponytail: uniform draw from the reserved band. Cross-run collision chance
    # is ~1/4096 per pair and harmless (a colliding id still isolates this run
    # from every persistent net). If concurrent CI runs ever collide, widen or
    # serialize the draw then.
    low, high = NETWORK_ID_RESERVED_RANGE
    return low + secrets.randbelow(high - low + 1)


def write_key_file(path: str, key_hex: str) -> None:
    # O_EXCL + 0600 inside the 0700 run dir: no symlink race, no loose perms
    # (Pitfall 7); the runner re-enforces the policy independently at boot.
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w") as handle:
        handle.write(key_hex + "\n")
    os.chmod(path, 0o600)  # umask may tighten a fresh file; force the policy mode


def _write_json(path: str, payload: dict) -> None:
    with open(path, "w") as handle:
        json.dump(payload, handle, indent=2)
        handle.write("\n")


def _node_dir(run_dir: str, index: int) -> str:
    # Trailing slash is a contract: service.cpp concatenates config file names
    # with no separator (Pitfall 3).
    return os.path.join(run_dir, "node%d" % index) + "/"


def build_topology(run_dir: str, nodes: int, runner_default_ports: bool = False) -> dict:
    """Generate identities + every pre-boot config and the topology manifest.

    runner_default_ports=True omits port_seed from network_config.json files
    (the runner's 40001 default applies) — single-node compatibility only.
    """
    if nodes < 1:
        raise ValueError("nodes must be >= 1")
    network_id = draw_network_id()

    keys_dir = os.path.join(run_dir, "keys")
    os.makedirs(keys_dir, exist_ok=True)

    identities = []  # (key_path, address) per node in 1-based order
    for i in range(1, nodes + 1):
        key_hex = secp256k1_address.generate_key_hex()
        key_path = os.path.join(keys_dir, "node%d.key" % i)
        write_key_file(key_path, key_hex)
        identities.append((key_path, secp256k1_address.ceremony_address(key_hex)))

    bootstrapper = identities[0][1]
    # THE peer-set fact (one home): every sgns_config below and plan 02-03's
    # make-manifest --peers call read exactly this list.
    peer_set = [address for _, address in identities[1:]]

    node_entries = []
    for i, (key_path, address) in enumerate(identities, start=1):
        base_dir = _node_dir(run_dir, i)
        os.makedirs(base_dir, exist_ok=True)
        _write_json(os.path.join(base_dir, "dev_config.json"), {
            "Address": "0xcafe",
            "Cut": "0.65",
            "TokenValue": "1.0",
            "TokenID": "0x" + "0" * 64,
        })
        # sgns_config.json is written by write_trust_configs once node 1's
        # account address B_1 is known (authorized_full_node wiring).
        # The node logger sinks to <base>/sgnslog2.log at err level in this
        # build (InitLoggers L1297); promote the one logger that owns the
        # PubSub multiaddr line (GeniusNode.cpp L1671) to info so the staged
        # bootstrap gate can observe it (RESEARCH Q3 observability knob).
        _write_json(os.path.join(base_dir, "log_config.json"), {
            "loggers": {NODE_LOGGER_NAME: "info"},
        })
        port_seed = NODE_PORT_BASE + (i - 1) * NODE_PORT_STRIDE
        network_config_path = os.path.join(base_dir, "network_config.json")
        if i == 1:
            # Phase A: only the bootstrapper's network config is complete
            # pre-boot (empty bootstrap list); peers get theirs in
            # write_peer_configs once node 1's multiaddr is observed.
            network_config = {"auto_dht": True, "bootstrap_addresses": []}
            if not runner_default_ports:
                network_config["port_seed"] = port_seed
            _write_json(network_config_path, network_config)
        node_entries.append({
            "name": "node%d" % i,
            "base_dir": base_dir,
            "key_file": key_path,
            "ceremony_address": address,
            "port_seed": port_seed,
            "network_config": network_config_path,
            "node_log": os.path.join(base_dir, "sgnslog2.log"),
        })

    actor_entries = []
    for i in range(1, nodes + 1):
        database = os.path.join(run_dir, "actor%ddb" % i)
        os.makedirs(database, exist_ok=True)  # reserve; DBs are disjoint trees
        actor_entries.append({
            "name": "actor%d" % i,
            "key_file": identities[i - 1][0],
            "network_config": os.path.join(run_dir, "actor%d-network.json" % i),
            "database": database,
            "pubsub_port": str(ACTOR_PORT_BASE + i - 1),  # STRING (actor parser)
        })

    # Manifest carries key file PATHS only — never key bytes (T-02-06).
    topology = {
        "run_dir": run_dir,
        "network_id": network_id,
        "topic": TOPIC,
        "bootstrapper": bootstrapper,
        "peer_set": peer_set,
        "runner_default_ports": runner_default_ports,
        "nodes": node_entries,
        "actors": actor_entries,
        "port_plan": {
            "node_port_base": NODE_PORT_BASE,
            "node_port_stride": NODE_PORT_STRIDE,
            "actor_port_base": ACTOR_PORT_BASE,
        },
    }
    _write_json(os.path.join(run_dir, "topology.json"), topology)
    return topology


_HEX = set("0123456789abcdef")


def write_trust_configs(topology: dict, authorized_full_node: str) -> None:
    """sgns_config.json for every node (phase A2 — after B_1 discovery).

    authorized_full_node is the bootstrapper's SDK ACCOUNT address B_1 (read
    from node1/secure_storage_id after its identity boot; KDF-derived, NOT the
    ceremony address A_1 — research Fact 5). Blockchain::Start defers until
    the genesis validator registry exists, and only the node whose account
    address equals authorized_full_node writes it
    (Blockchain::EnsureValidatorRegistry); everyone else waits for its
    broadcast — so without this key every node strands in
    INITIALIZING_BLOCKCHAIN and never reaches WAITING_FOR_TRUST_GENESIS.
    """
    if not (len(authorized_full_node) == 128
            and set(authorized_full_node) <= _HEX):
        raise ValueError("authorized_full_node must be a 128-hex address")
    for entry in topology["nodes"]:
        # Floor-parity anti-strand invariant: trusted_peers carries ALL N-1
        # non-bootstrapper addresses — the exact set make-manifest receives as
        # --peers (D-12b). The node rebuilds its expected genesis manifest from
        # these fields and defaults quorum floors from trusted_peers.size()
        # (GeniusNode.cpp L441-449), so a shorter list computes floors that can
        # never match the manifest's and every node strands in
        # WAITING_FOR_TRUST_GENESIS. subnet_id MUST equal the manifest
        # network-id (Pitfall 10).
        _write_json(os.path.join(entry["base_dir"], "sgns_config.json"), {
            "node_type": "Full",
            "subnet_id": topology["network_id"],
            "authorized_full_node": authorized_full_node,
            "bootstrapper_node": topology["bootstrapper"],
            "trusted_peers": list(topology["peer_set"]),
        })


def write_peer_configs(topology: dict, node1_multiaddr: str) -> None:
    """Phase B: configs that need the bootstrapper's live PubSub multiaddr.

    node1_multiaddr must be the full multiaddr WITH /p2p/<peer-id>
    (ParsePeerInfoFromString requires it — Pitfall 5).
    """
    for entry in topology["nodes"][1:]:
        network_config = {"auto_dht": True, "bootstrap_addresses": [node1_multiaddr]}
        if not topology["runner_default_ports"]:
            network_config["port_seed"] = entry["port_seed"]
        _write_json(entry["network_config"], network_config)
    for actor in topology["actors"]:
        # Actor network config is parsed by DIFFERENT code (pubsub_port is a
        # string here); never share one file between a node and an actor.
        _write_json(actor["network_config"], {
            "pubsub_port": actor["pubsub_port"],
            "pubsub_bind_address": "0.0.0.0",
            "bootstrap_addresses": [node1_multiaddr],
        })


def _read_json(path: str):
    with open(path) as handle:
        return json.load(handle)


def _selftest() -> None:
    run_dir = tempfile.mkdtemp(prefix="topology-selftest-")
    try:
        topology = build_topology(run_dir, 5)
        with open(os.path.join(run_dir, "topology.json")) as handle:
            manifest_text = handle.read()

        # Key files: regular, not symlinks, exactly 0600; the manifest carries
        # paths only — no key's 64-hex form may appear in its text (T-02-06).
        for entry in topology["nodes"]:
            assert os.path.isfile(entry["key_file"])
            assert not os.path.islink(entry["key_file"])
            assert (os.stat(entry["key_file"]).st_mode & 0o777) == 0o600
            with open(entry["key_file"]) as handle:
                key_hex = handle.read().strip()
            assert len(key_hex) == 64
            assert key_hex not in manifest_text

        # Addresses: 5 distinct 128-hex strings; manifest set facts agree.
        addresses = [entry["ceremony_address"] for entry in topology["nodes"]]
        assert len(set(addresses)) == 5
        for address in addresses:
            assert len(address) == 128
            assert all(c in "0123456789abcdef" for c in address)
        assert topology["bootstrapper"] == addresses[0]
        assert topology["peer_set"] == addresses[1:]

        # Reserved band (NET-05).
        low, high = NETWORK_ID_RESERVED_RANGE
        assert low <= topology["network_id"] <= high

        # Phase A: node 1's configs parse; no trust configs exist yet (phase
        # A2 writes them only after B_1 discovery).
        node1 = topology["nodes"][0]
        for name in ("dev_config.json", "network_config.json", "log_config.json"):
            _read_json(os.path.join(node1["base_dir"], name))
        assert not os.path.exists(os.path.join(node1["base_dir"], "sgns_config.json"))
        node_seeds = [node1["port_seed"]]
        for entry in topology["nodes"][1:]:
            assert not os.path.exists(entry["network_config"])  # phase B not yet
            assert not os.path.exists(os.path.join(entry["base_dir"], "sgns_config.json"))
            node_seeds.append(entry["port_seed"])

        # Phase A2: trust configs with the authorized account address.
        authorized = "ab" * 64
        write_trust_configs(topology, authorized)
        try:
            write_trust_configs(topology, "not-hex")
        except ValueError:
            pass
        else:
            raise AssertionError("non-128-hex authorized address must be rejected")
        for entry in topology["nodes"]:
            sgns = _read_json(os.path.join(entry["base_dir"], "sgns_config.json"))
            assert sgns["node_type"] == "Full"
            assert sgns["subnet_id"] == topology["network_id"]
            assert sgns["authorized_full_node"] == authorized
            assert sgns["bootstrapper_node"] == topology["bootstrapper"]
            assert sgns["trusted_peers"] == topology["peer_set"]

        # Ports: node seeds pairwise distinct and spaced >300 (Pitfall 4);
        # actor ports pairwise distinct and in a disjoint band.
        seeds = sorted(node_seeds)
        assert all(b - a > 300 for a, b in zip(seeds, seeds[1:]))
        actor_ports = [int(actor["pubsub_port"]) for actor in topology["actors"]]
        assert len(set(actor_ports)) == len(actor_ports)
        assert all(isinstance(actor["pubsub_port"], str)
                   for actor in topology["actors"])
        assert max(seeds) + 300 < min(actor_ports)

        # Actor DB dirs reserved, disjoint from node dirs.
        for actor in topology["actors"]:
            assert os.path.isdir(actor["database"])

        # Phase B round-trip with a stand-in multiaddr.
        multiaddr = "/ip4/127.0.0.1/tcp/41037/p2p/QmSelftestPeer"
        write_peer_configs(topology, multiaddr)
        for entry in topology["nodes"][1:]:
            net = _read_json(entry["network_config"])
            assert net["port_seed"] == entry["port_seed"]
            assert net["bootstrap_addresses"] == [multiaddr]
        for actor in topology["actors"]:
            cfg = _read_json(actor["network_config"])
            assert cfg["pubsub_port"] == actor["pubsub_port"]
            assert isinstance(cfg["pubsub_port"], str)
            assert cfg["bootstrap_addresses"] == [multiaddr]
    finally:
        shutil.rmtree(run_dir)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Selftest for topology/config generation (5-node build "
                    "into a temp dir; asserts parity, ports, secret-free "
                    "manifest).")
    parser.add_argument("--selftest", action="store_true",
                        help="build a 5-node topology and assert invariants")
    args = parser.parse_args()
    if args.selftest:
        _selftest()
        print("topology selftest PASS")
        return
    parser.error("nothing to do; use --selftest")


if __name__ == "__main__":
    main()
