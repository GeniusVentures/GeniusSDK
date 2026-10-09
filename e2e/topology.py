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
the bootstrapper's live PubSub multiaddr. Phase C (write_joiner_configs):
late-joiner configs — a non-ceremony node booting WITHOUT --key-file (the
Phase 5 joiner shape), so no joiner key ever exists and the D-07 scan scope
stays the topology node keys.

The topology manifest is the single source every later stage reads; peer_set is
THE peer-set fact shared by sgns_config generation here and plan 02-03's
make-manifest --peers call (one home per rule, D-12b).

Phase E (04-02, D-09/D-14): the STATIC FLEET generator trio — gen_fleet_keys
(5 persistent identities to an offline dir + the fleet.json manifest, key file
PATHS only), write_fleet_configs (per-node config bundle directories for the
real boxes), write_fleet_trust_configs (the post-B_1 full-payload push 04-03
drives) — and write_artifact_joiners (join mode: consume the published
staging-net.json artifact set, write joiner configs, boot WITHOUT --key-file).

Usage:
  python3 topology.py --selftest
  python3 topology.py --gen-fleet-keys --keys-dir <offline-dir> --out-fleet <fleet.json>
  python3 topology.py --static-fleet <fleet.json> --out-dir <bundle-dir>
  python3 topology.py --join <artifact-dir> --out-dir <dir> [--count N]
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

# D-06: the staging net id, mirroring sgns_version.hpp's registry sibling —
# the ONE sanctioned non-reserved value the harness may pin (the dht-health
# pre-provision proof; every default draw stays in the reserved band).
STAGING_NET_ID = 333

# sgns::version::SetNetworkId's accept-list, mirrored as the one Python-side
# fact for any config that pins net_id (sgns_config "net_id"): a value
# outside this tuple kills the node at init with "Invalid network ID"
# (Pitfall 1 — the RED half of 04-01's RED->GREEN pair proves it live).
# Order mirrors sgns_version.hpp: MAIN, TEST, DEV, STAGING.
REGISTRY_NET_IDS = (369, 963, 144, STAGING_NET_ID)

TOPIC = "SuperGNUSNode.TestNet.FullNode"  # GNUS_FULL_NODES_TOPIC constant (Pitfall 6)

NODE_PORT_BASE = 41001     # per-node port_seeds: base + (i-1)*500 (Pitfall 4:
NODE_PORT_STRIDE = 500     # resolution adds hash%301, so >300 spacing kills collisions)
ACTOR_PORT_BASE = 45501    # actor pubsub_port strings: disjoint band, base + (i-1)

NODE_LOGGER_NAME = "SuperGeniusNode"  # InitLoggers tag owning the PubSub line
BLOCKCHAIN_LOGGER_NAME = "Blockchain"  # InitLoggers tag owning the joiner sync lines

# Every log_config this module writes promotes the SAME logger set (one home,
# one shape — nodes and joiners get an identical set): the node logger owns
# the PubSub multiaddr line, the Blockchain logger owns the joiner sync
# anchors ("Request succeeded for Genesis", Blockchain.cpp L703 — Pitfall 2:
# it is NOT SuperGeniusNode; without this promotion those info lines never
# reach sgnslog2.log).
INFO_LOGGERS = {
    NODE_LOGGER_NAME: "info",
    BLOCKCHAIN_LOGGER_NAME: "info",
    "GossipPubSub": "info",  # owns the DHT observables ("DHT: New Peer: <pid>")
}

# The canonical staging fleet is the 5-node ceremony (genesis on the AWS box +
# 4 NAS peers); joiner port seeds continue the node band after it (one home
# for the band continuation join mode uses).
STAGING_FLEET_NODES = 5

# WAN ceremony margins recorded in every fleet manifest — provisional until
# 04-03's live fleet run calibrates them (loopback defaults are ceremony.py's
# 90/60; both are argv-tunable at every call site).
FLEET_SERVE_SECONDS = 180
FLEET_TIMEOUT_SECONDS = 120

# The co-located existing node on the AWS box (the services/network_config.json
# shape) — the collision-check input every fleet port seed must clear by >300.
# Correct it in fleet.json if the real box runs a different port.
FLEET_EXISTING_NODE_PORT = 40001


def draw_network_id() -> int:
    # ponytail: uniform draw from the reserved band. Cross-run collision chance
    # is ~1/4096 per pair and harmless (a colliding id still isolates this run
    # from every persistent net). If concurrent CI runs ever collide, widen or
    # serialize the draw then.
    low, high = NETWORK_ID_RESERVED_RANGE
    return low + secrets.randbelow(high - low + 1)


def validate_network_id(network_id: int) -> None:
    """One home for the NET-05 id policy (T-04-01-02): accept EXACTLY the
    reserved ephemeral band or the sanctioned STAGING_NET_ID — no open range.
    The staging escape (Pitfall 6) is what lets the dht-health gate, static
    fleet mode, and join mode consume the pinned 333; everything else keeps
    raising the original band ValueError."""
    low, high = NETWORK_ID_RESERVED_RANGE
    if not (low <= network_id <= high or network_id == STAGING_NET_ID):
        raise ValueError("network_id %d outside the reserved band %d-%d "
                         "(T-03-05)" % (network_id, low, high))


def make_dns4_multiaddr(hostname: str, port, peer_id: str) -> str:
    """/dns4/<hostname>/tcp/<port>/ipfs/<peer_id> — the DNS-named entry form
    (D-04). The /ipfs/ peer-id suffix is required by the bootstrap parser
    (ParsePeerInfoFromString rejects a bare address — Pitfall 5); dial-time
    resolution lives in the vendored TCP transport."""
    return "/dns4/%s/tcp/%d/ipfs/%s" % (hostname, int(port), peer_id)


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


def _write_dev_config(base_dir: str) -> None:
    # One home for the dev-config shape: every node AND every joiner writes
    # exactly these four keys (the SuperGenius parser reads them as strings).
    _write_json(os.path.join(base_dir, "dev_config.json"), {
        "Address": "0xcafe",
        "Cut": "0.65",
        "TokenValue": "1.0",
        "TokenID": "0x" + "0" * 64,
    })


def _write_log_config(base_dir: str) -> None:
    # One home for the promoted-logger set (INFO_LOGGERS above).
    _write_json(os.path.join(base_dir, "log_config.json"),
                {"loggers": dict(INFO_LOGGERS)})


