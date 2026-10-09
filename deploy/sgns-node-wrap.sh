#!/usr/bin/env bash
# sgns-node-wrap.sh — the Secret Service keyring wrapper for GeniusSDKService
# on Linux, as an idempotent script (the CI-proven dbus-run-session block from
# ceremony-e2e.yml, verbatim mechanics). ONE file serves BOTH deploy targets:
# the AWS systemd unit (dbus-run-session -- /usr/local/bin/sgns-node-wrap.sh)
# and the NAS containers (the image ENTRYPOINT) — every path arrives through
# the ENVIRONMENT (the AWS unit's EnvironmentFile or the compose environment
# block); this script hardcodes none.
#
# Required environment:
#   GENIUSSDK_BIN      path to the GeniusSDKService binary
#   GENIUSSDK_BASE     the node's base dir WITH trailing slash
#   GENIUSSDK_KEY_FILE path to the 0600 identity key (user-owned)
#
# Keyring seeding is idempotent (only when absent): the files persist on the
# service user's $HOME volume — on the NAS peers HOME=/data, the mounted
# persistent volume — so the empty-password login keyring and the node
# identity it guards survive reboots and container recreation.
set -euo pipefail

: "${GENIUSSDK_BIN:?GENIUSSDK_BIN must be set (path to GeniusSDKService)}"
: "${GENIUSSDK_BASE:?GENIUSSDK_BASE must be set (node base dir, trailing slash)}"
: "${GENIUSSDK_KEY_FILE:?GENIUSSDK_KEY_FILE must be set (0600 identity key)}"

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
exec "$GENIUSSDK_BIN" "$GENIUSSDK_BASE" --key-file "$GENIUSSDK_KEY_FILE"
