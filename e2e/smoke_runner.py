#!/usr/bin/env python3
"""
smoke_runner.py — one-node smoke check for the extended GeniusSDKService runner.

Proves the plan 02-01 walking skeleton end-to-end: boot one real node from a
policy-checked 0600 key file via --key-file, observe at least one
"STATUS node_state=" line on stdout (the D-11 observability surface), SIGTERM the
process, and assert exit code 0 with a shutdown line (the D-03 sigwait path).
The negative pass proves the D-02 key-file policy: a 0644 key file must be
rejected with a nonzero exit and a key-file message on stderr.

This script is the one runnable check for the runner change; plan 02-02
generalizes it into supervisor.py and deletes it.

Usage:
  python3 smoke_runner.py --runner /abs/path/to/GeniusSDKService [--timeout 180]
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

# secp256k1 curve order. Canonical home arrives with plan 02-02's
# e2e/secp256k1_address.py; inlined here so this self-contained stdlib smoke
# stays deletable.
SECP256K1_N = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141

STATUS_PREFIX = "STATUS node_state="


def generate_key_hex() -> str:
    # GeniusSigner::Generate contract: random 32 bytes, retry until 0 < k < N.
    while True:
        k = int.from_bytes(os.urandom(32), "big")
        if 0 < k < SECP256K1_N:
            return "%064x" % k


def write_key_file(path: str, key_hex: str, mode: int) -> None:
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, mode)
    with os.fdopen(fd, "w") as handle:
        handle.write(key_hex + "\n")
    os.chmod(path, mode)  # umask may tighten a fresh file; force the intended mode


def make_node_dir(run_dir: str) -> str:
    # Trailing slash is a contract: service.cpp concatenates the config file
    # name with no separator.
    node_dir = os.path.join(run_dir, "n1") + "/"
    os.makedirs(node_dir, exist_ok=True)
    dev_config = {
        "Address": "0xcafe",
        "Cut": "0.65",
        "TokenValue": "1.0",
        "TokenID": "0x" + "0" * 64,
    }
    with open(node_dir + "dev_config.json", "w") as handle:
        json.dump(dev_config, handle)
    # Single node: no bootstrap_addresses; unique spaced port seed (D-08 class).
    network_config = {"port_seed": 41001, "auto_dht": True}
    with open(node_dir + "network_config.json", "w") as handle:
        json.dump(network_config, handle)
    return node_dir


def read_path(path: str) -> str:
    with open(path, "r", errors="replace") as handle:
        return handle.read()


def tail(path: str, lines: int = 15) -> str:
    return "\n".join(read_path(path).splitlines()[-lines:])


def run_positive(runner: str, run_dir: str, timeout: int) -> None:
    node_dir = make_node_dir(run_dir)
    key_path = os.path.join(run_dir, "identity.key")
    write_key_file(key_path, generate_key_hex(), 0o600)
    out_path = os.path.join(run_dir, "node.out")
    err_path = os.path.join(run_dir, "node.err")

    with open(out_path, "wb") as out, open(err_path, "wb") as err:
        proc = subprocess.Popen(
            [runner, node_dir, "--key-file", key_path], stdout=out, stderr=err
        )
    try:
        deadline = time.monotonic() + timeout
        status_seen = False
        while time.monotonic() < deadline:
            if proc.poll() is not None:
                break
            if any(
                line.startswith(STATUS_PREFIX)
                for line in read_path(out_path).splitlines()
            ):
                status_seen = True
                break
            time.sleep(0.25)
        if not status_seen:
            raise AssertionError(
                "no '%s' line within %ss (exit=%s)\nstdout tail:\n%s\nstderr tail:\n%s"
                % (
                    STATUS_PREFIX,
                    timeout,
                    proc.poll(),
                    tail(out_path),
                    tail(err_path),
                )
            )
        storage_id = os.path.join(node_dir, "secure_storage_id")
        if not os.path.exists(storage_id):
            raise AssertionError(
                "secure_storage_id missing under %s - GeniusSDKInitWithKey did "
                "not run the real account path" % node_dir
            )
        proc.terminate()  # SIGTERM exercises the sigwait shutdown path (D-03)
        proc.wait(timeout=60)
        if proc.returncode != 0:
            raise AssertionError(
                "SIGTERM exit code %s != 0\nstdout tail:\n%s\nstderr tail:\n%s"
                % (proc.returncode, tail(out_path), tail(err_path))
            )
        if "shutting down" not in read_path(out_path):
            raise AssertionError("no shutdown line after SIGTERM\nstdout tail:\n%s"
                                 % tail(out_path))
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait(timeout=30)


def run_negative(runner: str, run_dir: str) -> None:
    node_dir = make_node_dir(os.path.join(run_dir, "negative") + "/")
    key_path = os.path.join(run_dir, "negative", "identity.key")
    write_key_file(key_path, generate_key_hex(), 0o644)  # policy violation
    err_path = os.path.join(run_dir, "negative", "node.err")
    with open(err_path, "wb") as err:
        proc = subprocess.Popen(
            [runner, node_dir, "--key-file", key_path],
            stdout=subprocess.DEVNULL,
            stderr=err,
        )
    proc.wait(timeout=30)
    stderr_text = read_path(err_path)
    if proc.returncode == 0:
        raise AssertionError("0644 key file was accepted (exit 0)")
    if "key file" not in stderr_text:
        raise AssertionError(
            "rejection message does not mention the key file:\n%s" % tail(err_path)
        )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="One-node boot/STATUS/SIGTERM smoke for GeniusSDKService."
    )
    parser.add_argument(
        "--runner",
        required=True,
        help="Absolute path to the GeniusSDKService binary",
    )
    parser.add_argument(
        "--timeout", type=int, default=180, help="Seconds to wait for the STATUS line"
    )
    args = parser.parse_args()

    runner = os.path.abspath(args.runner)
    if not os.path.isfile(runner) or not os.access(runner, os.X_OK):
        sys.exit("runner not executable: %s" % runner)

    run_dir = tempfile.mkdtemp(prefix="geniussdk-smoke-")
    try:
        run_positive(runner, run_dir, args.timeout)
        run_negative(runner, run_dir)
    except AssertionError as failure:
        main_err = os.path.join(run_dir, "node.err")
        detail = tail(main_err) if os.path.exists(main_err) else ""
        sys.exit(
            "SMOKE FAIL (run dir preserved: %s)\n%s\n%s"
            % (run_dir, failure, detail)
        )
    except Exception as failure:  # preserve artifacts on any failure
        sys.exit("SMOKE FAIL (run dir preserved: %s)\n%r" % (run_dir, failure))
    shutil.rmtree(run_dir)
    print("SMOKE PASS")
    sys.exit(0)


if __name__ == "__main__":
    main()
