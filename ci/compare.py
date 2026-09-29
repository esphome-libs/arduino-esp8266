"""Compare our archive with the PlatformIO registry package it replaces.

Every registry file must be in ours with the same content, except
package.json, which each publisher writes. Files only in ours are listed.

Usage: compare.py <registry.tar.gz> <ours.tar.gz>
"""

import hashlib
import sys
import tarfile

IGNORED = {"package.json"}


def digests(path: str) -> dict[str, str]:
    result = {}
    with tarfile.open(path) as archive:
        for member in archive:
            if not member.isfile():
                continue
            name = member.name.removeprefix("./")
            with archive.extractfile(member) as data:
                result[name] = hashlib.file_digest(data, "sha256").hexdigest()
    return result


def main() -> int:
    registry = digests(sys.argv[1])
    ours = digests(sys.argv[2])
    missing = sorted(name for name in registry if name not in ours)
    changed = sorted(
        name
        for name, digest in registry.items()
        if name in ours and ours[name] != digest and name not in IGNORED
    )
    extra = sorted(name for name in ours if name not in registry)
    print(f"registry files: {len(registry)}, ours: {len(ours)}")
    for label, names in (("missing", missing), ("changed", changed), ("only in ours", extra)):
        print(f"{label}: {len(names)}")
        for name in names[:40]:
            print(f"  {name}")
    return 1 if missing or changed else 0


if __name__ == "__main__":
    sys.exit(main())
