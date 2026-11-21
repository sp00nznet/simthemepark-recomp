# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[SemVer](https://semver.org/).

## [Unreleased]

## [0.1.0] - 2026-11-20

### Changed
- Builds against pcrecomp main; the `pcrecomp-ingame` branch is merged.
- Brought in line with the other recomps: the README in their order (Getting
  Started, Status, Screenshots, Documentation, Contributors, License), the
  build steps and command line moved to `docs/building.md`, `NOTICE` renamed
  `NOTICE.md` and saying what the MIT license covers and what it doesn't.
- Setup and the launcher fetch pcrecomp on `pcrecomp-ingame` (was `pcrecomp-stp`, which lacks buffered synthetic DirectInput).

### Added
- `CONTRIBUTING.md`, `CONTRIBUTORS.md`, `docs/building.md`, `docs/testing.md`.
- `setup.sh`, which says Linux and macOS are not supported yet and stops.
- `.github/workflows/installer.yml`, the recomp-launcher installer for
  Windows, run by hand only (`workflow_dispatch`).

### Added
- First boot of Sim Theme Park (US Gold Edition 2.0) on pcrecomp `native32`:
  `tp.exe` lifted whole, 38,235 functions, 0 lift errors, no IDA.
- The front end runs in recompiled code: the profile screen ("Create New
  Player" x4, Quit Game) over the island fly-by, drawn by the game's own MMX
  software renderer, windowed, at about 7 frames a second (generated C at /Od).
- `--headless` (pcrecomp `native32_headless`), `--shot S FILE.bmp` and
  `--record FILE.mp4` from the presented surface (`native32_on_present`),
  `--click X,Y@S` / `--key VK@S` through pcrecomp's synthetic DirectInput,
  `--mute` (and `--headless` mutes by default: `PCRECOMP_MUTE`).
- The installer's registry values served from `work\registry.reg`
  (`native32_registry`), and a windowed `dialog.tcf` written into the game
  folder when it has none ([docs/bringup.md](docs/bringup.md)).
- `Setup.cmd` (`tools/setup.ps1`), `launcher/launcher.json`, `tools/catalog.py`,
  `docs/bringup.md`.
