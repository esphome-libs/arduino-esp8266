#!/bin/bash
# Package a checked out ESP8266 Arduino core the way its own release script
# does, with the contents at the archive root like the PlatformIO package.
#
# Usage: package.sh <source dir> <core version> <tag> [output dir]
#   source dir   a checkout of the core with its submodules
#   core version the upstream version the tree is based on, e.g. 3.1.2
#   tag          the release tag, e.g. 3.1.2-esphome.1
set -euo pipefail
export LC_ALL=C

SRC=$(cd "${1:?source dir required}" && pwd)
VERSION=${2:?core version required}
TAG=${3:?tag required}
OUT=$(cd "${4:-.}" && pwd)
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
ARCHIVE=$OUT/arduino-esp8266-$TAG.tar.gz
COMMIT=$(git -C "$SRC" rev-parse HEAD)

# The excludes of package/build_boards_manager_package.sh, at any depth, plus
# the test trees, which the build never reads. Files git ignores are left out
# as that script does; a fresh checkout has none.
cat >"$STAGE/exclude.txt" <<'EOF'
.git
.git-blame-ignore-revs
.github
.gitignore
.gitmodules
ISSUE_TEMPLATE.md
doc
package
/tests
EOF
git -C "$SRC" ls-files --other --directory >>"$STAGE/exclude.txt"
mkdir "$STAGE/pkg"
rsync -a --exclude-from "$STAGE/exclude.txt" "$SRC/" "$STAGE/pkg/"

# platform.txt as the release script rewrites it: tool paths come from the
# board manager instead of the platform directory
sed -e 's/runtime.tools.xtensa-lx106-elf-gcc.path={runtime.platform.path}\/tools\/xtensa-lx106-elf//g' \
  -e 's/runtime.tools.python3.path=.*//g' \
  -e 's/runtime.tools.esptool.path={runtime.platform.path}\/tools\/esptool//g' \
  -e 's/tools.esptool.path={runtime.platform.path}\/tools\/esptool/tools.esptool.path=\{runtime.tools.esptool.path\}/g' \
  -e 's/^tools.esptool.cmd=.*//g' \
  -e 's/^tools.esptool.network_cmd=.*//g' \
  -e 's/^#tools.esptool.cmd=/tools.esptool.cmd=/g' \
  -e 's/^#tools.esptool.network_cmd=/tools.esptool.network_cmd=/g' \
  -e 's/tools.mkspiffs.path={runtime.platform.path}\/tools\/mkspiffs/tools.mkspiffs.path=\{runtime.tools.mkspiffs.path\}/g' \
  -e 's/recipe.hooks.*makecorever.*//g' \
  -e "s/version=.*/version=$VERSION/g" \
  -E -e "s/name=([a-zA-Z0-9\ -]+).*/name=\1($VERSION)/g" \
  "$SRC/platform.txt" >"$STAGE/pkg/platform.txt"

# What tools/makecorever.py writes for a release, without its dependence on
# which tags the clone happens to have
IFS=. read -r MAJOR MINOR REVISION <<<"$VERSION"
cat >"$STAGE/pkg/cores/esp8266/core_version.h" <<EOF
#define ARDUINO_ESP8266_GIT_VER   0x${COMMIT:0:8}
#define ARDUINO_ESP8266_GIT_DESC  $VERSION
#define ARDUINO_ESP8266_VERSION   $VERSION

#define ARDUINO_ESP8266_MAJOR     $MAJOR
#define ARDUINO_ESP8266_MINOR     $MINOR
#define ARDUINO_ESP8266_REVISION  $REVISION

#define ARDUINO_ESP8266_RELEASE   "$VERSION"
#define ARDUINO_ESP8266_RELEASE_${VERSION//./_}
EOF

cat >"$STAGE/pkg/package.json" <<EOF
{
  "name": "arduino-esp8266",
  "version": "$TAG",
  "description": "Arduino core for ESP8266, version $VERSION, packaged by ESPHome",
  "license": "LGPL-2.1-or-later",
  "repository": {
    "type": "git",
    "url": "https://github.com/esphome-libs/arduino-esp8266"
  },
  "commit": "$COMMIT"
}
EOF

# Fixed order, times and owner, so the same tree gives the same archive.
# That needs GNU tar; without it (macOS) the archive is still complete
TAR=$(command -v gtar || echo tar)
if "$TAR" --version 2>/dev/null | grep -q "GNU tar"; then
  "$TAR" -C "$STAGE/pkg" -cf - --sort=name --mtime="@${SOURCE_DATE_EPOCH:-0}" \
    --owner=0 --group=0 --numeric-owner . | gzip -n >"$ARCHIVE"
else
  "$TAR" -C "$STAGE/pkg" -cf - . | gzip -n >"$ARCHIVE"
fi
if command -v sha256sum >/dev/null; then
  (cd "$OUT" && sha256sum "$(basename "$ARCHIVE")" >"$(basename "$ARCHIVE").sha256")
else
  (cd "$OUT" && shasum -a 256 "$(basename "$ARCHIVE")" >"$(basename "$ARCHIVE").sha256")
fi
cat "$ARCHIVE.sha256"
