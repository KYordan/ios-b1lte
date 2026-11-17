# Troubleshooting

## XNU does not start
The boot menu comes back when XNU fails to start ([boot-menu.md](boot-menu.md)). Boot TWRP and read the kernel log kept on SYSTEM:

```bash
python3 scripts/pull-b2l-log.py --serial "$B2L_SERIAL" --output out/log.txt
python3 scripts/decode-b2l-diag.py   # decodes the diagnostic records; see --help
```

Both are read-only: they never write the phone.

## iOS starts but a daemon or app misbehaves
Export the unified logs from the HFS+ volume (read-only, works on a dirty volume):

```bash
python3 scripts/pull-b2l-unified-logs.py --help
```

To read them on a Mac with `log show`, build the `Info.plist` the archive lacks from the timesync data with `tools/b1l/logs/mkinfo.py`.

## A new GPU driver seems to be ignored
When bumping the Metal set, rename **both** the `b2l-metal-vN` folder and the `B2LMetalvN` class/bundle name. If only one changes, the previously loaded driver stays in use.

## Wi-Fi joins, then apps lose the network after about 60 s
`wifid` gives up when an `apple80211` request it needs is refused. Check `b2l_wifid_log` output and the request numbers in the kernel log; see [journal/build-b1l-209.md](journal/build-b1l-209.md) and the other Wi-Fi reports in [journal/](journal).

## Build or flash interrupted
`tools/b1l/watch-build.sh` shows which stage failed (`$S10_PORT/progress/NNNN.status`). Flashing is block-based and every block is read back, so rerunning the same command is safe: only differing blocks are written again.

## Host tests
`scripts/test-loader.sh`, `scripts/test-macho.sh` and `scripts/test-ufs-transport.sh` run on the Mac alone. `scripts/test-native-cache.sh`, `scripts/test-arm64e.sh` and part of `tests/*.py` need the prepared XNU tree, the dyld cache or build outputs; without them they fail.

TODO: add the known panics and their fixes.
