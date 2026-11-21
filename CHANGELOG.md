# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/). Build-by-build history: [docs/journal](docs/journal).

## [Unreleased]

### Added
- Galaxy S10e (`beyond0lte`, SM-G970F, 6 GiB) board profile: `B2L_BOARD=beyond0lte` selects its partitions, RAM ceiling, UEFI platform and 1080 × 2280 display ([docs/device-facts-beyond0lte.md](docs/device-facts-beyond0lte.md)).
- `tools/b1l/board.sh`, shared by the flash tools; `tests/test_board_profiles.py`.
- First public layout of the Galaxy S10 port, imported from `beyond2lte-ios` (branch `b1l`, build B1L-0273).
- `tools/b1l` (build/flash loop), `tools/flash-kit`, `userspace/audio-plugin`.
- `docs/architecture.md`, `docs/installation.md`, `docs/troubleshooting.md`, `THIRD_PARTY_NOTICES.md`, `.env.example`.

### Fixed
- `tools/flash-kit` scripts: unset `$adb`, assignment to bash's read-only `UID`, block paths compared without `/dev/block/`, vbmeta read from the wrong partition, BOOT hash compared on a truncated length.
- `tests/test_prepare_boot_firmware.py` fixture lacked `RamManager.c` and the `// DDR Regions` marker.
- The XNU loader refused GOPs narrower than 1184 px; the boot menu overlapped its note on panels shorter than 2600 px.

### Changed
- Build reports moved to `docs/journal/` (kept in French); status and plan documents translated to English under `docs/gpu`, `docs/audio`, `docs/bluetooth`, `docs/plans` and `docs/project`.
- Scripts read `S10_PORT`, `B2L_ROOT`, `ADB`, `B2L_SERIAL` and `PY_ENV` instead of fixed paths.

### Removed
- The phone's serial number, the home network name and personal paths from scripts and documents.
