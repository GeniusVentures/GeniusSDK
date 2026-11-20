# Updating the node binary (manual service update)

One documented procedure for the most common fleet operation: swapping the
`GeniusSDKService` runner binary on a live deployment WITHOUT touching the
ceremony, the identities, or the databases. This is the binaries-only path —
the trust payload, key files, base dirs, and keyrings are all durable state
this procedure never modifies. If what needs to change is the net itself
(membership, manifest, keys), that is a re-ceremony — see
`deploy/staging-net/MANUAL-CEREMONY.md` — never this document.

Everything below applies to ANY deployment of the runner through the
`deploy/systemd/` unit shape and `deploy/sgns-node-wrap.sh` (the staging
fleet's template instances `sgns-staging-node@<i>.service`, the single
genesis unit, or a custom unit following the same EnvironmentFile pattern).
Host-specific values use the Readme's placeholder convention:
`[FLEET_HOST]`, `[STAGING_UPLOAD_DIR]`, `[BINARY_PATH]`.

## 0. What this swap does and does not touch

- Swaps ONE file: the binary named by `GENIUSSDK_BIN` (the shared
  `/etc/geniussdk/common.conf` on the staging shape — typically
  `/usr/local/bin/GeniusSDKService`).
- Does NOT touch: key files, base dirs (`GENIUSSDK_BASE`), node databases,
  the keyring files under the service user's `$HOME`, unit files, or any
  EnvironmentFile.
- systemd's stop/start manages the WHOLE unit cgroup: the node plus its
  `dbus-run-session` + gnome-keyring wrapper go down and come back together.
  The wrapper re-seeds the keyring only when absent, so identity survives
  every restart with no manual keyring work.

## 1. Obtain a validated binary

Two sanctioned sources, strongest provenance first:

1. **A green Ceremony E2E CI run's `runner-binary` artifact** — the exact
   build that completed the full ceremony suite (happy path plus whatever
   scenarios that run exercised), bundled with `runner-ldd.txt` (its `ldd`
   closure and sha256). Retention is 14 days.
   `gh run download [RUN_ID] -n runner-binary -D [LOCAL_DIR] -R GeniusVentures/GeniusSDK`
2. **A GitHub release** (the `build-release-tag.yml` channel) — the durable
   distribution path when the artifact has expired.

Caveats before trusting either:

- The e2e build consumes the SuperGenius `develop` release tarball, not
  SuperGenius source. If your change lives in SuperGenius, confirm the
  e2e run started AFTER the SuperGenius release workflow published the
  tarball containing it (the run's "Download SuperGenius" step logs which
  tarball it pulled) — otherwise a green run proves the previous engine.
- Verify the download: `sha256sum` the binary and compare against the
  `sha256:` line in `runner-ldd.txt` (or the release digest).
- CI binaries are built on AlmaLinux 8 (glibc 2.28 floor) and run on any
  newer host (e.g. Ubuntu 24.04). Run `ldd [BINARY]` on the target host and
  resolve every `not found` BEFORE stopping anything: either install the
  package or ship the library beside the binary and set `LD_LIBRARY_PATH`
  in `common.conf`.

## 2. Stage the upload on the host

Copy the binary up under a DATED directory — the staging host's convention
is `[STAGING_UPLOAD_DIR]/YYYYMMDD/` (e.g. `/home/ubuntu/staging-updates/20261119/`)
so every swap leaves its own input behind as the rollback copy:

```bash
scp [LOCAL_DIR]/GeniusSDKService [FLEET_HOST]:[STAGING_UPLOAD_DIR]/[YYYYMMDD]/
ssh [FLEET_HOST] 'sha256sum [STAGING_UPLOAD_DIR]/[YYYYMMDD]/GeniusSDKService'
```

The on-box sha256 must equal the source digest from step 1.

## 3. Record the current state (rollback facts)

Before stopping anything, capture what "current" means:

```bash
grep GENIUSSDK_BIN /etc/geniussdk/common.conf        # the binary in service
sha256sum [BINARY_PATH]                              # its digest
systemctl list-units 'sgns-*' --no-pager             # the running units
```

Note: EnvironmentFile values are re-read at every unit start — a
`GENIUSSDK_BIN` change needs NO `systemctl daemon-reload` (that is only for
unit-file edits).

## 4. Stop the nodes

```bash
sudo systemctl stop sgns-staging-node@2.service sgns-staging-node@3.service \
                    sgns-staging-node@4.service sgns-staging-node@5.service
sudo systemctl stop sgns-staging-node@1.service      # genesis last
```

- `stop` (never `kill`): the unit's `Restart=on-failure` does not fire on a
  clean stop, so the nodes stay down while you work.
