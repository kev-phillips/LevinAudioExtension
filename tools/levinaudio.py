#!/usr/bin/env python3
"""Make, inspect and unpack .levinaudio files.

A .levinaudio file is a normal XM or MOD module plus metadata (named channel
groups, start volumes, cues, tags) that LevinPlayer reads automatically.
Plain .xm/.mod files keep working as before.

  python3 tools/levinaudio.py pack song.xm song.json -o song.levinaudio
  python3 tools/levinaudio.py info song.levinaudio
  python3 tools/levinaudio.py unpack song.levinaudio -o out_dir

Metadata JSON (every key optional):

  {
    "title": "Whispers in the Attic",
    "tags": ["level", "spooky"],
    "channels": { "drums": [1, 2], "bass": [3], "drama": [5, 6] },
    "start_volumes": { "drama": 0.0 },
    "cues": [ { "id": "chorus", "order": 4, "row": 0 } ]
  }

Channels are numbered from 1, as in the Lua API. See levinplayer/include/levinaudio.h
for the byte layout.
"""
import argparse
import json
import os
import struct
import sys

MAGIC = b"LVNA"
VERSION = 1
MODULE_IDS = {".xm": b"XM  ", ".mod": b"MOD "}


def validate_metadata(meta):
    """Returns a list of problems; empty means the metadata looks right."""
    problems = []
    if not isinstance(meta, dict):
        return ["metadata must be a JSON object"]
    channels = meta.get("channels", {})
    if not isinstance(channels, dict):
        problems.append('"channels" must map group names to channel lists')
        channels = {}
    for name, chans in channels.items():
        if not isinstance(chans, list) or not all(isinstance(c, int) and c >= 1 for c in chans):
            problems.append(f'channel group "{name}" must be a list of channel numbers starting at 1')
    for name, vol in (meta.get("start_volumes") or {}).items():
        if name not in channels:
            problems.append(f'start_volumes names "{name}", which is not a channel group')
        if not isinstance(vol, (int, float)) or not 0 <= vol <= 1:
            problems.append(f'start volume for "{name}" must be between 0 and 1')
    for cue in meta.get("cues") or []:
        if not isinstance(cue, dict) or not isinstance(cue.get("order"), int):
            problems.append('each cue needs an "order" number (and usually "id" and "row")')
    if "tags" in meta and not isinstance(meta["tags"], list):
        problems.append('"tags" must be a list')
    return problems


def pack(module_path, meta, edit_bytes=None):
    ext = os.path.splitext(module_path)[1].lower()
    if ext not in MODULE_IDS:
        raise ValueError(f"module must be .xm or .mod, not '{ext}'")
    with open(module_path, "rb") as f:
        module = f.read()
    out = bytearray(MAGIC + struct.pack("<HH", VERSION, 0))
    chunks = []
    if meta is not None:
        chunks.append((b"META", json.dumps(meta, ensure_ascii=False, separators=(",", ":")).encode("utf-8")))
    chunks.append((MODULE_IDS[ext], module))
    if edit_bytes:
        chunks.append((b"EDIT", edit_bytes))
    for cid, body in chunks:
        out += cid + struct.pack("<I", len(body)) + body
    return bytes(out)


def read_chunks(data):
    if data[:4] != MAGIC:
        raise ValueError("not a .levinaudio file")
    version, _flags = struct.unpack_from("<HH", data, 4)
    if version == 0 or version > VERSION:
        raise ValueError(f"unsupported .levinaudio version {version}")
    pos, chunks = 8, []
    while pos < len(data):
        if len(data) - pos < 8:
            raise ValueError("truncated chunk header")
        cid = data[pos:pos + 4]
        (size,) = struct.unpack_from("<I", data, pos + 4)
        pos += 8
        if size > len(data) - pos:
            raise ValueError(f"chunk {cid!r} runs past the end of the file")
        chunks.append((cid, data[pos:pos + size]))
        pos += size
    return version, chunks


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("pack", help="wrap an XM/MOD and a metadata JSON into a .levinaudio file")
    p.add_argument("module")
    p.add_argument("metadata", nargs="?", help="metadata JSON file (optional)")
    p.add_argument("-o", "--output")
    p.add_argument("--edit", help="optional editor project data to embed (ignored by the player)")
    i = sub.add_parser("info", help="show what's inside a .levinaudio file")
    i.add_argument("file")
    u = sub.add_parser("unpack", help="extract the module and metadata")
    u.add_argument("file")
    u.add_argument("-o", "--output", default=".")
    args = ap.parse_args(argv)

    if args.cmd == "pack":
        meta = None
        if args.metadata:
            with open(args.metadata, encoding="utf-8") as f:
                meta = json.load(f)
            problems = validate_metadata(meta)
            if problems:
                for prob in problems:
                    print("error:", prob, file=sys.stderr)
                return 1
        edit = open(args.edit, "rb").read() if args.edit else None
        out = args.output or os.path.splitext(args.module)[0] + ".levinaudio"
        data = pack(args.module, meta, edit)
        with open(out, "wb") as f:
            f.write(data)
        print(f"wrote {out} ({len(data)} bytes, module {os.path.getsize(args.module)} bytes)")
        return 0

    data = open(args.file, "rb").read()
    version, chunks = read_chunks(data)
    if args.cmd == "info":
        print(f"{args.file}: .levinaudio v{version}, {len(data)} bytes")
        for cid, body in chunks:
            print(f"  {cid.decode('latin-1')!r:8} {len(body):>9} bytes")
            if cid == b"META":
                print("    " + json.dumps(json.loads(body), indent=2).replace("\n", "\n    "))
        return 0

    os.makedirs(args.output, exist_ok=True)
    base = os.path.splitext(os.path.basename(args.file))[0]
    for cid, body in chunks:
        name = {b"META": ".json", b"XM  ": ".xm", b"MOD ": ".mod", b"EDIT": ".edit"}.get(cid)
        if name:
            path = os.path.join(args.output, base + name)
            with open(path, "wb") as f:
                f.write(body)
            print("wrote", path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
