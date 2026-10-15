#!/usr/bin/env python3
"""
secp256k1_address.py — ceremony address derivation (A_i) for the e2e harness.

Mirrors GeniusSigner::GetAddress() (SuperGenius/src/account/GeniusSigner.cpp
L94-120): the ceremony address of a private key is hex_lower(X || Y) of k*G on
secp256k1, uncompressed, WITHOUT the 0x04 tag -> exactly 128 lowercase hex
chars. This is the one sanctioned hand-roll (no stdlib secp256k1 exists and
pip is forbidden by D-18); the math below was cross-verified byte-identical
against `openssl pkey` in the phase-2 research session and carries a pinned
self-check vector that runs at import so every harness start fails fast if the
constants or arithmetic ever drift (BOOTSTRAPPER_MISMATCH at ceremony time is
the runtime backstop — a wrong address can never false-pass).

Usage:
  python3 secp256k1_address.py --selftest
"""

import argparse
import os

# Source: verified live cross-check vs `openssl pkey` (DER SPKI, uncompressed point) — MATCH: True
# Mirrors GeniusSigner::GetAddress() (SuperGenius/src/account/GeniusSigner.cpp L94-120):
# address = hex_lower(X || Y) of k*G, uncompressed, WITHOUT the 0x04 tag -> exactly 128 hex chars.
P  = 2**256 - 2**32 - 977
N  = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141
G  = (0x79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798,
     0x483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8)

# ponytail: naive bit-by-bit double-and-add — one modular inversion per point op.
# Ceiling: ~256 inversions per key, irrelevant at ~5 keys per run. If this ever
# drives bulk derivation, upgrade to Jacobian coordinates + windowed mult; the
# pinned vector below keeps the upgrade honest. BOOTSTRAPPER_MISMATCH is the
# runtime backstop: a wrong derivation fails the ceremony loudly, never silently.
def _add(p1, p2):
    if p1 is None: return p2
    if p2 is None: return p1
    (x1, y1), (x2, y2) = p1, p2
    if x1 == x2 and (y1 + y2) % P == 0: return None
    l = (3*x1*x1) * pow(2*y1, -1, P) % P if p1 == p2 else (y2 - y1) * pow(x2 - x1, -1, P) % P
    x3 = (l*l - x1 - x2) % P
    return (x3, (l*(x1 - x3) - y1) % P)

def ceremony_address(priv_hex: str) -> str:          # priv_hex: 64 lowercase hex chars
    k = int(priv_hex, 16)
    if not (0 < k < N): raise ValueError("invalid secp256k1 scalar")
    r, pt = None, G
    while k:
        if k & 1: r = _add(r, pt)
        pt = _add(pt, pt); k >>= 1
    return "%064x%064x" % r                          # 128-hex, no 0x

# Pinned self-check vector (generated + OpenSSL-verified 2026-10-06):
# (continuation '\' added to line 3 — the research snippet as transcribed was
# not parseable; every literal is byte-identical)
assert ceremony_address("b7e151628aed2a6abf7158809cf4f3c7"
                        "62e7160f38b4da56a784d9045190cf3d") == \
       "6c34afa5161d151125c60f3cddb35b9e80ffd6817f2f3f81ccff01054a04bd8" \
       "0880edf60053f5bd37700315cebceb41990484aa2b7d8c9e02a9d5c2ea600cb55"


def generate_key_hex() -> str:
    # GeniusSigner::Generate contract (GeniusSigner.cpp L48-65): random 32
    # bytes, retry until a valid scalar (0 < k < N).
    while True:
        k = int.from_bytes(os.urandom(32), "big")
        if 0 < k < N:
            return "%064x" % k


def _selftest() -> None:
    # The import-time pinned assert above already ran when this module loaded
    # (fail fast at harness start); here exercise the fresh-key properties.
    for _ in range(3):
        key = generate_key_hex()
        assert len(key) == 64 and key == key.lower()
        assert all(c in "0123456789abcdef" for c in key)
        assert 0 < int(key, 16) < N
        address = ceremony_address(key)
        assert len(address) == 128 and address == address.lower()
        assert all(c in "0123456789abcdef" for c in address)
        assert ceremony_address(key) == address  # deterministic
    try:
        ceremony_address("0" * 64)
    except ValueError:
        pass
    else:
        raise AssertionError("zero scalar must be rejected")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Pinned-vector self-check for the ceremony address "
                    "derivation (the vector also asserts at import).")
    parser.add_argument("--selftest", action="store_true",
                        help="run the fresh-key property checks; exit 0 on pass")
    args = parser.parse_args()
    if args.selftest:
        _selftest()
        print("secp256k1_address selftest PASS")
        return
    parser.error("nothing to do; use --selftest")


if __name__ == "__main__":
    main()
