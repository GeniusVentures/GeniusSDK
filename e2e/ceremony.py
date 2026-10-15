#!/usr/bin/env python3
"""
ceremony.py — sgns-trust step drivers for the ceremony e2e harness (Variant B).

Each driver invokes the sgns-trust CLI with an argv list only (never through a
shell) and asserts on BOTH the exit code AND the pinned stdout strings from
genesis_tool/main.cpp + GenesisCeremony.cpp (02-RESEARCH Q4) — `list` exits 0
even when its output is empty, so its result is content-asserted here and by
the caller, never by exit code alone. Actor stdout/stderr are captured to
<run_dir>/<name>.out/.err as the process runs (D-17 evidence materialized
live). Every candidate id and the fingerprint are re-validated against their
strict patterns before any reuse in an argv (T-02-12/T-02-13).

make_manifest additionally consumes Phase 1's determinism guarantee at the
multi-process layer: a second invocation with identical inputs must produce
byte-identical manifest bytes, and sha256(manifest.bin) must equal the
fingerprint the tool printed (stdlib hashlib — the harness never re-implements
the C++ canonical encoder).

Usage: imported by ceremony_e2e.py; `python3 ceremony.py --selftest` checks
the stdout parsers against the pinned line shapes.
"""

import argparse
import hashlib
import os
import re
import subprocess

import topology  # TOPIC has exactly one home (Pitfall 6 lives there)

FINGERPRINT_RE = re.compile(r"^fingerprint: ([0-9a-f]{64})$", re.MULTILINE)
THRESHOLD_RE = re.compile(r"^(membership|burn) threshold: (\d+)$", re.MULTILINE)
CANDIDATE_LINE_RE = re.compile(r"^(policy|burn) ([a-z0-9-]+:[0-9]+:[0-9a-f]{64})$",
                               re.MULTILINE)
CANDIDATE_ID_RE = re.compile(r"^[a-z0-9-]+:[0-9]+:[0-9a-f]{64}$")
FINGERPRINT_ID_RE = re.compile(r"^[0-9a-f]{64}$")

# The harness ALWAYS passes both explicitly (bounded wall clock; the tool
# defaults of 600/30 are far beyond a local run's budget).
SERVE_SECONDS = 90
TIMEOUT_SECONDS = 60
# Python-side hard bound per actor process: serve window + catch-up deadline
# + startup margin. A hung actor is SIGKILLed rather than stalling the run.
ACTOR_HARD_TIMEOUT = SERVE_SECONDS + TIMEOUT_SECONDS + 90


class CeremonyError(RuntimeError):
    """A ceremony step failed: step, actor, exit code, captured output."""


def _read_text(path):
    with open(path, encoding="utf-8", errors="replace") as handle:
        return handle.read()


def _tail(text, lines=15):
    captured = text.splitlines()
    return "\n".join(captured[-lines:]) if captured else "(no output)"


def _expect(condition, message):
    if not condition:
        raise CeremonyError(message)


def _run_actor(name, argv, run_dir, stdin_text=None):
    """Run one sgns-trust actor to completion, sinks teed to the run dir.

    stdin_text (the genesis fingerprint echo) is written IMMEDIATELY and stdin
    is then closed — std::getline blocks until data, so waiting to read the
    "Type the exact fingerprint to submit:" prompt first is unnecessary and
    racy (Pitfall 1). Returns (exit_code, stdout_text, stderr_text).
    """
    out_path = os.path.join(run_dir, name + ".out")
    err_path = os.path.join(run_dir, name + ".err")
    with open(out_path, "wb") as out_handle, open(err_path, "wb") as err_handle:
        proc = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=out_handle,
                                stderr=err_handle, start_new_session=True)
        try:
            proc.communicate(input=(stdin_text or "").encode("ascii"),
                             timeout=ACTOR_HARD_TIMEOUT)
        except BaseException as error:
            # WR-02: the actor runs start_new_session — an interrupted harness
            # (its own SIGTERM handler, Ctrl-C, a timeout) must kill and reap
            # the child here or it stays alive holding its RocksDB lock past
            # teardown. The typed CeremonyError is raised only for the timeout
            # case; every other interruption re-raises unchanged.
            proc.kill()
            proc.communicate()
            if isinstance(error, subprocess.TimeoutExpired):
                raise CeremonyError("%s exceeded the harness budget of %ss "
                                    "(serve %ss + timeout %ss + margin): %s"
                                    % (name, ACTOR_HARD_TIMEOUT, SERVE_SECONDS,
                                       TIMEOUT_SECONDS, argv)) from error
            raise
    return proc.returncode, _read_text(out_path), _read_text(err_path)


