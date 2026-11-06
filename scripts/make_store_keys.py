#!/usr/bin/env python3
"""Makes Squelchmoth store keys (SQM2) for the store's per-product key lists. Owner-only: needs the store signing key.

  python3 scripts/make_store_keys.py "/path/to/STORE SIGNING KEY - keep private.txt" PRODUCT COUNT OUT.txt

PRODUCT: CORE | COMPLETE | VOCAL | MASTER | INSTR | BEAT | CREATIVE.  Writes one key per line (97 characters each,
the store takes up to 100), ready to paste or upload as the product's key list. Serials are random and never reused
(a ledger of every serial issued is kept next to the signing key file as 'issued-serials.txt').
Never commit the output: these keys are product.
"""
import sys, os, base64, secrets, re
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

CODES = {"CORE": 1, "COMPLETE": 2, "VOCAL": 3, "MASTER": 4, "INSTR": 5, "BEAT": 6, "CREATIVE": 7}

def main():
    if len(sys.argv) != 5 or sys.argv[2] not in CODES:
        print(__doc__); sys.exit(1)
    keyfile, product, count, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
    m = re.search(r"PRIVATE:\s*([0-9a-f]{64})", open(keyfile).read())
    if not m: sys.exit("no PRIVATE: line in " + keyfile)
    sk = Ed25519PrivateKey.from_private_bytes(bytes.fromhex(m.group(1)))
    ledger = os.path.join(os.path.dirname(os.path.abspath(keyfile)), "issued-serials.txt")
    used = set(open(ledger).read().split()) if os.path.exists(ledger) else set()
    keys, new = [], []
    while len(keys) < count:
        serial = secrets.token_bytes(4)
        if serial.hex().upper() in used: continue
        used.add(serial.hex().upper()); new.append(f"{serial.hex().upper()} {product}")
        payload = bytes([CODES[product]]) + serial
        sig = sk.sign(b"squelchmoth2" + payload)
        k = "SQM2-" + base64.urlsafe_b64encode(payload + sig).decode().rstrip("=")
        assert len(k) <= 100
        keys.append(k)
    open(out, "w").write("\n".join(keys) + "\n")
    with open(ledger, "a") as f: f.write("\n".join(new) + "\n")
    print(f"{count} {product} keys -> {out}")

if __name__ == "__main__":
    main()