def build_topology(run_dir: str, nodes: int, runner_default_ports: bool = False,
                   network_id: int = None) -> dict:
    """Generate identities + every pre-boot config and the topology manifest.

    runner_default_ports=True omits port_seed from network_config.json files
    (the runner's 40001 default applies) — single-node compatibility only.
    network_id=None draws from the reserved band exactly as before; an
    explicit value flows through validate_network_id (the staging pin 333 is
    the one sanctioned non-reserved value — the dht-health pre-provision
    proof; existing callers are unchanged).
    """
    if nodes < 1:
        raise ValueError("nodes must be >= 1")
    if network_id is None:
        network_id = draw_network_id()
    else:
        validate_network_id(network_id)

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
        _write_dev_config(base_dir)
        # sgns_config.json is written by write_trust_configs once node 1's
        # account address B_1 is known (authorized_full_node wiring).
        # The node logger sinks to <base>/sgnslog2.log at err level in this
        # build (InitLoggers L1297); the promoted logger set (INFO_LOGGERS)
        # makes the PubSub multiaddr line (GeniusNode.cpp L1671) and the
        # Blockchain sync lines observable there (RESEARCH Q3 / Pitfall 2).
        _write_log_config(base_dir)
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
        entry = {
            "name": "actor%d" % i,
            "key_file": identities[i - 1][0],
            "network_config": os.path.join(run_dir, "actor%d-network.json" % i),
            "database": database,
            "pubsub_port": str(ACTOR_PORT_BASE + i - 1),  # STRING (actor parser)
        }
        if network_id == STAGING_NET_ID:
            # Every pubsub/DHT topic an actor joins carries the net appendix
            # (PubsubBroadcasterExt), so a staging actor MUST run the tool
            # with --net-id 333 or its topics (.3.7.144) never meet the
            # staging nodes' (.3.7.333) — the CRDT catch-up starves (observed
            # live, 04-01). ceremony._shared_argv consumes this key.
            entry["net_id"] = STAGING_NET_ID
        actor_entries.append(entry)

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


def _expect_128_hex(address: str, what: str) -> None:
    # One home for the address-shape check (write_trust_configs and the
    # joiner writer consume the same fact: SDK account addresses are 128 hex).
    if not (len(address) == 128 and set(address) <= _HEX):
        raise ValueError("%s must be a 128-hex address (got %r)"
                         % (what, address[:16] + "..."))


def _sgns_trust_payload(topology: dict, authorized_full_node: str) -> dict:
    """The ONE sgns_config trust shape — every ceremony node AND every joiner
    writes exactly this payload (observed live, 03-02: without these keys a
    node either rejects the net's genesis — authorized_full_node — or fails
    closed with 'no trusted peers configured and no persisted trust state' —
    trusted_peers/bootstrapper_node; a joiner differs from a ceremony node
    only by booting WITHOUT --key-file).

    Floor-parity anti-strand invariant: trusted_peers carries ALL N-1
    non-bootstrapper addresses — the exact set make-manifest receives as
    --peers (D-12b). The node rebuilds its expected genesis manifest from
    these fields and defaults quorum floors from trusted_peers.size()
    (GeniusNode.cpp L441-449), so a shorter list computes floors that can
    never match the manifest's and every node strands in
    WAITING_FOR_TRUST_GENESIS. subnet_id MUST equal the manifest
    network-id (Pitfall 10).

    Staging pin (04-01): when the topology's network_id is exactly
    STAGING_NET_ID, the payload additionally pins net_id (the C++ net
    registry id that scopes the DHT keyspace and globaldb path) — without
    it a staging node silently lands on the DEV default 144 and shares the
    dev-net DHT CID. Reserved-band runs add no net_id key: the registry
    rejects the whole band (Pitfall 1), so their isolation rides subnet_id
    + ports exactly as in Phase 2-3.
    """
    payload = {
        "node_type": "Full",
        "subnet_id": topology["network_id"],
        "authorized_full_node": authorized_full_node,
        "bootstrapper_node": topology["bootstrapper"],
        "trusted_peers": list(topology["peer_set"]),
    }
    if topology["network_id"] == STAGING_NET_ID:
        payload["net_id"] = STAGING_NET_ID
    return payload


def write_identity_config(topology: dict) -> None:
    """Minimal sgns_config for node1's identity boot — ONLY when the run
    pins net_id (staging). The C++ net id scopes the node's globaldb path,
    where the libp2p keypair persists: an identity boot on the default net
    (no config) writes the keypair under the 144 path, the staging final
    boot then looks under the 333 path, finds nothing, generates a FRESH
    peer id — and every peer dials the identity boot's captured address,
    which nobody answers as (observed live, 04-01: node1's listener id !=
    its advertised id; the whole mesh starved). The minimal shape is the
    same one write_foreign_net_configs emits (node_type + net_id, no trust
    keys — B_1 is the thing this boot exists to discover; write_trust_configs
    overwrites it with the full payload afterwards). Reserved-band runs pin
    no net id, so this is a no-op for them: both boots share the silent
    default exactly as in Phase 2-3."""
    if topology["network_id"] != STAGING_NET_ID:
        return
    _write_json(os.path.join(topology["nodes"][0]["base_dir"],
                             "sgns_config.json"),
                _minimal_identity_payload(STAGING_NET_ID))


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
    _expect_128_hex(authorized_full_node, "authorized_full_node")
    for entry in topology["nodes"]:
        _write_json(os.path.join(entry["base_dir"], "sgns_config.json"),
                    _sgns_trust_payload(topology, authorized_full_node))


def write_peer_configs(topology: dict, node1_multiaddr: str,
                       node_multiaddr_map: dict = None) -> None:
    """Phase B: configs that need the bootstrapper's live PubSub multiaddr.

    node1_multiaddr must be the full multiaddr WITH /p2p/<peer-id>
    (ParsePeerInfoFromString requires it — Pitfall 5).

    node_multiaddr_map (dht-health split bootstrap): optional {node name:
    multiaddr} — each named peer's bootstrap_addresses take that entry
    instead of node1's; names absent from the map (and the None default)
    keep node1 exactly as in Phase 2. The two-wave dht-health boot calls
    this twice: first with only node2's /dns4/ entry, then with the final
    map once node2's own multiaddr is observed (rewrites are boot-time
    reads only — nodes 3-5 have not spawned yet, node2 re-reads nothing).
    """
    for entry in topology["nodes"][1:]:
        bootstrap = node1_multiaddr
        if node_multiaddr_map is not None:
            bootstrap = node_multiaddr_map.get(entry["name"], node1_multiaddr)
        network_config = {"auto_dht": True, "bootstrap_addresses": [bootstrap]}
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


