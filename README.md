# arduino-esp8266

ESPHome's packaging of the [ESP8266 Arduino core](https://github.com/esp8266/Arduino)
for its native ESP8266 toolchain, so the build depends on nothing from the
PlatformIO registry (esphome/backlog#174).

This repository is a fork of the core. The upstream branches and tags are
left as they are. This branch, `esphome`, holds only the packaging:

- `package.sh` copies a checkout with its submodules the way the core's own
  release script does, rewrites `platform.txt` and `cores/esp8266/core_version.h`
  as that script would, and writes a `.tar.gz` with the contents at the
  archive root, the layout of the PlatformIO package.
- `.github/workflows/release.yml` packages a chosen core ref, checks the
  result against the PlatformIO registry package of the same version, and on
  a manual run publishes it as a release.

## Releases

Releases are `<core version>-esphome.<n>`, for example `3.1.2-esphome.3`;
each release takes the next `n`. Two branches are involved:

- `esphome` (this branch, the default) holds the packaging and the workflow.
- `esphome-3.1.2` is upstream's `3.1.2` tag plus the fixes ESPHome carries.
  Releases are packaged from it.

1. Fixes are pull requests against `esphome-3.1.2`. Its CI runs the core's
   host tests and builds; it must pass.
2. After merging, run the `Package` workflow from `esphome` with `ref` set to
   `esphome-3.1.2` and `tag` set to the next release:

   ```sh
   gh workflow run release.yml --repo esphome-libs/arduino-esp8266 --ref esphome \
     -f ref=esphome-3.1.2 -f tag=3.1.2-esphome.4
   ```

   It checks that the tag matches the version in the core's `platform.txt`,
   packages the branch, tags the packaged commit and publishes
   `arduino-esp8266-<tag>.tar.gz` and `sha256sums.txt` as a release.
3. Check the release: the tag must point at the merged commit
   (`gh api repos/esphome-libs/arduino-esp8266/git/ref/tags/<tag>`), and the
   archive must contain the change.
4. Point ESPHome at it: in `esphome/arduino8266/framework.py` set the
   `FRAMEWORK_RELEASES` entry to the tag, the archive's sha256 (from
   `sha256sums.txt`) and its size in bytes. ESPHome checks both when it
   downloads, so build an ESP8266 config before opening the pull request.

Pull requests and pushes to `esphome` package upstream `3.1.2` as a check
without publishing; only an upstream tag packaged as is gets compared with the
PlatformIO registry package, since a branch carrying fixes differs on purpose.

## Carrying a fix for a new core version

Branch from the upstream tag, for example `esphome-3.1.3` from `3.1.3`, move the
fixes over, and package that branch the same way with `3.1.3-esphome.1`.
Upstream no longer takes ESP8266 fixes, so they are carried here.

## License

`package.sh`, the workflow and the files under `ci/` are licensed under the
Apache License 2.0, see `LICENSE`. The core itself is licensed under the
LGPL 2.1, see its own `LICENSE` in the packaged archive.
