#!/usr/bin/env bash
# sgns-node-wrap.sh — the Secret Service keyring wrapper for GeniusSDKService
# on Linux, as an idempotent script (the CI-proven dbus-run-session block from
# ceremony-e2e.yml, verbatim mechanics). ONE file serves BOTH deploy targets:
# the AWS systemd unit (dbus-run-session -- /usr/local/bin/sgns-node-wrap.sh)
# and the NAS containers (the image ENTRYPOINT) — every path arrives through
# the ENVIRONMENT (the AWS unit's EnvironmentFile or the compose environment
# block); this script hardcodes none. The same wrapper also boots the keyless
# joiner (fresh account) when GENIUSSDK_KEYLESS=1 is set — an explicit
# opt-in, so every other path (the fleet units, whose EnvironmentFile is
# optional) fails fast on a missing key instead of silently booting a fresh
# identity. The keyring seeding below is unconditional because the dbus
# session, not the key, is the hard Linux requirement.
#
# Required environment:
#   GENIUSSDK_BIN      path to the GeniusSDKService binary
#   GENIUSSDK_BASE     the node's base dir WITH trailing slash
#   GENIUSSDK_KEY_FILE path to the 0600 identity key (user-owned); the fleet
#                      shape — REQUIRED unless GENIUSSDK_KEYLESS=1
# Optional environment:
#   GENIUSSDK_KEYLESS  set to exactly 1 to boot the keyless joiner shape
#                      (fresh account, D-11); mutually exclusive with
#                      GENIUSSDK_KEY_FILE
#
# Keyring seeding is idempotent (only when absent): the files persist on the
# service user's $HOME volume — on the NAS peers HOME=/data, the mounted
# persistent volume — so the empty-password login keyring and the node
# identity it guards survive reboots and container recreation.
set -euo pipefail

: "${GENIUSSDK_BIN:?GENIUSSDK_BIN must be set (path to GeniusSDKService)}"
: "${GENIUSSDK_BASE:?GENIUSSDK_BASE must be set (node base dir, trailing slash)}"
# Normalize for the set -u safe tests below; the :? in the keyed branch still
# fires on empty (the fleet fail-fast), and GENIUSSDK_KEYLESS=1 is the only
# way past it (the joiner shape, D-11).
GENIUSSDK_KEY_FILE="${GENIUSSDK_KEY_FILE:-}"

mkdir -p "$HOME/.cache" "$HOME/.local/share/keyrings"
if [ ! -f "$HOME/.local/share/keyrings/default" ]; then
    printf 'login' > "$HOME/.local/share/keyrings/default"
fi
if [ ! -f "$HOME/.local/share/keyrings/login.keyring" ]; then
    printf '[keyring]\ndisplay-name=login\nlock-on-idle=false\nlock-after=false\n' \
        > "$HOME/.local/share/keyrings/login.keyring"
fi

# Empty-password unlock of the secrets component (headless service shape,
# proven daily in CI; staging-grade — the mainnet path differs, out of scope).
eval "$(printf '\n' | gnome-keyring-daemon --unlock --components=secrets)"

# Honest args, env-indirected: exec replaces the shell so the node is PID of
# record under the session supervisor (systemd/container restart policy).
# KEYLESS=1 is the explicit joiner opt-in (D-11) — every other shape must
# name a key, so a misconfigured fleet node fails its unit here instead of
# silently booting a fresh identity.
if [ "${GENIUSSDK_KEYLESS:-0}" = "1" ]; then
    if [ -n "$GENIUSSDK_KEY_FILE" ]; then
        echo "FATAL: GENIUSSDK_KEYLESS=1 and GENIUSSDK_KEY_FILE are mutually exclusive" >&2
        exit 1
    fi
    args=("$GENIUSSDK_BASE")
else
    : "${GENIUSSDK_KEY_FILE:?GENIUSSDK_KEY_FILE must be set (0600 identity key), or set GENIUSSDK_KEYLESS=1 for the joiner shape (D-11)}"
    args=("$GENIUSSDK_BASE" "--key-file" "$GENIUSSDK_KEY_FILE")
fi
exec "$GENIUSSDK_BIN" "${args[@]}"
