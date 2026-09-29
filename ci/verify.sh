#!/bin/bash
# Check that an archive has what ESPHome's native build reads and carries
# the expected version.
# Usage: verify.sh <archive> <core version>
set -euo pipefail

ARCHIVE=${1:?archive required}
VERSION=${2:?core version required}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

tar -xzf "$ARCHIVE" -C "$WORK"
for path in cores/esp8266/Arduino.h tools/sdk/include tools/sdk/lib/libhal.a \
  tools/sdk/lib/liblwip2-1460-feat.a tools/sdk/lib/NONOSDK22x_190703/libmain.a \
  tools/sdk/ld/eagle.app.v6.common.ld.h tools/sdk/ld/eagle.flash.4m2m.ld \
  tools/sdk/lwip2/include variants/d1_mini libraries/ESP8266WiFi/src \
  libraries/ESP8266mDNS/src tools/elf2bin.py bootloaders/eboot/eboot.elf \
  platform.txt package.json; do
  test -e "$WORK/$path" || { echo "missing $path"; exit 1; }
done
for absent in .git .github tests doc package .gitmodules; do
  test ! -e "$WORK/$absent" || { echo "$absent must not be in the package"; exit 1; }
done
grep -q "^#define ARDUINO_ESP8266_RELEASE   \"$VERSION\"$" "$WORK/cores/esp8266/core_version.h"
grep -q "^#define ARDUINO_ESP8266_RELEASE_${VERSION//./_}$" "$WORK/cores/esp8266/core_version.h"
grep -q "^version=$VERSION$" "$WORK/platform.txt"
echo "layout ok, $(find "$WORK" -type f | wc -l | tr -d ' ') files"
cat "$WORK/cores/esp8266/core_version.h"
