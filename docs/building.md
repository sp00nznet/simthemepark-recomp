# Building and the command line

`Setup.cmd` (`tools/setup.ps1`) runs exactly the steps below, skipping the ones
already done, and logs everything to `setup.log`. It clones pcrecomp on the
**`pcrecomp-ingame`** branch if it is not beside this folder; that branch
carries the toolkit work this game needs (`pcrecomp-stp`, plus buffered
synthetic DirectInput) and is not merged into `main` yet.

Linux and macOS: not yet. The host runs the lifted game inside a 32-bit
Windows process against the real Windows DLLs (pcrecomp `native32`), and the
Wine route has not been tried; `setup.sh` says so and stops.

## Step by step

Prerequisites: Windows 10/11, **Python 3.10+** (`py -3 --version`), **git**,
**Visual Studio 2022** (or its Build Tools) with *Desktop development with
C++*, ffmpeg on `PATH` for `--record` only, and pcrecomp cloned **beside**
this repository on the `pcrecomp-ingame` branch:

```
some-folder\
  pcrecomp\              <- git clone -b pcrecomp-ingame https://github.com/sp00nznet/pcrecomp
  simthemepark-recomp\   <- this repository
```

1. `py -3 -m pip install --user pefile capstone`
2. `mklink /J game "C:\Program Files (x86)\SimTheme Park"`
3. `py -3 tools\catalog.py` (about 3 minutes). Expected:
   `[*] Functions: 38235 ...`
4. `py -3 run_lift.py`, then `build.cmd`. Expected from the lift:
   `38235 functions in the dispatch table, lift errors 0`.
5. `build\simthemepark.exe`. Expected:
   `[bind] 0x00400000: ... 0 unresolved`, then
   `(dry run: mapped and bound; --run enters 0x0067EE10)`.
6. `build\simthemepark.exe --run`.

The usual trip-ups: `python` opening the Microsoft Store (use `py -3`); a new
Python not on `PATH` until a new window; a pcrecomp on `main` instead of
`pcrecomp-ingame`. The first `--run` writes `dialog.tcf` (windowed) into the game
folder and `work\registry.reg`.

## The command line

```
build\simthemepark.exe --run
build\simthemepark.exe --headless --run --watchdog 180 --record run.mp4 ^
    --shot 100 menu.bmp --click 414,36@100
```

| Flag | |
|---|---|
| `--run` | enter the game (without it: map, bind and stop) |
| `--headless` | a private desktop that is never shown (works over RDP); muted unless `PCRECOMP_MUTE=0` |
| `--mute` | mute the audio session (`PCRECOMP_MUTE=1`; Windows remembers it per program, `PCRECOMP_MUTE=0` restores it) |
| `--shot S FILE.bmp` | the presented frame at S seconds (up to 16) |
| `--record FILE.mp4` | every presented frame to ffmpeg |
| `--click X,Y@S`, `--key VK@S` | input at S seconds, through pcrecomp's synthetic DirectInput and posted messages |
| `--watchdog S` | stop after S seconds and say where the game was |
| `--16bit` | relaunch under Windows' 16-bit color compatibility layer |
| `--game DIR` | the game folder (default `game`) |
| `--native-trace`, `--callbacks` | one line per call into Windows, or back from it |

## Build options

`PCRECOMP` (Python) and `-DPCRECOMP=` (CMake, through
`set CMAKE_ARGS=...`) point at another toolkit checkout. The generated C
builds at `/Od` (about a minute); `-DPCRECOMP_GEN_OPT=ON` builds it at `/O2`
(over 30 minutes, and today that build draws no 3D). On the farm:
`netlab build simthemepark-recomp`. What the game needed, in order, is
[docs/bringup.md](docs/bringup.md).