def _shared_argv(sgns_trust, operation, actor, manifest):
    """The four options every local operation requires. --database is always
    the actor's own run-dir tree, never a live node base dir (RocksDB
    single-writer, Pitfall 2); --topic is always the node trust topic
    (Pitfall 6 — a mismatched topic strands the ceremony silently). A
    staging actor additionally pins --net-id (every pubsub topic carries the
    net appendix — without the pin the actor joins .3.7.144 topics while the
    staging nodes are on .3.7.333 and the CRDT catch-up starves; the key is
    set by topology only for a staging-pin run)."""
    argv = [sgns_trust, operation,
            "--manifest", manifest,
            "--network-config", actor["network_config"],
            "--database", actor["database"],
            "--topic", topology.TOPIC]
    if "net_id" in actor:
        argv += ["--net-id", str(actor["net_id"])]
    return argv


def make_manifest(sgns_trust, run_dir, network_id, bootstrapper, peers):
    """Build the canonical manifest, verify it two ways, parse its facts.

    (1) sha256(manifest.bin) must equal the fingerprint the tool printed —
    the harness's artifact assertion consuming Phase 1's determinism work.
    (2) A second invocation with identical inputs must produce byte-identical
    bytes (multi-process determinism check). Returns the parsed facts.
    """
    manifest_path = os.path.join(run_dir, "manifest.bin")
    manifest2_path = os.path.join(run_dir, "manifest2.bin")
    argv = [sgns_trust, "make-manifest",
            "--network-id", str(network_id),
            "--bootstrapper", bootstrapper,
            "--peers", ",".join(peers),
            "--out", manifest_path]
    result = subprocess.run(argv, capture_output=True, text=True)
    _expect(result.returncode == 0,
            "make-manifest exited %s\nstderr:\n%s"
            % (result.returncode, result.stderr.strip()))
    fingerprint = FINGERPRINT_RE.search(result.stdout)
    _expect(fingerprint is not None,
            "make-manifest stdout has no fingerprint line:\n%s"
            % _tail(result.stdout))
    fingerprint = fingerprint.group(1)
    thresholds = dict(THRESHOLD_RE.findall(result.stdout))
    _expect("burn" in thresholds and "membership" in thresholds,
            "make-manifest stdout lacks threshold lines:\n%s"
            % _tail(result.stdout))

    second = subprocess.run(argv[:-1] + [manifest2_path], capture_output=True,
                            text=True)
    _expect(second.returncode == 0,
            "second make-manifest exited %s\nstderr:\n%s"
            % (second.returncode, second.stderr.strip()))
    with open(manifest_path, "rb") as handle:
        first_bytes = handle.read()
    with open(manifest2_path, "rb") as handle:
        second_bytes = handle.read()
    _expect(first_bytes and first_bytes == second_bytes,
            "make-manifest is not deterministic: manifest.bin and "
            "manifest2.bin differ (%d vs %d bytes)"
            % (len(first_bytes), len(second_bytes)))
    digest = hashlib.sha256(first_bytes).hexdigest()
    _expect(digest == fingerprint,
            "sha256(manifest.bin) %s != printed fingerprint %s"
            % (digest, fingerprint))
    return {
        "fingerprint": fingerprint,
        "membership_threshold": int(thresholds["membership"]),
        "burn_threshold": int(thresholds["burn"]),
        "manifest": manifest_path,
        "stdout": result.stdout,
    }


def genesis(sgns_trust, actor, manifest, fingerprint, run_dir,
            serve_seconds=SERVE_SECONDS):
    """Bootstrapper-only genesis (peers get BOOTSTRAPPER_MISMATCH by design —
    Phase 1 pinned it). Asserts exit 0, both pinned success strings, and that
    the key file was unlinked on success (Pitfall 9)."""
    _expect(FINGERPRINT_ID_RE.match(fingerprint) is not None,
            "fingerprint %r is not 64 lowercase hex" % fingerprint)
    argv = _shared_argv(sgns_trust, "genesis", actor, manifest) + [
        "--key-file", actor["key_file"],
        "--serve-seconds", str(serve_seconds),
        "--timeout-seconds", str(TIMEOUT_SECONDS)]
    code, out, err = _run_actor(actor["name"], argv, run_dir,
                                stdin_text=fingerprint + "\n")
    _expect(code == 0,
            "%s genesis exited %s\nstdout tail:\n%s\nstderr:\n%s"
            % (actor["name"], code, _tail(out), err.strip()))
    _expect("Genesis durably confirmed." in out,
            "%s genesis stdout lacks 'Genesis durably confirmed.':\n%s"
            % (actor["name"], _tail(out)))
    _expect("Serving genesis to peers for" in out,
            "%s genesis stdout lacks 'Serving genesis to peers for':\n%s"
            % (actor["name"], _tail(out)))
    _expect(not os.path.exists(actor["key_file"]),
            "%s genesis succeeded but its key file was not unlinked "
            "(Pitfall 9): %s" % (actor["name"], actor["key_file"]))
    return out