- Stop order is immaterial on the staging net (frozen-head semantics — the
  quorum-loss scenario proved survivors hold the head), but peers-first /
  genesis-last is the conventional shape.
- Over SSH or in scripts on the staging host: the admin user has
  passwordless sudo but NO polkit manage-units grant, so a bare
  `systemctl stop` over ssh fails "Interactive authentication required" —
  use `sudo -n systemctl ...` (observed live 04-05).
- Downtime choice: full stop (this document) gives an atomic version change
  — no mixed-version mesh window. A rolling restart (one unit at a time,
  keeping 3-of-5 quorum) minimizes downtime of the published entry and is
  equally safe on this net; pick it only if that matters.

## 5. Swap the binary

In-place overwrite (the staging shape — requires the stop, Linux refuses to
write a running executable with ETXTBSY):

```bash
sudo install -m 0755 [STAGING_UPLOAD_DIR]/[YYYYMMDD]/GeniusSDKService \
    [BINARY_PATH]
sha256sum [BINARY_PATH]      # must equal the staged digest
```

The previous binary remains available in the older dated upload directory —
that is the rollback copy. The alternative shape is a versioned bin
directory (`.../bin/3.7.x/GeniusSDKService`) with `GENIUSSDK_BIN`
repointed in `common.conf`; rollback is then a one-line config edit, at the
cost of one extra fact on the host.

## 6. Start and verify

Start the genesis first, then the peers:

```bash
sudo -n systemctl start sgns-staging-node@1.service
sudo -n systemctl start sgns-staging-node@{2..5}.service
```

Verify each node (`journalctl -u sgns-staging-node@1.service -n 200 --no-pager`):

- The STATUS cadence reaches `node_state=READY` — the norm is ~2 minutes;
  60–90s in `INITIALIZING_BLOCKCHAIN` first is normal, not stuck, and
  `Scheduling blockchain retry after failure` lines DURING that window are
  expected noise (see `deploy/staging-net/JOIN.md` §6). CAVEAT: the ~2 min
  norm is a fresh joiner on a dedicated box. On the fleet shape (five
  nodes sharing one VPS) the first boot after a swap can take
  considerably longer — one observed swap printed its first READY ~25
  min after unit start. The working-vs-stuck signal is ACTIVITY in the
  journal during the gap (sync/routing lines such as GraphsyncDAGSyncer),
  not elapsed time alone. Note too that sink routing is per-deployment
  (`log_config.json`): on the systemd fleet shape log output lands in the
  journal; `sgnslog2.log` in the base dir is the JOINER bundle's routing,
  not this one's.
- On the staging net, the head must equal the artifact's genesis CID —
  the same value every node held before the swap:

```bash
jq -r .genesis_cid deploy/staging-net/staging-net.json   # from this repo
journalctl -u sgns-staging-node@1.service | grep -oE 'head=[A-Za-z0-9]+' | sort -u
```

The update is proven when every node reports READY and the sampled `head=`
values match the artifact's `genesis_cid` exactly.

## 7. Rollback if unhealthy

Any node that will not settle back to READY at the same head: stop the
units, restore the previous binary (step 5 in reverse — `install` the older
dated upload back over `[BINARY_PATH]`, or repoint `GENIUSSDK_BIN` to the
versioned directory), start, and re-verify. Nothing else needs undoing —
the databases and trust state were never touched.

## 8. Post-update follow-ups (staging net only)

- `deploy/staging-net/staging-net.json` pins `node_version`,
  `supergenius_tarball_sha256`, and `runner_sha256` against the EXACT
  fleet binaries (JOIN.md §2). After a swap all three are stale until
  republished: update them from OBSERVED values (the new runner's
  reported version string, the sha256 of the SuperGenius release tarball
  the build consumed, and the new runner's sha256) — either by hand or
  through `e2e/staging_artifact.py` (`write_staging_artifact` validates
  every field and copies `manifest.bin` byte-identical). Every other
  fact — the `ceremony_fingerprint`, `genesis_cid`, multiaddr, and
  addresses — is unchanged by a binaries-only update. Commit the pair to
  `develop` (joiners and the `staging-*` CI legs fetch it by raw URL from
  `develop`; the repin takes effect only once landed there). A
  major.minor version change is NEVER a drop-in swap — the pubsub topic
  appendix carries `.{major}.{minor}.{net}`, so 3.7 -> 3.8 forks the
  net's topics and requires a coordinated migration, not this document.
- Optional live-net post-gate: dispatch the `staging-join` scenario of the
  Ceremony E2E CI workflow — a hosted keyless joiner proving the updated
  net still accepts and syncs external joiners.
