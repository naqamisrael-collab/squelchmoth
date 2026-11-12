#!/usr/bin/env python3
"""Squelchmoth short licence keys (SQM3, 1.13.1).

A key is 20 random Crockford base-32 characters (100 bits), shown as XXXXX-XXXXX-XXXXX-XXXXX.
The app never holds the keys: it ships only a sorted table of 12-byte SHA-256 prefixes of
"squelchmoth3:" + key, each followed by one product byte. Nobody can work a key back out of the table.

Usage: make_short_keys.py <private-out-dir> <table-out.bin> [per-product]
  - writes "<PRODUCT> short keys (N).txt" (one key per line, ready for the store's "From a list" upload)
    and "OWNER short key - keep private.txt" into the private folder - never the repo
  - writes the hash table the app ships (Resources/licence/keys3.bin)
Running it again ADDS new keys to the existing table and lists (old keys keep working).
Keys are never printed.
"""
import hashlib, os, secrets, sys

ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"   # Crockford: no I L O U
PRODUCTS = {1: "CORE", 2: "COMPLETE", 3: "VOCAL", 4: "MASTER", 5: "INSTR", 6: "BEAT", 7: "CREATIVE", 8: "COLOR"}
ENTRY = 13

def new_key():
    raw = "".join(secrets.choice(ALPHABET) for _ in range(20))
    return "-".join(raw[i:i + 5] for i in range(0, 20, 5))

def digest(key):
    norm = key.replace("-", "").upper()
    return hashlib.sha256(("squelchmoth3:" + norm).encode()).digest()[:12]

def main():
    priv, table_path = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 1000
    os.makedirs(priv, exist_ok=True)
    entries = {}
    if os.path.exists(table_path):
        data = open(table_path, "rb").read()
        for i in range(0, len(data), ENTRY):
            entries[data[i:i + 12]] = data[i + 12]
    made = {}
    def add(product):
        while True:
            k = new_key(); h = digest(k)
            if h not in entries:
                entries[h] = product; return k
    owner_file = os.path.join(priv, "OWNER short key - keep private.txt")
    if not os.path.exists(owner_file):
        k = add(2)
        open(owner_file, "w").write(
            "Squelchmoth owner key (unlocks the full program and every pack)\n\n" + k +
            "\n\nHelp > Licences and Upgrades > paste > Activate. Never share or post this key.\n")
    for code, name in PRODUCTS.items():
        made[name] = [add(code) for _ in range(per)]
    for name, keys in made.items():
        n = 1
        while os.path.exists(p := os.path.join(priv, f"{name} short keys ({per})" + (f" batch {n}" if n > 1 else "") + ".txt")):
            n += 1
        open(p, "w").write("\n".join(keys) + "\n")
    os.makedirs(os.path.dirname(table_path), exist_ok=True)
    with open(table_path, "wb") as f:
        for h in sorted(entries):
            f.write(h + bytes([entries[h]]))
    print(f"table: {len(entries)} keys, {len(entries) * ENTRY} bytes; lists written to the private folder")

if __name__ == "__main__":
    main()
