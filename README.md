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

## Cutting a release

Run the `Package` workflow by hand with `ref` set to the core commit, tag or
branch to package and `tag` set to `<core version>-esphome.<n>`, for example
`ref` `3.1.2` and `tag` `3.1.2-esphome.1`. The release is created on that
commit with `arduino-esp8266-<tag>.tar.gz` and `sha256sums.txt`, which ESPHome
pins in `esphome/arduino8266/framework.py`.

Pull requests and pushes to this branch package `3.1.2` as a check without
publishing.

## Carrying a fix

Branch from the upstream tag, for example `esphome-3.1.2` from `3.1.2`, commit
the fix, and run the workflow with `ref` set to that branch and the next
`-esphome.<n>` tag. Send the fix upstream from the same commit.

## License

`package.sh`, the workflow and the files under `ci/` are licensed under the
Apache License 2.0, see `LICENSE`. The core itself is licensed under the
LGPL 2.1, see its own `LICENSE` in the packaged archive.
