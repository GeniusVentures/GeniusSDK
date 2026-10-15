# The NAS staging-peer node-runtime image (further-amended D-03).
#
# Thin assembly only — the binary is NOT rebuilt here: it is the e2e job's
# EL8-container Release build (the matching userland), uploaded as the CI
# runner-binary artifact and staged into the build context before building.
#
# Expected context layout (context = deploy/, so from GeniusSDK/deploy/):
#   docker build -f nas/Dockerfile -t ghcr.io/geniusventures/geniussdk-staging-node:staging .
#     ./sgns-node-wrap.sh            <- this repo (the wrapper the unit also uses)
#     ./nas/<runner binary>          <- CI stages the artifact binary here
#                                        (consumed by the COPY line below)
#
# The image carries the NODE RUNTIME ONLY (further-amended D-03): no ceremony
# tooling (actors stay on the operator machine), no configs, no keys —
# per-peer config bundles and the 0600 key are MOUNTED at run time by
# docker-compose.yml.
# IPC_LOCK is not a Dockerfile concern: it is a runtime capability the compose
# grants (EL8 gnome-keyring-daemon carries file capabilities; a container
# bounding set without IPC_LOCK makes its exec return EPERM).
FROM ghcr.io/geniusventures/almalinux-8:latest

# Runtime counterparts of the CI build list — explicit even where the org base
# image already carries them (the wrapper needs gnome-keyring + dbus; the node
# needs libsecret/libzstd/vulkan-loader at load time).
RUN dnf install -y gnome-keyring dbus libsecret libzstd vulkan-loader \
    && dnf clean all

COPY nas/GeniusSDKService /usr/local/bin/GeniusSDKService
COPY sgns-node-wrap.sh /usr/local/bin/sgns-node-wrap.sh
RUN chmod 0755 /usr/local/bin/*

# dbus-run-session stays the outer shape — one home for the Secret Service
# wrapper (the same nesting the AWS systemd unit uses).
ENTRYPOINT ["/usr/bin/dbus-run-session", "--", "/usr/local/bin/sgns-node-wrap.sh"]
