#!/usr/bin/env python3
"""
staging_artifact.py — the published staging-net artifact-set generator (NET-02,
D-10/D-11).

write_staging_artifact(out_dir, facts) assembles the canonical published set
from OBSERVED values only — ceremony_fingerprint from make-manifest stdout,
genesis_cid from the STATUS head= line, addresses read off the live fleet —
never recomputed (the harness consumes; a recomputation bug would publish an
unverifiable net). Validation reuses topology's one-home validators
(_expect_128_hex, validate_network_id, STAGING_NET_ID) plus the fingerprint
shape from ceremony; manifest.bin is copied BYTE-IDENTICAL and its sha256 must
equal the recorded ceremony_fingerprint (Phase 1's determinism guarantee
consumed at publication time).

Output shape (deploy/staging-net/, fetched by raw URL, consumed by topology
join mode):
  staging-net.json — net_id, genesis_multiaddr (/dns4/ DNS-named form, D-04;
    /ip4/ accepted under the operator's documented 04-04 raw-IP waiver —
    never a silent weakening: both forms name the one public entry),
    node_version, supergenius_tarball_sha256, runner_sha256,
    ceremony_fingerprint, genesis_cid, authorized_full_node,
    bootstrapper_node, trusted_peers
  manifest.bin     — the ceremony manifest bytes, byte-identical

Usage:
  python3 staging_artifact.py --selftest
"""

import argparse
import hashlib
import json
import os
import secrets
import shutil
import tempfile

import topology


def _expect_64_hex(value, what):
    # The sha256-string shape (fingerprint, tarball/runner digests): exactly
    # 64 lowercase hex, checked through topology's one hex charset home.
    if not (isinstance(value, str) and len(value) == 64
            and set(value) <= topology._HEX):
        raise ValueError("%s must be 64 lowercase hex (got %r)"
                         % (what, value))


def write_staging_artifact(out_dir, facts):
    """Validate observed facts and assemble the published artifact set.

    facts (all OBSERVED values — callers never synthesize any of them):
      net_id, genesis_multiaddr, node_version, supergenius_tarball_sha256,
      runner_sha256, ceremony_fingerprint, genesis_cid,
      authorized_full_node, bootstrapper_node, trusted_peers,
      manifest_path (the observed ceremony manifest.bin to copy).

    Every fact is validated BEFORE anything is written (a reject leaves no
    partial artifact set behind), manifest.bin is copied byte-identical and
    never regenerated, and sha256(manifest.bin) must equal the recorded
    ceremony_fingerprint — the make-manifest contract consumed at publication
    time (a recomputed or mismatched fingerprint would publish an
    unverifiable net). Returns the written staging-net.json payload.
    """
    net = facts.get("net_id")
    if not isinstance(net, int):
        raise ValueError("net_id must be an int (got %r)" % (net,))
    topology.validate_network_id(net)
    if net != topology.STAGING_NET_ID:
        raise ValueError("the published artifact set is THE staging net: "
                         "net_id must be %d (got %r)"
                         % (topology.STAGING_NET_ID, net))
    multiaddr = facts.get("genesis_multiaddr", "")
    if not (isinstance(multiaddr, str)
            and multiaddr.startswith(("/dns4/", "/ip4/"))):
        raise ValueError("genesis_multiaddr must be the /dns4/ DNS-named form "
                         "(or /ip4/ under the operator's documented 04-04 "
                         "raw-IP waiver) (got %r)" % (multiaddr[:40],))
    for field in ("node_version", "genesis_cid"):
        if not facts.get(field):
            raise ValueError("%s must be a non-empty observed string" % field)
    _expect_64_hex(facts.get("supergenius_tarball_sha256"),
                   "supergenius_tarball_sha256")
    _expect_64_hex(facts.get("runner_sha256"), "runner_sha256")
    fingerprint = facts.get("ceremony_fingerprint")
    _expect_64_hex(fingerprint, "ceremony_fingerprint")
    topology._expect_128_hex(facts["authorized_full_node"],
                             "authorized_full_node")
    topology._expect_128_hex(facts["bootstrapper_node"],
                             "bootstrapper_node")
    peers = facts.get("trusted_peers") or []
    if not peers:
        raise ValueError("trusted_peers must be non-empty")
    for address in peers:
        topology._expect_128_hex(address, "trusted peer")
    manifest_path = facts.get("manifest_path")
    if not manifest_path or not os.path.isfile(manifest_path):
        raise ValueError("manifest_path must point at the observed "
                         "manifest.bin (got %r)" % (manifest_path,))
    with open(manifest_path, "rb") as handle:
        manifest_bytes = handle.read()
    if not manifest_bytes:
        raise ValueError("observed manifest.bin is empty")
    digest = hashlib.sha256(manifest_bytes).hexdigest()
    if digest != fingerprint:
        raise ValueError("ceremony_fingerprint %s != sha256(manifest.bin) %s "
                         "— the artifact must publish observed, matching "
                         "facts" % (fingerprint, digest))

    # All facts validated — only now touch the output directory.
    os.makedirs(out_dir, exist_ok=True)
    published_manifest = os.path.join(out_dir, "manifest.bin")
    shutil.copyfile(manifest_path, published_manifest)
    with open(published_manifest, "rb") as handle:
        if handle.read() != manifest_bytes:
            raise ValueError("manifest copy is not byte-identical — refusing "
                             "to publish")
    artifact = {
        "net_id": net,
        "genesis_multiaddr": multiaddr,
        "node_version": facts["node_version"],
        "supergenius_tarball_sha256": facts["supergenius_tarball_sha256"],
        "runner_sha256": facts["runner_sha256"],
        "ceremony_fingerprint": fingerprint,
        "genesis_cid": facts["genesis_cid"],
        "authorized_full_node": facts["authorized_full_node"],
        "bootstrapper_node": facts["bootstrapper_node"],
        "trusted_peers": list(peers),
    }
    with open(os.path.join(out_dir, "staging-net.json"), "w") as handle:
        json.dump(artifact, handle, indent=2)
        handle.write("\n")
    return artifact


