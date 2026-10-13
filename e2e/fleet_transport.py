#!/usr/bin/env python3
"""
fleet_transport.py — the transport seam for the fleet ceremony (04-03, D-07
Shape A).

The one-time fleet ceremony runs from the operator's machine: sgns-trust
actors dial the genesis node's published multiaddr over libp2p, while the ONLY
co-located steps (B_1 read, config push, service restart, log scrape) go
through this seam — SSH for the real fleet, local for the proof run. The seam
is the sanctioned test boundary (04-RESEARCH Pattern 4): the exact code path
Wave 5 runs against real machines is proven green locally through
LocalTransport first.

The contract — exactly the five ops the ceremony needs:
  exec(cmd)                      -> (rc, stdout)   cmd is an argv LIST
  read_file(path)                -> str
  write_file(path, content, mode=0o600)
  restart(node_ref)              -> make the node run its current config
  log_lines(node_ref)            -> recent text from the node's supervision
                                     surface (journald / docker logs on Linux;
                                     supervisor.combined locally — the 04-01
                                     public accessor, out + err + extra logs,
                                     correct on both macOS and Linux logging)

SECURITY GUARD (T-04-03-01): every command is an argv list executed WITHOUT a
shell — this module contains zero shell=True and zero remote string
interpolation of untrusted input. Paths and hosts originate from the reviewed
fleet.json only; nothing that crossed a process-output boundary is ever
spliced into a command string. Do not add either pattern.
"""

import supervisor


class FleetTransportError(RuntimeError):
    """A transport op failed loudly: host + op + cause (never swallowed)."""


def _selftest():
    import os
    import subprocess
    import tempfile
    import time

    # --- LocalTransport: exec / read_file / write_file roundtrips ---
    with tempfile.TemporaryDirectory(prefix="fleet-transport-") as scratch:
        local = LocalTransport(supervisor, lambda ref: supervisor.spawn(
            ref["name"], ref["argv"], scratch))
        rc, out = local.exec(["true"])
        assert rc == 0, "exec(['true']) must return rc 0 (got %s)" % rc
        rc, out = local.exec(["printf", "fleet-transport-roundtrip"])
        assert rc == 0 and out == "fleet-transport-roundtrip", (rc, out)
        probe = os.path.join(scratch, "probe.txt")
        local.write_file(probe, "byte-identical\npayload", mode=0o600)
        assert local.read_file(probe) == "byte-identical\npayload"
        assert (os.stat(probe).st_mode & 0o777) == 0o600, \
            "write_file must enforce the policy mode (umask may tighten only)"
        local.write_file(probe, "rewritten", mode=0o640)
        assert (os.stat(probe).st_mode & 0o777) == 0o640
        try:
            local.read_file(os.path.join(scratch, "absent.txt"))
        except FleetTransportError as error:
            assert "local" in str(error) and "read_file" in str(error), \
                "the failure must name host + op: %s" % error
        else:
            raise AssertionError("read_file of an absent path must raise")

    # --- LocalTransport: restart/log_lines map onto supervisor spawns ---
    with tempfile.TemporaryDirectory(prefix="fleet-transport-sup-") as scratch:
        node = {"name": "fleet-selftest-node", "argv": ["sleep", "30"]}

        def spawn_ref(ref):
            supervisor.spawn(ref["name"], ref["argv"], scratch)

        local = LocalTransport(supervisor, spawn_ref)
        local.restart(node)  # start (nothing running yet)
        marker = {"name": "fleet-selftest-marker", "argv": ["printf", "m\n"]}
        spawn_ref(marker)
        deadline = time.monotonic() + 10
        while "m" not in supervisor.combined(marker["name"]):
            assert time.monotonic() < deadline, "marker never captured"
            time.sleep(0.1)
        # log_lines IS the combined() evidence set (out + err + extra logs).
        assert local.log_lines(marker["name"]) == \
            supervisor.combined(marker["name"])
        proc = supervisor._processes[node["name"]]["proc"]
        first_pid = proc.pid
        local.restart(node)  # terminate + respawn
        assert supervisor._processes[node["name"]]["proc"].pid != first_pid, \
            "restart must terminate the running process and respawn it"
        supervisor.teardown(timeout=10)

    # --- SshTransport: constructor validation ---
    for bad in (None, "", "  ", "-leading-dash", "host with space"):
        try:
            SshTransport(bad)
        except FleetTransportError:
            pass
        else:
            raise AssertionError("host %r must be rejected" % (bad,))

    # --- SshTransport: exact argv shapes, dry-parsed (no connection) ---
    ssh = SshTransport("fleet-box.example")
    assert ssh.exec_argv(["true"]) == \
        ["ssh", "-o", "BatchMode=yes", "fleet-box.example", "true"]
    aws_node = {"name": "node1", "transport": {
        "kind": "ssh", "host": "fleet-box.example",
        "unit_name": "sgns-staging-node.service"}}
    assert ssh.restart_argv(aws_node) == \
        ["ssh", "-o", "BatchMode=yes", "fleet-box.example",
         "systemctl", "restart", "sgns-staging-node.service"]
    assert ssh.logs_argv(aws_node) == \
        ["ssh", "-o", "BatchMode=yes", "fleet-box.example",
         "journalctl", "-u", "sgns-staging-node.service", "-n", "200",
         "--no-pager"]
    nas_node = {"name": "node2", "transport": {
        "kind": "compose", "host": "nas.example", "service": "staging-peer-2"}}
    assert ssh.restart_argv(nas_node) == \
        ["ssh", "-o", "BatchMode=yes", "nas.example",
         "docker", "restart", "staging-peer-2"]
    assert ssh.logs_argv(nas_node) == \
        ["ssh", "-o", "BatchMode=yes", "nas.example",
         "docker", "logs", "--tail", "200", "staging-peer-2"]
    for broken in ({"kind": "ssh", "host": "h"},          # no unit_name
                   {"kind": "compose", "host": "h"},      # no service
                   {"kind": "local", "host": "h"}):       # not an ssh kind
        try:
            ssh.restart_argv({"name": "n", "transport": broken})
        except FleetTransportError:
            pass
        else:
            raise AssertionError("broken transport %r must raise" % (broken,))


def main():
    import argparse
    parser = argparse.ArgumentParser(
        description="Selftest for the fleet transport seam (local roundtrips "
                    "+ dry-parsed SshTransport argv shapes; no connections).")
    parser.add_argument("--selftest", action="store_true",
                        help="run the seam selftest")
    args = parser.parse_args()
    if args.selftest:
        _selftest()
        print("fleet_transport selftest PASS")
        return
    parser.error("nothing to do; use --selftest")


if __name__ == "__main__":
    main()
