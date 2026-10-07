#!/usr/bin/env python3
"""
supervisor.py — process supervision for the ceremony e2e harness.

Spawns runner/actor processes with stdout and stderr each captured to
<run_dir>/<name>.out / <name>.err as they run (the file IS the artifact —
D-17 failure evidence is materialized the moment anything writes it), gates on
bounded observable output rather than sleeps, and escalates SIGTERM -> SIGKILL
on teardown. Gates scan every capture file for a process — the tee'd .out/.err
plus any extra_logs the caller registered, because node log lines (e.g. the
PubSub multiaddr) are emitted through the node logger, whose sink in this
build is <base>/sgnslog2.log rather than stdout/stderr.

All spawns use argv lists only — never through a shell (T-02-08).
"""

import os
import re
import signal
import subprocess
import time

POLL_INTERVAL = 0.25  # seconds between gate polls; gates never sleep longer

_processes = {}  # name -> {"proc", "out", "err", "extra", "handles"}

_STATUS_RE = re.compile(r"STATUS node_state=(\S+)")
# The D-08 additive field: runner STATUS lines carry head=<genesis CID> only
# once the node is READY and the accessor succeeded. One home per format fact:
# supervisor owns STATUS parsing (state and head beside each other).
_HEAD_RE = re.compile(r"STATUS node_state=\S+ init=\S+ head=(\S+)")


class GateError(RuntimeError):
    """A bounded wait failed: process, pattern, timeout, and captured tails."""


def spawn(name, argv, run_dir, extra_logs=()):
    """Start argv under supervision, capturing both sinks to the run dir.

    Returns the Popen. extra_logs are additional files (e.g. the node's own
    sgnslog2.log) included in every gate scan for this name.
    """
    out_path = os.path.join(run_dir, name + ".out")
    err_path = os.path.join(run_dir, name + ".err")
    out_handle = open(out_path, "wb")
    err_handle = open(err_path, "wb")
    try:
        proc = subprocess.Popen(
            list(argv), stdout=out_handle, stderr=err_handle, start_new_session=True
        )
    except BaseException:
        out_handle.close()
        err_handle.close()
        raise
    _processes[name] = {
        "proc": proc,
        "out": out_path,
        "err": err_path,
        "extra": list(extra_logs),
        "handles": (out_handle, err_handle),
    }
    return proc


def _capture_paths(name):
    record = _processes[name]
    return [record["out"], record["err"]] + record["extra"]


def _read(path):
    try:
        with open(path, "rb") as handle:
            return handle.read().decode("utf-8", errors="replace")
    except FileNotFoundError:
        return ""


def _combined(name):
    return "\n".join(_read(path) for path in _capture_paths(name))


def _tails(name, lines=15):
    parts = []
    for path in _capture_paths(name):
        captured = _read(path).splitlines()
        if captured:
            parts.append("--- %s (last %d lines) ---" % (path, lines))
            parts.extend(captured[-lines:])
    return "\n".join(parts) if parts else "(no captured output yet)"


def wait_for(name, pattern, timeout):
    """Bounded gate: poll every capture file until pattern matches.

    Returns the re.Match on the combined capture text. Raises GateError when
    the process exits first (with its exit code) or the timeout elapses —
    both name the process, pattern, and last observed lines from every sink.
    """
    regex = re.compile(pattern)
    deadline = time.monotonic() + timeout
    while True:
        match = regex.search(_combined(name))
        if match:
            return match
        proc = _processes[name]["proc"]
        if proc.poll() is not None:
            # Final flush may have landed after the last read; look once more
            # before declaring an early exit.
            match = regex.search(_combined(name))
            if match:
                return match
            raise GateError(
                "%s exited with code %s before any line matched %r\n%s"
                % (name, proc.returncode, pattern, _tails(name))
            )
        if time.monotonic() >= deadline:
            raise GateError(
                "timeout after %ss waiting for %s to print %r\n%s"
                % (timeout, name, pattern, _tails(name))
            )
        time.sleep(POLL_INTERVAL)


def state_of(name):
    """Latest runner STATUS state across the combined capture, or None."""
    states = _STATUS_RE.findall(_combined(name))
    return states[-1] if states else None


def head_of(name):
    """Latest STATUS head= (genesis CID) across the combined capture, or None."""
    heads = _HEAD_RE.findall(_combined(name))
    return heads[-1] if heads else None


def terminate(name, timeout=60):
    """SIGTERM (the runner's sigwait path), wait, then SIGKILL escalation.

    Returns the process exit code, or None if the name was never spawned.
    """
    record = _processes.pop(name, None)
    if record is None:
        return None
    proc = record["proc"]
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=30)
    for handle in record["handles"]:
        if not handle.closed:
            handle.close()
    return proc.returncode


def kill(name, timeout=30):
    """SIGKILL a supervised process — real-crash simulation (D-06), the
    inverse of spawn. The pid is the supervisor's own Popen (never a shell,
    never a pgrep pattern). ALWAYS reaps before returning (Pitfall 10:
    respawn racing the dying process's DB handles) and pops the record, the
    same discipline as terminate. Returns the exit code, or None if the name
    was never spawned."""
    record = _processes.pop(name, None)
    if record is None:
        return None
    proc = record["proc"]
    if proc.poll() is None:
        os.kill(proc.pid, signal.SIGKILL)
        proc.wait(timeout=timeout)
    for handle in record["handles"]:
        if not handle.closed:
            handle.close()
    return proc.returncode


def teardown(timeout=60):
    """Terminate every supervised process; always safe to call. Returns
    {name: exit_code} for what it actually stopped."""
    return {
        name: terminate(name, timeout=timeout) for name in list(_processes)
    }