def _selftest() -> None:
    run_dir = tempfile.mkdtemp(prefix="staging-artifact-selftest-")
    try:
        # Fixture observed facts: random manifest bytes whose sha256 IS the
        # fingerprint (the make-manifest contract), valid 128-hex addresses,
        # the dns4 genesis multiaddr form.
        manifest_path = os.path.join(run_dir, "observed-manifest.bin")
        manifest_bytes = secrets.token_bytes(96)
        with open(manifest_path, "wb") as handle:
            handle.write(manifest_bytes)
        facts = {
            "net_id": topology.STAGING_NET_ID,
            "genesis_multiaddr": topology.make_dns4_multiaddr(
                "staging.example.invalid", 41001, "QmGenesisPeer"),
            "node_version": "SuperGenius test-version string",
            "supergenius_tarball_sha256": "ab" * 32,
            "runner_sha256": "cd" * 32,
            "ceremony_fingerprint": hashlib.sha256(manifest_bytes).hexdigest(),
            "genesis_cid": "QmTestGenesisCid",
            "authorized_full_node": "aa" * 64,
            "bootstrapper_node": "bb" * 64,
            "trusted_peers": ["cc" * 64, "dd" * 64, "ee" * 64, "ff" * 64],
            "manifest_path": manifest_path,
        }
        publish_dir = os.path.join(run_dir, "publish")
        os.makedirs(publish_dir)
        write_staging_artifact(publish_dir, facts)

        # Every field round-trips into staging-net.json.
        with open(os.path.join(publish_dir, "staging-net.json")) as handle:
            artifact = json.load(handle)
        expected = {key: facts[key] for key in (
            "net_id", "genesis_multiaddr", "node_version",
            "supergenius_tarball_sha256", "runner_sha256",
            "ceremony_fingerprint", "genesis_cid", "authorized_full_node",
            "bootstrapper_node", "trusted_peers")}
        assert artifact == expected

        # manifest.bin is byte-identical (never regenerated) and its sha256
        # equals the published fingerprint.
        with open(os.path.join(publish_dir, "manifest.bin"), "rb") as handle:
            copied = handle.read()
        assert copied == manifest_bytes
        assert hashlib.sha256(copied).hexdigest() == \
            artifact["ceremony_fingerprint"]

        # The published set is exactly the two files.
        assert sorted(os.listdir(publish_dir)) == \
            ["manifest.bin", "staging-net.json"]

        # The 04-04 raw-IP waiver: the /ip4/ entry form round-trips too.
        waiver_dir = os.path.join(run_dir, "publish-ip4")
        os.makedirs(waiver_dir)
        waiver_facts = dict(facts)
        waiver_facts["genesis_multiaddr"] = topology.make_ip4_multiaddr(
            "208.113.209.106", 41273, "12D3KooWMvXKGwt2mDcgusWrjALbLMwfYCgTvWJxMSmwLi633txx")
        write_staging_artifact(waiver_dir, waiver_facts)
        with open(os.path.join(waiver_dir, "staging-net.json")) as handle:
            assert json.load(handle)["genesis_multiaddr"] == \
                waiver_facts["genesis_multiaddr"]

        # Reject paths: each a loud ValueError before anything is written.
        rejects = [
            ("net_id", 62000, "non-staging net_id"),
            ("net_id", 369, "registry-but-not-staging net_id"),
            ("genesis_multiaddr",
             "/dns6/::1/tcp/41001/ipfs/QmX", "non-dns4/ip4 multiaddr"),
            ("authorized_full_node", "zz", "non-128-hex address"),
            ("bootstrapper_node", "0x1234", "non-128-hex bootstrapper"),
            ("trusted_peers", ["cc" * 64, "zz"], "non-128-hex trusted peer"),
            ("ceremony_fingerprint", "00" * 32,
             "fingerprint != sha256(manifest)"),
            ("runner_sha256", "not-a-sha", "malformed runner sha256"),
            ("supergenius_tarball_sha256", "also-not", "malformed tarball sha"),
            ("node_version", "", "empty node_version"),
            ("genesis_cid", "", "empty genesis_cid"),
        ]
        for key, value, why in rejects:
            bad = dict(facts)
            bad[key] = value
            bad_dir = os.path.join(run_dir, "reject")
            try:
                write_staging_artifact(bad_dir, bad)
            except ValueError:
                pass
            else:
                raise AssertionError("%s must be rejected" % why)
            assert not os.path.exists(bad_dir)
    finally:
        shutil.rmtree(run_dir)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Selftest for the staging-net artifact-set generator "
                    "(fixture observed facts; asserts shape validation, "
                    "byte-identical manifest copy, reject paths).")
    parser.add_argument("--selftest", action="store_true",
                        help="run the fixture checks; exit 0 on pass")
    args = parser.parse_args()
    if args.selftest:
        _selftest()
        print("staging_artifact selftest PASS")
        return
    parser.error("nothing to do; use --selftest")


if __name__ == "__main__":
    main()
