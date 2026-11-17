# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/). Build-by-build history: [docs/journal](docs/journal).

## [Unreleased]

### Added
- First public layout of the Galaxy S10 port, imported from `beyond2lte-ios` (branch `b1l`, build B1L-0273).
- `tools/b1l` (build/flash loop), `tools/flash-kit`, `userspace/audio-plugin`.
- `docs/architecture.md`, `docs/installation.md`, `docs/troubleshooting.md`, `THIRD_PARTY_NOTICES.md`, `.env.example`.

### Changed
- Build reports moved to `docs/journal/` (kept in French); status and plan documents translated to English under `docs/gpu`, `docs/audio`, `docs/bluetooth`, `docs/plans` and `docs/project`.
- Scripts read `S10_PORT`, `B2L_ROOT`, `ADB`, `B2L_SERIAL` and `PY_ENV` instead of fixed paths.

### Removed
- The phone's serial number, the home network name and personal paths from scripts and documents.
