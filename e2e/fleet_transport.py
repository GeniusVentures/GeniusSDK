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
shell — this module spawns nothing through a shell (zero shell-True spawns)
and performs zero remote string interpolation of untrusted input. Paths and
hosts originate from the reviewed fleet.json only; nothing that crossed a
process-output boundary is ever spliced into a command string. Do not add
either pattern.
"""

import os
import subprocess

import supervisor


class FleetTransportError(RuntimeError):
    """A transport op failed loudly: host + op + cause (never swallowed)."""


class FleetTransport:
    """The seam's public contract, defined exactly once (template method):
    exec / read_file / write_file / restart / log_lines. Subclasses implement
    the _exec/_read/_write/_restart/_logs primitives; _guard wraps every
    primitive failure in a FleetTransportError naming host + op — nothing is
    ever swallowed."""

    def exec(self, cmd):
        """Run an argv list to completion; returns (rc, stdout)."""
        return self._guard("exec", self._exec, list(cmd))

    def read_file(self, path):
        return self._guard("read_file", self._read, path)

    def write_file(self, path, content, mode=0o600):
        self._guard("write_file", self._write, path, content, mode)

    def restart(self, node_ref):
        self._guard("restart", self._restart, node_ref)

    def log_lines(self, node_ref):
        return self._guard("log_lines", self._logs, node_ref)

    def _guard(self, op, function, *args):
        try:
            return function(*args)
        except FleetTransportError:
            raise
        except BaseException as error:
            raise FleetTransportError("%s: %s failed: %s"
                                      % (self._where(), op, error)) from error


class LocalTransport(FleetTransport):
    """The test seam AND the local-fleet mapping: exec via subprocess argv,
    read/write via the filesystem with the policy mode forced, restart and
    log_lines delegating to the injected supervisor instance (the supervisor
    module) plus the caller's spawn function. log_lines returns
    supervisor.combined(name) — the 04-01 public accessor for the FULL
    evidence set (tee'd .out/.err + registered extra logs): the same set
    wait_for/state_of scan, correct on both macOS (node logger file sink) and
    Linux (node logger rides stdout)."""

    def __init__(self, sup, spawn_node):
        self._sup = sup
        self._spawn_node = spawn_node

    def _where(self):
        return "local"

    def _exec(self, cmd):
        result = subprocess.run(cmd, capture_output=True, text=True)
        return result.returncode, result.stdout

    def _read(self, path):
        with open(path, encoding="utf-8", errors="replace") as handle:
            return handle.read()

    def _write(self, path, content, mode):
        # topology.write_key_file discipline: write, then force the policy
        # mode (umask may tighten a fresh file; chmod re-asserts it).
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(content)
        os.chmod(path, mode)

    def _restart(self, node_ref):
        name = node_ref["name"]
        self._sup.terminate(name)  # no-op when nothing is running
        self._spawn_node(node_ref)

    def _logs(self, node_ref):
        return self._sup.combined(node_ref["name"])


class SshTransport(FleetTransport):
    """The real-fleet mapping: every op is ["ssh", -o BatchMode=yes, <host>,
    <command...>] — argv lists only, key auth assumed (established in Wave 4's
    preflight; BatchMode refuses interactive prompts instead of hanging).
    restart maps to systemctl restart <unit> for ssh-kind nodes and docker
    restart <service> for compose-kind nodes (the compose host is the NAS ssh
    host); log_lines maps to journalctl -u <unit> -n 200 --no-pager /
    docker logs --tail 200 <service> — both carry stdout, exactly the surface
    Linux nodes log to (the 04-01 CI lesson)."""

    def __init__(self, host):
        if (not isinstance(host, str) or not host.strip()
                or host.startswith("-") or any(c.isspace() for c in host)):
            raise FleetTransportError(
                "SshTransport needs a non-empty single-token host (no leading "
                "dash, no whitespace; hosts come from the reviewed fleet.json "
                "only): %r" % (host,))
        self._host = host

    def _where(self):
        return "ssh %s" % self._host

    def _argv(self, command):
        # T-04-03-01: argv list only — never a shell, never string
        # interpolation of anything that crossed a process boundary.
        return ["ssh", "-o", "BatchMode=yes", self._host] + list(command)

    def build_exec_argv(self, command):
        """The exact exec argv for a remote command (dry-parse: no connect)."""
        return self._argv(command)

    def _remote_restart(self, node_ref):
        transport = node_ref.get("transport") or {}
        kind = transport.get("kind")
        if kind == "ssh":
            unit = transport.get("unit_name")
            if not unit:
                raise FleetTransportError(
                    "%s: ssh-kind node %s has no unit_name"
                    % (self._where(), node_ref.get("name")))
            return ["systemctl", "restart", unit]
        if kind == "compose":
            service = transport.get("service")
            if not service:
                raise FleetTransportError(
                    "%s: compose-kind node %s has no service"
                    % (self._where(), node_ref.get("name")))
            return ["docker", "restart", service]
        raise FleetTransportError("%s: node %s transport kind %r is not "
                                  "remote (ssh|compose)"
                                  % (self._where(), node_ref.get("name"), kind))

    def _remote_logs(self, node_ref):
        transport = node_ref.get("transport") or {}
        kind = transport.get("kind")
        if kind == "ssh":
            unit = transport.get("unit_name")
            if not unit:
                raise FleetTransportError(
                    "%s: ssh-kind node %s has no unit_name"
                    % (self._where(), node_ref.get("name")))
            return ["journalctl", "-u", unit, "-n", "200", "--no-pager"]
        if kind == "compose":
            service = transport.get("service")
            if not service:
                raise FleetTransportError(
                    "%s: compose-kind node %s has no service"
                    % (self._where(), node_ref.get("name")))
            return ["docker", "logs", "--tail", "200", service]
        raise FleetTransportError("%s: node %s transport kind %r is not "
                                  "remote (ssh|compose)"
                                  % (self._where(), node_ref.get("name"), kind))

    def build_restart_argv(self, node_ref):
        """The exact restart argv (dry-parse: no connect)."""
        return self._argv(self._remote_restart(node_ref))

    def build_logs_argv(self, node_ref):
        """The exact log_lines argv (dry-parse: no connect)."""
        return self._argv(self._remote_logs(node_ref))

    def _run_argv(self, argv, input_text=None):
        return subprocess.run(argv, input=input_text, capture_output=True,
                              text=True)

    def _exec(self, cmd):
        result = self._run_argv(self._argv(cmd))
        return result.returncode, result.stdout

    def _read(self, path):
        result = self._run_argv(self._argv(["cat", path]))
        if result.returncode != 0:
            raise FleetTransportError(
                "%s: read_file %s: cat exited %s: %s"
                % (self._where(), path, result.returncode,
                   result.stderr.strip()))
        return result.stdout

    def _write(self, path, content, mode):
        # tee truncates and streams stdin to the file (no shell redirect);
        # chmod then forces the policy mode (tee's umask default is not it).
        teed = self._run_argv(self._argv(["tee", path]), input_text=content)
        if teed.returncode != 0:
            raise FleetTransportError(
                "%s: write_file %s: tee exited %s: %s"
                % (self._where(), path, teed.returncode, teed.stderr.strip()))
        fixed = self._run_argv(self._argv(["chmod", format(mode, "o"), path]))
        if fixed.returncode != 0:
            raise FleetTransportError(
                "%s: write_file %s: chmod %s exited %s: %s"
                % (self._where(), path, format(mode, "o"),
                   fixed.returncode, fixed.stderr.strip()))

    def _restart(self, node_ref):
        result = self._run_argv(self.build_restart_argv(node_ref))
        if result.returncode != 0:
            raise FleetTransportError(
                "%s: restart %s exited %s: %s"
                % (self._where(), node_ref.get("name"), result.returncode,
                   (result.stderr or result.stdout).strip()))

    def _logs(self, node_ref):
        result = self._run_argv(self.build_logs_argv(node_ref))
        if result.returncode != 0:
            raise FleetTransportError(
                "%s: log_lines %s exited %s: %s"
                % (self._where(), node_ref.get("name"), result.returncode,
                   (result.stderr or result.stdout).strip()))
        # docker logs splits container stdout/stderr across our sinks — the
        # node supervision surface is the union.
        return result.stdout + result.stderr


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
        assert local.log_lines(marker) == \
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
    assert ssh.build_exec_argv(["true"]) == \
        ["ssh", "-o", "BatchMode=yes", "fleet-box.example", "true"]
    aws_node = {"name": "node1", "transport": {
        "kind": "ssh", "host": "fleet-box.example",
        "unit_name": "sgns-staging-node.service"}}
    assert ssh.build_restart_argv(aws_node) == \
        ["ssh", "-o", "BatchMode=yes", "fleet-box.example",
         "systemctl", "restart", "sgns-staging-node.service"]
    assert ssh.build_logs_argv(aws_node) == \
        ["ssh", "-o", "BatchMode=yes", "fleet-box.example",
         "journalctl", "-u", "sgns-staging-node.service", "-n", "200",
         "--no-pager"]
    nas_node = {"name": "node2", "transport": {
        "kind": "compose", "host": "nas.example", "service": "staging-peer-2"}}
    nas = SshTransport("nas.example")  # one transport per host (the NAS ssh
    # host serves every compose-kind node)
    assert nas.build_restart_argv(nas_node) == \
        ["ssh", "-o", "BatchMode=yes", "nas.example",
         "docker", "restart", "staging-peer-2"]
    assert nas.build_logs_argv(nas_node) == \
        ["ssh", "-o", "BatchMode=yes", "nas.example",
         "docker", "logs", "--tail", "200", "staging-peer-2"]
    for broken in ({"kind": "ssh", "host": "h"},          # no unit_name
                   {"kind": "compose", "host": "h"},      # no service
                   {"kind": "local", "host": "h"}):       # not an ssh kind
        try:
            ssh.build_restart_argv({"name": "n", "transport": broken})
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
