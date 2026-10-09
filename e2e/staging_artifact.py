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
  staging-net.json — net_id, genesis_multiaddr (/dns4/ form, D-04),
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


def write_staging_artifact(out_dir, facts):
    """Validate observed facts and assemble the published artifact set.

    facts (all OBSERVED values — callers never synthesize any of them):
      net_id, genesis_multiaddr, node_version, supergenius_tarball_sha256,
      runner_sha256, ceremony_fingerprint, genesis_cid,
      authorized_full_node, bootstrapper_node, trusted_peers,
      manifest_path (the observed ceremony manifest.bin to copy).
    """
    raise NotImplementedError("04-02 RED scaffold")


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

        # Reject paths: each a loud ValueError before anything is written.
        rejects = [
            ("net_id", 62000, "non-staging net_id"),
            ("net_id", 369, "registry-but-not-staging net_id"),
            ("genesis_multiaddr",
             "/ip4/127.0.0.1/tcp/41001/ipfs/QmX", "non-dns4 multiaddr"),
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