def write_joiner_configs(topology: dict, node1_multiaddr: str,
                         authorized_full_node: str, count: int = 1) -> list:
    """Phase C: configs for late joiners (non-ceremony nodes), plus the
    joiners[] manifest entries.

    A joiner boots WITHOUT --key-file — a fresh account, exactly the Phase 5
    joiner shape — so no joiner key ever exists and the D-07 scan scope stays
    the topology node keys. node1_multiaddr mirrors write_peer_configs: it is
    the live multiaddr THIS run's boot gate captured, so bootstrap_addresses
    only ever point at node1 of this topology (T-03-05: no cross-net
    contamination; subnet_id is asserted in the reserved band at write time).

    authorized_full_node (B_1, same fact write_trust_configs consumes): a
    joiner may not CREATE genesis, but Blockchain::VerifyGenesisBlock rejects
    any genesis whose creator != the authorized address — without the key the
    joiner falls back to the compile-time default, refuses the net's genesis
    ("unauthorized key"), and strands in INITIALIZING_BLOCKCHAIN forever
    (observed live, 03-02). And the FULL trust wiring (bootstrapper_node +
    trusted_peers) is equally mandatory: TrustStartupController fail-closes
    ("No trusted peers configured and no persisted trust state; refusing
    unrestricted boot" — observed live, 03-02) because a fresh joiner has no
    persisted trust state and must rebuild the expected manifest from config
    (GeniusNode.cpp L301-427 parses every key optional). The joiner therefore
    writes the SAME sgns_config payload as every node (_sgns_trust_payload —
    one home); it differs from a ceremony node ONLY by booting without
    --key-file. Port seed continues the node band after the last node
    (41001 + (N + i - 1) * 500): >300 spaced like every node and clear of the
    actor band.
    """
    if count < 1:
        raise ValueError("count must be >= 1")
    _expect_128_hex(authorized_full_node, "authorized_full_node")
    # One home for the band policy since 04-01: the reserved band OR exactly
    # the sanctioned staging id (the Pitfall 6 escape — join mode consumes
    # the artifact's pinned 333 through here).
    validate_network_id(topology["network_id"])
    joiners = []
    node_count = len(topology["nodes"])
    for i in range(1, count + 1):
        base_dir = os.path.join(topology["run_dir"], "joiner%d" % i) + "/"
        os.makedirs(base_dir, exist_ok=True)
        _write_dev_config(base_dir)
        _write_log_config(base_dir)
        port_seed = NODE_PORT_BASE + (node_count + i - 1) * NODE_PORT_STRIDE
        network_config = {"auto_dht": True, "bootstrap_addresses": [node1_multiaddr]}
        if not topology["runner_default_ports"]:
            network_config["port_seed"] = port_seed
        network_config_path = os.path.join(base_dir, "network_config.json")
        _write_json(network_config_path, network_config)
        _write_json(os.path.join(base_dir, "sgns_config.json"),
                    _sgns_trust_payload(topology, authorized_full_node))
        joiners.append({
            "name": "joiner%d" % i,
            "base_dir": base_dir,
            "network_config": network_config_path,
            "node_log": os.path.join(base_dir, "sgnslog2.log"),
            "port_seed": port_seed,
        })
    topology["joiners"] = joiners
    # The manifest is the single source every later stage reads; re-emit it
    # with the joiners[] entries (paths only, like every other entry).
    _write_json(os.path.join(topology["run_dir"], "topology.json"), topology)
    return joiners


def write_foreign_net_configs(run_dir: str, net_id: int) -> dict:
    """The co-located foreign-net node (dht-health coexistence gate, D-01
    amended): a 6th process on the SAME machine under a DIFFERENT net
    registry id — the AWS-box co-location pattern proven at zero spend.

    Its sgns_config is deliberately minimal ({"node_type": "Full", "net_id":
    <id>} — no trust wiring: an unwired node strands in an init state and
    STAYS ALIVE, which is exactly what the gate observes; it must never take
    part in anything). net_id must be inside REGISTRY_NET_IDS and differ
    from the staging net, or the node dies at init (Pitfall 1). Empty
    bootstrap list: it is never told about the staging mesh — any contact
    would be contamination, and the gate asserts none happens. The port seed
    takes the band slot after the 5 staging nodes (41001 + 5*500); the
    logger set rides the same _write_log_config home, so the GossipPubSub
    promotion makes its "CID Test::" line observable too.
    """
    if net_id not in REGISTRY_NET_IDS:
        raise ValueError("foreign net_id %r outside the C++ registry %s — "
                         "the node would die at init (Pitfall 1)"
                         % (net_id, REGISTRY_NET_IDS))
    base_dir = os.path.join(run_dir, "foreign-net") + "/"
    os.makedirs(base_dir, exist_ok=True)
    _write_dev_config(base_dir)
    _write_log_config(base_dir)
    port_seed = NODE_PORT_BASE + 5 * NODE_PORT_STRIDE
    _write_json(os.path.join(base_dir, "network_config.json"),
                {"auto_dht": True, "bootstrap_addresses": [],
                 "port_seed": port_seed})
    _write_json(os.path.join(base_dir, "sgns_config.json"),
                _minimal_identity_payload(net_id))
    return {
        "name": "foreign-net",
        "base_dir": base_dir,
        "net_id": net_id,
        "port_seed": port_seed,
        "node_log": os.path.join(base_dir, "sgnslog2.log"),
    }


def _minimal_identity_payload(net_id: int) -> dict:
    """The net-pinned boot shape with NO trust wiring — ONE home shared by the
    genesis identity boot (write_identity_config), the co-located foreign-net
    node (write_foreign_net_configs), and the static-fleet genesis bundle
    (write_fleet_configs): {"node_type": "Full", "net_id": <registry id>}."""
    return {"node_type": "Full", "net_id": net_id}