def list_candidates(sgns_trust, actor, manifest, run_dir):
    """List current-head candidates as (type, id) pairs, e.g.
    ("burn", "burn-config:1:<hash>"). The tool exits 0 even when the list is
    empty, so an empty parse after the tool's own catch-up window is a loud
    failure naming the actor and its last output — never a silent pass."""
    argv = _shared_argv(sgns_trust, "list", actor, manifest) + [
        "--timeout-seconds", str(TIMEOUT_SECONDS)]
    code, out, err = _run_actor(actor["name"], argv, run_dir)
    _expect(code == 0,
            "%s list exited %s\nstdout tail:\n%s\nstderr:\n%s"
            % (actor["name"], code, _tail(out), err.strip()))
    candidates = [(found.group(1), found.group(2))
                  for found in CANDIDATE_LINE_RE.finditer(out)]
    _expect(candidates,
            "%s list saw no candidates after the %ss catch-up window "
            "(content-gated — exit code alone never proves visibility):\n%s"
            % (actor["name"], TIMEOUT_SECONDS, _tail(out)))
    return candidates


def approve(sgns_trust, actor, manifest, candidate_id, run_dir,
            serve_seconds=SERVE_SECONDS):
    """Approve one exact candidate id. The id is re-validated against its
    strict pattern before it is placed in argv (it crossed a child-stdout to
    process-argument boundary — T-02-12)."""
    _expect(CANDIDATE_ID_RE.match(candidate_id) is not None,
            "candidate id %r fails ^[a-z0-9-]+:[0-9]+:[0-9a-f]{64}$"
            % candidate_id)
    argv = _shared_argv(sgns_trust, "approve", actor, manifest) + [
        "--key-file", actor["key_file"],
        "--candidate-id", candidate_id,
        "--serve-seconds", str(serve_seconds),
        "--timeout-seconds", str(TIMEOUT_SECONDS)]
    code, out, err = _run_actor(actor["name"], argv, run_dir)
    _expect(code == 0,
            "%s approve of %s exited %s\nstdout tail:\n%s\nstderr:\n%s"
            % (actor["name"], candidate_id, code, _tail(out), err.strip()))
    _expect(candidate_id in out,
            "%s approve stdout does not echo candidate %s:\n%s"
            % (actor["name"], candidate_id, _tail(out)))
    _expect("Serving updated trust state to peers for" in out,
            "%s approve stdout lacks 'Serving updated trust state to peers "
            "for':\n%s" % (actor["name"], _tail(out)))
    return out


def _selftest():
    pinned_hash = "a" * 64
    make_out = ("manifest written to /run/manifest.bin\n"
                "network: 62000\n"
                "bootstrapper: " + "ab" * 64 + "\n"
                "policy version: 1\n"
                "membership threshold: 3\n"
                "burn threshold: 3\n"
                "initial burn basis points: 100\n"
                "ordered peers:\n"
                "  " + "cd" * 64 + "\n"
                "  " + "ef" * 64 + "\n"
                "fingerprint: " + pinned_hash + "\n")
    found = FINGERPRINT_RE.search(make_out)
    assert found and found.group(1) == pinned_hash
    thresholds = dict(THRESHOLD_RE.findall(make_out))
    assert thresholds == {"membership": "3", "burn": "3"}

    list_out = ("No candidates visible yet - waiting for CRDT catch-up...\n"
                "policy trusted-peer:1:" + pinned_hash + "\n"
                "burn burn-config:1:" + pinned_hash + "\n")
    candidates = [(m.group(1), m.group(2))
                  for m in CANDIDATE_LINE_RE.finditer(list_out)]
    assert candidates == [("policy", "trusted-peer:1:" + pinned_hash),
                          ("burn", "burn-config:1:" + pinned_hash)]
    # Waiting chatter must not parse as a candidate line.
    assert not CANDIDATE_LINE_RE.findall("No candidates visible yet - waiting"
                                         " for CRDT catch-up...\n")

    assert CANDIDATE_ID_RE.match("burn-config:1:" + pinned_hash)
    assert not CANDIDATE_ID_RE.match("Burn-Config:1:" + pinned_hash)
    assert not CANDIDATE_ID_RE.match("burn-config:x:" + pinned_hash)
    assert not CANDIDATE_ID_RE.match("burn-config:1:" + pinned_hash.upper())
    assert FINGERPRINT_ID_RE.match(pinned_hash)
    assert not FINGERPRINT_ID_RE.match(pinned_hash.upper())


def main():
    parser = argparse.ArgumentParser(
        description="Selftest for the sgns-trust stdout parsers (pinned line "
                    "shapes from genesis_tool main.cpp).")
    parser.add_argument("--selftest", action="store_true",
                        help="check the parsers against pinned shapes")
    args = parser.parse_args()
    if args.selftest:
        _selftest()
        print("ceremony selftest PASS")
        return
    parser.error("nothing to do; use --selftest")


if __name__ == "__main__":
    main()