def _validate_fleet(fleet: dict) -> None:
    """One home for the fleet-manifest invariants write_fleet_configs and
    write_fleet_trust_configs consume (D-09: the manifest is generated,
    validated code — never hand-trusted): THE staging net, address/peer-set
    agreement, port seeds unique + spaced >300 + >300 clear of the co-located
    AWS node's port, positive WAN margins, and a filled /dns4/ genesis
    published multiaddr (D-04 — the one address every peer and joiner dials)."""
    net = fleet.get("network_id")
    if not isinstance(net, int):
        raise ValueError("fleet network_id must be an int (got %r)" % (net,))
    validate_network_id(net)
    if net != STAGING_NET_ID:
        raise ValueError("static-fleet mode writes THE staging net: "
                         "network_id must be %d (got %r)"
                         % (STAGING_NET_ID, net))
    nodes = fleet.get("nodes") or []
    if len(nodes) < 2:
        raise ValueError("fleet needs >= 2 nodes (got %d)" % len(nodes))
    if nodes[0].get("role") != "genesis":
        raise ValueError("nodes[0] role must be 'genesis'")
    if fleet.get("bootstrapper") != nodes[0].get("ceremony_address"):
        raise ValueError("bootstrapper must be node1's ceremony_address")
    if list(fleet.get("peer_set") or []) != \
            [node.get("ceremony_address") for node in nodes[1:]]:
        raise ValueError("peer_set must be exactly nodes 2..N addresses")
    for node in nodes:
        _expect_128_hex(node["ceremony_address"],
                        "%s ceremony_address" % node.get("name"))
        kind = (node.get("transport") or {}).get("kind")
        if kind not in ("ssh", "compose", "local"):
            raise ValueError("node %s transport kind must be ssh|compose|local"
                             " (got %r)" % (node.get("name"), kind))
        if not isinstance(node.get("port_seed"), int) or \
                node["port_seed"] <= 0:
            raise ValueError("node %s port_seed must be a positive int"
                             % node.get("name"))
    seeds = [node["port_seed"] for node in nodes]
    ordered = sorted(seeds)
    if len(set(ordered)) != len(ordered) or \
            not all(b - a > 300 for a, b in zip(ordered, ordered[1:])):
        raise ValueError("fleet port seeds must be unique and spaced >300 "
                         "(Pitfall 4): %s" % seeds)
    existing = fleet.get("existing_node_port")
    if existing is not None:
        if not isinstance(existing, int) or existing <= 0:
            raise ValueError("existing_node_port must be a positive int or "
                             "null (got %r)" % (existing,))
        clash = [seed for seed in seeds if abs(seed - existing) <= 300]
        if clash:
            raise ValueError("fleet port seeds %s within 300 of the "
                             "co-located node's port %d" % (clash, existing))
    for field in ("serve_seconds", "timeout_seconds"):
        value = fleet.get(field)
        if not isinstance(value, int) or value <= 0:
            raise ValueError("fleet %s must be a positive int (got %r)"
                             % (field, value))
    genesis_multiaddr = nodes[0].get("published_multiaddr", "")
    if not genesis_multiaddr.startswith("/dns4/"):
        raise ValueError("genesis published_multiaddr must be the /dns4/ form "
                         "(got %r)" % (genesis_multiaddr[:40],))
    if "PLACEHOLDER" in genesis_multiaddr:
        raise ValueError("genesis published_multiaddr still carries a "
                         "PLACEHOLDER — fill the real DNS name and the "
                         "genesis peer id (captured at its identity boot) "
                         "before generating bundles")


def gen_fleet_keys(keys_dir: str, out_fleet: str) -> dict:
    """Generate the 5 PERSISTENT staging identities into an operator-chosen
    OFFLINE dir (D-08: 0600, held offline, never committed) and write the
    fleet.json manifest carrying key file PATHS only — zero key hex
    (T-04-02-01). write_key_file's O_EXCL makes a re-run into the same dir
    fail loudly: persistent identities are generated ONCE.

    The manifest mirrors topology.json's field names where facts coincide
    (network_id, bootstrapper, peer_set, per-node name/port_seed) and adds
    the remote facts as PATHS/handles the operator completes before
    --static-fleet: transport (ssh genesis box / compose NAS peers / local
    parity fixture), base_dir on the box, and the genesis
    published_multiaddr (filled once the identity boot captured the peer id).
    """
    os.makedirs(keys_dir, exist_ok=True)
    addresses = []
    nodes = []
    for i in range(1, STAGING_FLEET_NODES + 1):
        key_hex = secp256k1_address.generate_key_hex()
        key_path = os.path.join(keys_dir, "node%d.key" % i)
        write_key_file(key_path, key_hex)
        address = secp256k1_address.ceremony_address(key_hex)
        addresses.append(address)
        port_seed = NODE_PORT_BASE + (i - 1) * NODE_PORT_STRIDE
        if i == 1:
            transport = {"kind": "ssh", "host": "PLACEHOLDER-AWS-HOST",
                         "unit_name": "sgns-staging-node.service"}
            base_dir = "/var/lib/geniussdk/staging-genesis/"
        else:
            transport = {"kind": "compose", "service": "staging-peer-%d" % i}
            base_dir = "/volume1/staging/peer-%d/" % i
        nodes.append({
            "name": "node%d" % i,
            "role": "genesis" if i == 1 else "peer",
            "ceremony_address": address,
            "key_file": os.path.abspath(key_path),
            "base_dir": base_dir,
            "port_seed": port_seed,
            "published_multiaddr": make_dns4_multiaddr(
                "staging-genius.placeholder.invalid", port_seed,
                "PLACEHOLDER-PEER-ID"),
            "transport": transport,
        })
    fleet = {
        "network_id": STAGING_NET_ID,
        "topic": TOPIC,
        "bootstrapper": addresses[0],
        "peer_set": addresses[1:],
        "serve_seconds": FLEET_SERVE_SECONDS,
        "timeout_seconds": FLEET_TIMEOUT_SECONDS,
        "existing_node_port": FLEET_EXISTING_NODE_PORT,
        "nodes": nodes,
    }
    _write_json(out_fleet, fleet)
    return fleet


def write_fleet_configs(fleet: dict, out_dir: str) -> dict:
    """Write the static-fleet config bundles (D-09): ONE directory per node
    under out_dir/<name>/ — the unit 04-04 mounts (NAS peers) or scps (the
    AWS genesis box) as a whole. Every node gets dev/log configs and a
    network_config with its own port_seed; the GENESIS node additionally
    gets the minimal identity-boot sgns_config (the write_identity_config
    shape, one home — its first boot exists to capture B_1 and the peer id);
    PEERS bootstrap to the genesis published /dns4/ multiaddr (amended D-04:
    the ONE public entry address, also what the NAS peers dial from behind
    home NAT). No key material is written — keys are staged offline by
    provisioning. Records bundle_dir on each node and returns the fleet."""
    _validate_fleet(fleet)
    genesis_multiaddr = fleet["nodes"][0]["published_multiaddr"]
    os.makedirs(out_dir, exist_ok=True)
    for node in fleet["nodes"]:
        node_dir = os.path.join(out_dir, node["name"]) + "/"
        os.makedirs(node_dir, exist_ok=True)
        _write_dev_config(node_dir)
        _write_log_config(node_dir)
        _write_json(os.path.join(node_dir, "network_config.json"), {
            "auto_dht": True,
            "port_seed": node["port_seed"],
            "bootstrap_addresses": ([] if node["role"] == "genesis"
                                    else [genesis_multiaddr]),
        })
        if node["role"] == "genesis":
            _write_json(os.path.join(node_dir, "sgns_config.json"),
                        _minimal_identity_payload(STAGING_NET_ID))
        node["bundle_dir"] = node_dir
    return fleet


def write_fleet_trust_configs(fleet: dict, authorized_full_node: str) -> None:
    """The post-B_1 push (04-03's extension point): once the genesis identity
    boot's secure_storage_id yields B_1, write the FULL trust payload — the
    SAME _sgns_trust_payload every ceremony node writes, net_id pinned beside
    subnet_id — into every node's bundle dir, overwriting the genesis
    identity-boot config exactly as the ephemeral write_trust_configs does."""
    for node in fleet.get("nodes") or []:
        if not node.get("bundle_dir"):
            raise ValueError("node %s has no bundle_dir — run "
                             "write_fleet_configs first" % node.get("name"))
    net = fleet.get("network_id")
    if not isinstance(net, int) or net != STAGING_NET_ID:
        raise ValueError("fleet network_id must be exactly the staging id %d "
                         "(got %r)" % (STAGING_NET_ID, net))
    _expect_128_hex(authorized_full_node, "authorized_full_node")
    for node in fleet["nodes"]:
        _write_json(os.path.join(node["bundle_dir"], "sgns_config.json"),
                    _sgns_trust_payload(fleet, authorized_full_node))


def write_artifact_joiners(artifact_dir: str, out_dir: str,
                           count: int = 1) -> list:
    """Join mode (D-14, NET-04): consume the PUBLISHED artifact set's
    staging-net.json and write joiner config bundles — a joiner boots WITHOUT
    --key-file (no key file is ever written; a fresh account, the Phase 5
    joiner shape) with the FULL trust payload rebuilt from the artifact's
    facts (03-02 A3: joiners need the SAME payload, or they refuse the net's
    genesis and fail closed) and bootstrap_addresses pointing at the
    artifact's genesis_multiaddr. Port seeds take the band slots after the
    5-node fleet. Rejects any artifact whose net_id is not exactly the
    staging id — an ephemeral reserved-band artifact is another net's, never
    joinable as the staging net."""
    if count < 1:
        raise ValueError("count must be >= 1")
    artifact_path = os.path.join(artifact_dir, "staging-net.json")
    with open(artifact_path) as handle:
        artifact = json.load(handle)
    net = artifact.get("net_id")
    if not isinstance(net, int):
        raise ValueError("artifact net_id must be an int (got %r)" % (net,))
    validate_network_id(net)  # the 04-01 staging escape (Pitfall 6)
    if net != STAGING_NET_ID:
        raise ValueError("artifact %s pins net_id %r — join mode consumes "
                         "exactly the staging net %d"
                         % (artifact_path, net, STAGING_NET_ID))
    multiaddr = artifact.get("genesis_multiaddr", "")
    if not multiaddr.startswith("/dns4/"):
        raise ValueError("artifact genesis_multiaddr must be the /dns4/ form "
                         "(got %r)" % (multiaddr[:40],))
    _expect_128_hex(artifact["authorized_full_node"],
                    "artifact authorized_full_node")
    _expect_128_hex(artifact["bootstrapper_node"],
                    "artifact bootstrapper_node")
    peers = list(artifact.get("trusted_peers") or [])
    if not peers:
        raise ValueError("artifact trusted_peers must be non-empty")
    for address in peers:
        _expect_128_hex(address, "artifact trusted peer")
    payload = _sgns_trust_payload(
        {"network_id": net,
         "bootstrapper": artifact["bootstrapper_node"],
         "peer_set": peers},
        artifact["authorized_full_node"])
    os.makedirs(out_dir, exist_ok=True)
    joiners = []
    for i in range(1, count + 1):
        base_dir = os.path.join(out_dir, "joiner%d" % i) + "/"
        os.makedirs(base_dir, exist_ok=True)
        _write_dev_config(base_dir)
        _write_log_config(base_dir)
        port_seed = NODE_PORT_BASE + (STAGING_FLEET_NODES + i - 1) * \
            NODE_PORT_STRIDE
        network_config_path = os.path.join(base_dir, "network_config.json")
        _write_json(network_config_path, {
            "auto_dht": True,
            "port_seed": port_seed,
            "bootstrap_addresses": [multiaddr],
        })
        _write_json(os.path.join(base_dir, "sgns_config.json"), payload)
        joiners.append({
            "name": "joiner%d" % i,
            "base_dir": base_dir,
            "network_config": network_config_path,
            "node_log": os.path.join(base_dir, "sgnslog2.log"),
            "port_seed": port_seed,
        })
    return joiners


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
        # Pitfall 2: every node log_config promotes the SAME logger set — the
        # Blockchain logger owns the joiner sync anchors, so it must be there.
        for entry in topology["nodes"]:
            log_cfg = _read_json(os.path.join(entry["base_dir"], "log_config.json"))
            assert log_cfg["loggers"] == dict(INFO_LOGGERS)
            assert BLOCKCHAIN_LOGGER_NAME in log_cfg["loggers"]
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

        # Phase C: joiner configs (2 joiners prove the band continues past the
        # last node). Minimal sgns_config (subnet_id == network_id, asserted
        # in-band at write time, plus authorized_full_node — the joiner must
        # know the authorized creator to ACCEPT the net's genesis), node1-only
        # bootstrap, same logger set, and joiner ports distinct from every
        # node/actor port.
        joiners = write_joiner_configs(topology, multiaddr, authorized, count=2)
        assert len(joiners) == 2
        try:
            write_joiner_configs({**topology, "network_id": 369}, multiaddr,
                                 authorized)
        except ValueError:
            pass
        else:
            raise AssertionError("out-of-band network_id must be rejected")
        try:
            write_joiner_configs(topology, multiaddr, "not-hex")
        except ValueError:
            pass
        else:
            raise AssertionError("non-128-hex authorized address must be "
                                 "rejected")
        joiner_seeds = []
        for index, joiner in enumerate(joiners, start=1):
            sgns = _read_json(os.path.join(joiner["base_dir"], "sgns_config.json"))
            # Same trust payload as every node (one home): a joiner differs
            # ONLY by booting without --key-file.
            assert sgns == _read_json(os.path.join(
                topology["nodes"][0]["base_dir"], "sgns_config.json"))
            net = _read_json(joiner["network_config"])
            assert net["port_seed"] == joiner["port_seed"]
            assert net["bootstrap_addresses"] == [multiaddr]
            log_cfg = _read_json(os.path.join(joiner["base_dir"], "log_config.json"))
            assert log_cfg["loggers"] == dict(INFO_LOGGERS)
            _read_json(os.path.join(joiner["base_dir"], "dev_config.json"))
            assert joiner["port_seed"] == (
                NODE_PORT_BASE
                + (len(topology["nodes"]) + index - 1) * NODE_PORT_STRIDE)
            joiner_seeds.append(joiner["port_seed"])
        all_seeds = sorted(node_seeds + joiner_seeds)
        assert all(b - a > 300 for a, b in zip(all_seeds, all_seeds[1:]))
        assert max(all_seeds) + 300 < min(actor_ports)  # clear of the actor band
        assert len(set(all_seeds + actor_ports)) == len(all_seeds + actor_ports)
        manifest = _read_json(os.path.join(run_dir, "topology.json"))
        assert [j["name"] for j in manifest["joiners"]] == \
            [joiner["name"] for joiner in joiners]
        assert manifest["joiners"][0]["node_log"] == joiners[0]["node_log"]

        # Phase D (04-01): the staging escape + the dht-health knobs.
        # validate_network_id accepts EXACTLY the reserved band or the
        # sanctioned staging id — no open range (T-04-01-02 reject path).
        validate_network_id(low)
        validate_network_id(high)
        validate_network_id(STAGING_NET_ID)
        for rejected in (0, 369, 963, 144, 65536, -1):
            try:
                validate_network_id(rejected)
            except ValueError:
                pass
            else:
                raise AssertionError("network_id %r must be rejected "
                                     "(reserved band or STAGING_NET_ID only)"
                                     % rejected)
        # The dns4 helper emits the /ipfs/-suffixed form the parser accepts —
        # never a bare address (Pitfall 5).
        assert make_dns4_multiaddr("localhost", 41037, "QmPeer") == \
            "/dns4/localhost/tcp/41037/ipfs/QmPeer"
        # An explicit staging build pins the manifest id and the sgns_config
        # net_id beside subnet_id; default draws add no net_id key (the
        # silent dev-net default is the Phase 2-3 behavior, unchanged).
        staging_dir = os.path.join(run_dir, "staging")
        os.makedirs(staging_dir)
        staging = build_topology(staging_dir, 2, network_id=STAGING_NET_ID)
        assert staging["network_id"] == STAGING_NET_ID
        # Staging actors carry the --net-id pin (topic appendix parity with
        # the staging nodes); reserved-band actors carry nothing (default
        # registry id both sides, exactly as Phase 2-3).
        assert all(actor["net_id"] == STAGING_NET_ID
                   for actor in staging["actors"])
        assert all("net_id" not in actor
                   for actor in topology["actors"])
        write_trust_configs(staging, authorized)
        staging_sgns = _read_json(os.path.join(
            staging["nodes"][0]["base_dir"], "sgns_config.json"))
        assert staging_sgns["net_id"] == STAGING_NET_ID
        assert staging_sgns["subnet_id"] == STAGING_NET_ID
        assert "net_id" not in _read_json(os.path.join(
            node1["base_dir"], "sgns_config.json"))
        # The staging escape opens the joiner writer for exactly the staging
        # id (Pitfall 6): 333 passes where 369 (out of band) still raises.
        write_joiner_configs(staging, multiaddr, authorized, count=1)
        # The split-bootstrap map: named nodes get their own entry, everyone
        # else keeps node1's (the dht-health two-wave boot shape).
        split_entry = make_dns4_multiaddr("localhost", 41037, "QmPeer")
        write_peer_configs(topology, multiaddr,
                           node_multiaddr_map={"node3": split_entry})
        for entry in topology["nodes"][1:]:
            net = _read_json(entry["network_config"])
            expected = split_entry if entry["name"] == "node3" else multiaddr
            assert net["bootstrap_addresses"] == [expected]
        # The co-located foreign-net node: own base dir, minimal sgns_config
        # pinning a different registry net, empty bootstrap, the next port
        # band slot, and the same promoted logger set (GossipPubSub rides
        # INFO_LOGGERS — one home, inherited here).
        foreign = write_foreign_net_configs(run_dir, 144)
        assert foreign["name"] == "foreign-net"
        fnet = _read_json(os.path.join(foreign["base_dir"],
                                       "network_config.json"))
        assert fnet["bootstrap_addresses"] == []
        assert fnet["auto_dht"] is True
        assert fnet["port_seed"] == NODE_PORT_BASE + 5 * NODE_PORT_STRIDE
        fsgns = _read_json(os.path.join(foreign["base_dir"],
                                        "sgns_config.json"))
        assert fsgns == {"node_type": "Full", "net_id": 144}
        flog = _read_json(os.path.join(foreign["base_dir"],
                                       "log_config.json"))
        assert flog["loggers"] == dict(INFO_LOGGERS)
        assert "GossipPubSub" in flog["loggers"]
        _read_json(os.path.join(foreign["base_dir"], "dev_config.json"))

        # The identity-boot net pin: ONLY a staging topology writes the
        # minimal sgns_config before node1's identity boot (the run's libp2p
        # keypair persists under the NET-SCOPED globaldb path — an identity
        # boot on the default net would hand the final boot a different path
        # and a fresh peer id, stranding every captured bootstrap address).
        # Reserved-band runs pin nothing (Phase 2-3 behavior, unchanged).
        identity_cfg = os.path.join(staging["nodes"][0]["base_dir"],
                                    "sgns_config.json")
        os.unlink(identity_cfg)  # write_trust_configs wrote the full payload
        write_identity_config(staging)
        assert _read_json(identity_cfg) == {"node_type": "Full",
                                            "net_id": STAGING_NET_ID}
        identity_cfg5 = os.path.join(node1["base_dir"], "sgns_config.json")
        before = _read_json(identity_cfg5)  # Phase A2 payload, already written
        write_identity_config(topology)
        assert _read_json(identity_cfg5) == before  # reserved band: untouched

        # Phase E (04-02): static-fleet keygen + bundle + trust push + join.
        # Keygen: 5 persistent identities 0600 in the offline dir, distinct
        # 128-hex addresses, fleet.json carrying key file PATHS only — zero
        # key hex in the manifest text (T-04-02-01).
        keys_dir = os.path.join(run_dir, "offline-keys")
        fleet_path = os.path.join(run_dir, "fleet.json")
        fleet = gen_fleet_keys(keys_dir, fleet_path)
        with open(fleet_path) as handle:
            fleet_text = handle.read()
        assert len(os.listdir(keys_dir)) == 5
        fleet_addresses = [node["ceremony_address"] for node in fleet["nodes"]]
        assert len(set(fleet_addresses)) == 5
        for node in fleet["nodes"]:
            assert (os.stat(node["key_file"]).st_mode & 0o777) == 0o600
            with open(node["key_file"]) as handle:
                assert handle.read().strip() not in fleet_text
            assert len(node["ceremony_address"]) == 128
            assert set(node["ceremony_address"]) <= _HEX
        assert fleet["network_id"] == STAGING_NET_ID
        assert fleet["bootstrapper"] == fleet_addresses[0]
        assert fleet["peer_set"] == fleet_addresses[1:]

        # The operator fills the genesis published multiaddr once the genesis
        # identity boot has captured its peer id (D-04: the DNS-named entry);
        # the bundle writer consumes exactly that fact.
        fleet["nodes"][0]["published_multiaddr"] = make_dns4_multiaddr(
            "staging.example.invalid", 41001, "QmGenesisPeer")

        # Bundle writer: one directory per node; genesis carries the minimal
        # identity-boot sgns_config (the write_identity_config shape — one
        # home), peers bootstrap to the genesis /dns4/ multiaddr; every port
        # seed unique, spaced >300, and >300 clear of the co-located AWS
        # node's port.
        bundle_dir = os.path.join(run_dir, "fleet-bundles")
        fleet = write_fleet_configs(fleet, bundle_dir)
        fleet_seeds = []
        for node in fleet["nodes"]:
            node_dir = node["bundle_dir"]
            assert os.path.isdir(node_dir)
            _read_json(os.path.join(node_dir, "dev_config.json"))
            log_cfg = _read_json(os.path.join(node_dir, "log_config.json"))
            assert log_cfg["loggers"] == dict(INFO_LOGGERS)
            net = _read_json(os.path.join(node_dir, "network_config.json"))
            assert net["auto_dht"] is True
            assert net["port_seed"] == node["port_seed"]
            fleet_seeds.append(node["port_seed"])
            if node["role"] == "genesis":
                assert net["bootstrap_addresses"] == []
                assert _read_json(os.path.join(
                    node_dir, "sgns_config.json")) == {
                    "node_type": "Full", "net_id": STAGING_NET_ID}
            else:
                assert net["bootstrap_addresses"] == \
                    [fleet["nodes"][0]["published_multiaddr"]]
                assert not os.path.exists(os.path.join(node_dir,
                                                      "sgns_config.json"))
        sorted_seeds = sorted(fleet_seeds)
        assert all(b - a > 300 for a, b in zip(sorted_seeds, sorted_seeds[1:]))
        assert all(abs(seed - fleet["existing_node_port"]) > 300
                   for seed in fleet_seeds)

        # Post-B_1 trust push: the SAME _sgns_trust_payload (net_id pinned
        # beside subnet_id) into every node's bundle dir — 04-03's extension
        # point; without a bundle it fails loudly.
        fresh_keygen = gen_fleet_keys(os.path.join(run_dir, "offline-keys2"),
                                      os.path.join(run_dir, "fleet2.json"))
        try:
            write_fleet_trust_configs(fresh_keygen, authorized)
        except ValueError:
            pass
        else:
            raise AssertionError("trust push before write_fleet_configs must "
                                 "be rejected")
        write_fleet_trust_configs(fleet, authorized)
        for node in fleet["nodes"]:
            sgns = _read_json(os.path.join(node["bundle_dir"],
                                           "sgns_config.json"))
            assert sgns["node_type"] == "Full"
            assert sgns["net_id"] == STAGING_NET_ID
            assert sgns["subnet_id"] == STAGING_NET_ID
            assert sgns["authorized_full_node"] == authorized
            assert sgns["bootstrapper_node"] == fleet["bootstrapper"]
            assert sgns["trusted_peers"] == fleet["peer_set"]

        # Bundle-writer reject paths (each its own ValueError): seeds within
        # 300 of each other, a seed within 300 of the co-located AWS node, a
        # non-dns4 genesis multiaddr, an unfilled PLACEHOLDER multiaddr, and a
        # fleet that is not the staging net.
        def _clone_fleet():
            return json.loads(json.dumps(fleet))

        reject_dir = os.path.join(run_dir, "reject")
        bad_cases = []
        bad = _clone_fleet()
        bad["nodes"][1]["port_seed"] = bad["nodes"][0]["port_seed"] + 100
        bad_cases.append((bad, "seeds within 300 of each other"))
        bad = _clone_fleet()
        bad["nodes"][0]["port_seed"] = fleet["existing_node_port"] + 100
        bad_cases.append((bad, "seed within 300 of existing_node_port"))
        bad = _clone_fleet()
        bad["nodes"][0]["published_multiaddr"] = \
            "/ip4/127.0.0.1/tcp/41001/ipfs/QmX"
        bad_cases.append((bad, "non-dns4 published_multiaddr"))
        bad = _clone_fleet()
        bad["nodes"][0]["published_multiaddr"] = make_dns4_multiaddr(
            "host.invalid", 41001, "PLACEHOLDER-PEER-ID")
        bad_cases.append((bad, "unfilled PLACEHOLDER multiaddr"))
        bad = _clone_fleet()
        bad["network_id"] = 62000
        bad_cases.append((bad, "fleet net_id != STAGING_NET_ID"))
        for mutate, why in bad_cases:
            try:
                write_fleet_configs(mutate, reject_dir)
            except ValueError:
                pass
            else:
                raise AssertionError("%s must be rejected" % why)

        # Join mode: a fixture artifact set; joiner sgns_config is the FULL
        # trust payload from the artifact (net_id 333 — 03-02 A3), no key file
        # anywhere, bootstrap == the artifact's genesis_multiaddr; an artifact
        # whose net_id is anything but the staging id is rejected loudly.
        artifact_dir = os.path.join(run_dir, "staging-artifact")
        os.makedirs(artifact_dir)
        artifact = {
            "net_id": STAGING_NET_ID,
            "genesis_multiaddr": make_dns4_multiaddr(
                "staging.example.invalid", 41001, "QmGenesisPeer"),
            "authorized_full_node": "aa" * 64,
            "bootstrapper_node": "bb" * 64,
            "trusted_peers": ["cc" * 64, "dd" * 64],
        }
        artifact_path = os.path.join(artifact_dir, "staging-net.json")
        _write_json(artifact_path, artifact)
        joiner_out = os.path.join(run_dir, "joiner-configs")
        joiners = write_artifact_joiners(artifact_dir, joiner_out, count=2)
        assert len(joiners) == 2
        for index, joiner in enumerate(joiners, start=1):
            sgns = _read_json(os.path.join(joiner["base_dir"],
                                           "sgns_config.json"))
            assert sgns["net_id"] == STAGING_NET_ID
            assert sgns["subnet_id"] == STAGING_NET_ID
            assert sgns["authorized_full_node"] == artifact["authorized_full_node"]
            assert sgns["bootstrapper_node"] == artifact["bootstrapper_node"]
            assert sgns["trusted_peers"] == artifact["trusted_peers"]
            assert "key_file" not in sgns
            net = _read_json(joiner["network_config"])
            assert net["bootstrap_addresses"] == [artifact["genesis_multiaddr"]]
            assert net["port_seed"] == joiner["port_seed"]
            assert joiner["port_seed"] == (
                NODE_PORT_BASE + (STAGING_FLEET_NODES + index - 1)
                * NODE_PORT_STRIDE)
            assert not any(name.endswith(".key")
                           for name in os.listdir(joiner["base_dir"]))
            assert _read_json(os.path.join(joiner["base_dir"],
                                           "log_config.json"))["loggers"] == \
                dict(INFO_LOGGERS)
        _write_json(artifact_path, dict(artifact, net_id=62000))
        try:
            write_artifact_joiners(artifact_dir, joiner_out)
        except ValueError:
            pass
        else:
            raise AssertionError("artifact net_id != STAGING_NET_ID must be "
                                 "rejected")
        _write_json(artifact_path, dict(artifact, authorized_full_node="zz"))
        try:
            write_artifact_joiners(artifact_dir, joiner_out)
        except ValueError:
            pass
        else:
            raise AssertionError("non-128-hex artifact address must be "
                                 "rejected")
    finally:
        shutil.rmtree(run_dir)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Topology/config generation: selftest, the static-fleet "
                    "trio (keygen -> bundles -> post-B_1 trust push), and "
                    "artifact-driven join mode.")
    parser.add_argument("--selftest", action="store_true",
                        help="build a 5-node topology and assert invariants")
    parser.add_argument("--gen-fleet-keys", action="store_true",
                        help="generate the 5 persistent staging identities "
                             "OFFLINE (0600) + the fleet.json manifest (key "
                             "file PATHS only)")
    parser.add_argument("--keys-dir",
                        help="offline directory for the persistent fleet keys")
    parser.add_argument("--out-fleet",
                        help="path of the fleet.json manifest to write")
    parser.add_argument("--static-fleet", metavar="FLEET_JSON",
                        help="write per-node config bundles from a completed "
                             "fleet.json into --out-dir")
    parser.add_argument("--join", metavar="ARTIFACT_DIR",
                        help="write joiner configs from a published "
                             "staging-net artifact set into --out-dir")
    parser.add_argument("--out-dir",
                        help="output directory for --static-fleet/--join")
    parser.add_argument("--count", type=int, default=1,
                        help="joiner count for --join (default 1)")
    args = parser.parse_args()
    if args.selftest:
        _selftest()
        print("topology selftest PASS")
        return
    if args.gen_fleet_keys:
        if not args.keys_dir or not args.out_fleet:
            parser.error("--gen-fleet-keys requires --keys-dir and "
                         "--out-fleet")
        fleet = gen_fleet_keys(args.keys_dir, args.out_fleet)
        print("wrote %d persistent keys to %s (0600; held offline)"
              % (len(fleet["nodes"]), args.keys_dir))
        print("wrote fleet manifest %s (key PATHS only — zero key hex)"
              % args.out_fleet)
        print("NEXT: fill the genesis published_multiaddr (real DNS name + "
              "peer id from its identity boot) and the transport hosts, then "
              "run --static-fleet %s --out-dir <bundle-dir>"
              % args.out_fleet)
        return
    if args.static_fleet:
        if not args.out_dir:
            parser.error("--static-fleet requires --out-dir")
        with open(args.static_fleet) as handle:
            fleet = json.load(handle)
        fleet = write_fleet_configs(fleet, args.out_dir)
        for node in fleet["nodes"]:
            print("wrote bundle %s (role %s, port_seed %d)"
                  % (node["bundle_dir"], node["role"], node["port_seed"]))
        print("genesis sgns_config is the minimal identity shape; run "
              "write_fleet_trust_configs (04-03) after B_1 is read")
        return
    if args.join:
        if not args.out_dir:
            parser.error("--join requires --out-dir")
        joiners = write_artifact_joiners(args.join, args.out_dir,
                                         count=args.count)
        for joiner in joiners:
            print("wrote joiner bundle %s (boot WITHOUT --key-file; port_seed"
                  " %d)" % (joiner["base_dir"], joiner["port_seed"]))
        return
    parser.error("nothing to do; use --selftest")


if __name__ == "__main__":
    main()
