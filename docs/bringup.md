# Bring-up notes

What Sim Theme Park needed, in the order it turned up. Each item says where
the fix lives: in this repository's host (`src/runtime/host.c`), or in
pcrecomp (branch `pcrecomp-stp`, now inside `pcrecomp-ingame`), when another game would hit it too.

## The copy of the game

The copy used here is the **US Gold Edition (v2.0)**, from an Inno Setup
installer that `innoextract` unpacks without running anything (`app/` is the
game folder). Its `tp.exe` was unwrapped from SafeDisc before it got here (the string
`unwrapped with unSafedisc` sits in its DOS stub); that changes nothing for
the lift, but it is why the PE timestamp says 2022.

That copy also carried dgVoodoo (`DDraw.dll`, `D3DImm.dll`, `dgVoodoo.conf`,
`dgVoodooCpl.exe`). None of it is in this repository, and it was moved
aside: the host loads Windows' own DirectDraw,
and a `DDraw.dll` in the game folder would win the DLL search for anything
that loads DirectDraw by name. `Setup.cmd` warns when it finds one.

The last official patch is `tppatchtwoeuroamer20000324a.exe` (an InstallShield PackageForTheWeb
holding RTPatch files dated March 2000). The Gold Edition shipped after it and
already contains it, and RTPatch files can only be applied by running the
patcher, so nothing was applied.

## Lifting

One module. `tp.exe` is based at `0x00400000` with no relocations; its DLLs
(`QMixer.dll`, the `wea*.dll` online modules, `usp11.dll`, `dplayerx.dll`) are
third-party and load natively. MSVC C++ with RTTI (51 classes, 167 virtual
methods named by `rtti.py`), statically linked CRT.

- **Two code sections.** `.text` and `cseg`, the hand-written MMX software
  renderer ("MMX Software Renderer, by Martin Griffiths"). pcrecomp's shared
  `drive32.lift_image` read only `.text`'s raw bytes, so everything in `cseg`
  lifted as empty bodies. Fixed in pcrecomp: it now reads every code section
  as mapped.
- **Code pointer tables in code.** `cseg` keeps its span routines in a table
  inside the section (`0x006F19B0`: nine routines) and reaches them through
  `jmp [table + eax*4]`. Eight were never named by a call, and the first
  frame stopped at `ITAIL: unresolved VA 0x006F6090`. Fixed in pcrecomp:
  disasm32 now also takes aligned runs of code pointers in code sections that
  no decoded instruction covers.
- Result: 38,235 functions in the dispatch table, 0 lift errors. 210
  instructions are lifted as no-ops: 3DNow! (`pfmul`, `pfadd`, `pfacc`,
  `pfrcp`, `femms`, 172 in all) in the AMD paths, which the game only takes
  when CPUID reports 3DNow!, and a handful of `int`/`cli`/BCD opcodes in data
  that decodes as code.

## The registry

The installer wrote `HKLM\Software\Bullfrog Productions Ltd\Theme Park World`.
The game creates that key with `RegCreateKeyExA` (a step at a time:
`software\Bullfrog Productions Ltd`, then `Theme Park World` under the
handle) and reads `Language`. Without it, it quits silently.

`Language` is a DWORD LANGID, not a name: the reader passes a 4-byte buffer
(`ERROR_MORE_DATA` for the string "American") and the game's default is
`0x809`. US English is `0x409`, which selects `data\Language\American`.

Rather than write HKLM (administrator rights, or the VirtualStore when the
process has no manifest, which is where the first run's empty key went),
pcrecomp now has `native32_registry(file)`: the subtrees a regedit-4 file
names are served from that file. The host writes `work\registry.reg` with
`Language` on first run.

## DirectDraw

`fallback.txt` and `enginedebug.txt` in the game folder are the engine's own
logs, and they say why it stopped.

1. `GetDeviceIdentifier(DDGDI_GETHOSTIDENTIFIER)` on `IDirectDraw4` answers
   `DDERR_NOTFOUND` (`887600ff`, "Requested item was not found"). On a private
   desktop it fails without the flag too. pcrecomp's built-in
   `DirectDrawCreate` shim now asks again without the flag and then answers a
   vendorless identifier.
2. Full screen fails on `native32_headless`'s private desktop (`80004001`,
   "Action not supported"), so the game has to run windowed. Its windowed mode
   is bit 0 of the engine flags (`0x7A1A8C`, default `0xC0E15`, bit 0 = full
   screen). The flags come from `dialog.tcf` in the game folder when it
   exists, which the original launcher wrote. Its layout (`sub_00424560`):

   | Offset | Field | Value the host writes |
   |---|---|---|
   | 0 | version (at least 2) | 2 |
   | 4 | engine flags (`0x7A1A8C`) | `0xC0E14`: the default without full screen |
   | 8 | start-up flags (`0x79D86C`) | `0x4002`: `0x4000` starts the front end; without it the game loads a saved park named by bits `0x800`/`0x1000`/`0x2000` and fails with "Unable to load game (data\)" |
   | 12 | unused here | 0 |
   | 16 | `CURRENTTheme` | 0 |
   | 20 | `DEBUGGuardUInts` | 0 |
   | 24 | `TextureCacheSize` (MB, 0 = default) | 0 |

   The host writes this file when the game folder has none.
3. Windowed, the primary's back buffer is asked for in video memory, and the
   private desktop has no HAL (`88760233`, `DDERR_NOHAL`). pcrecomp's shim
   now makes such a surface again in system memory.
4. The surface patches did not stick at first: Windows' own DirectDraw
   compatibility shims hook `CreateSurface` again on every `QueryInterface`.
   pcrecomp now patches `QueryInterface` too and re-applies the hooks to what
   it returns.

The game picks renderer 4, its own software renderer, which draws into
DirectDraw surfaces and needs nothing from Direct3D on this desktop.

A side effect to know about: after a run that failed to find a 16-bit mode,
Windows' Program Compatibility Assistant put `DWM8And16BitMitigation` on
`build\simthemepark.exe` (`HKCU\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers`).
The 16-bit relaunch is now opt-in (`--16bit`); the game runs windowed at the
desktop's depth.

## DirectSound

The sound thread calls `IDirectSound::SetCooperativeLevel(GetForegroundWindow(), DSSCL_PRIORITY)`.
On the private desktop nothing is in the foreground, `GetForegroundWindow`
answers NULL, DirectSound answers `E_INVALIDARG`, the game releases the
object and later calls through it (a read of address 0 in `sub_006C4300`).
pcrecomp's built-in `GetForegroundWindow` now answers the process's own
window under `native32_headless`.

## Screenshots

On the private desktop a GDI copy of the window is black. pcrecomp's
`native32_on_present` hands the host the surface the game blits to its window;
`--shot` and `--record` read that.

## Input

The game reads the mouse and keyboard through DirectInput (`GetDeviceState`
on the system mouse and keyboard; it imports no `GetCursorPos`). On the
private desktop the devices are never acquired (`SetCooperativeLevel` with an
invalid window answers `E_HANDLE`, `Acquire` `E_ACCESSDENIED`, `GetDeviceState`
`DIERR_NOTACQUIRED`). pcrecomp's synthetic DirectInput serves both devices
from the host under `native32_headless`, and `--click X,Y@S` moves the mouse
there (relative: first to the top-left corner, then by X,Y) and clicks.

The front end reads both devices buffered (`GetDeviceData`), not with
`GetDeviceState` (four calls in a run), and the synthetic devices answered
no buffered events. pcrecomp (`pcrecomp-ingame`) now queues every synthetic
move, button and key as a `DIDEVICEOBJECTDATA` event per device, and the
front end, the name box and the park answer. The mouse is 1:1 in screen
pixels.

## Sound

Test runs were loud. `--headless` now sets `PCRECOMP_MUTE=1`, and pcrecomp's
`native32_init` then sets the process's audio session volume to 0
(`waveOutSetVolume(NULL, 0)`; DirectSound and QMixer share that session).
Windows remembers a program's session volume, so a later normal run is silent
too until one runs with `PCRECOMP_MUTE=0`, which sets it back to full.

## Speed and /O2

At `/Od` the front end runs at about 7 frames a second (1,216 frames in a
166-second recording). An `/O2` build (over 30 minutes to compile, three
chunks take most of it) draws the menus but no 3D scene: a white background.
Something in the lifted code behaves differently once optimized; not chased
yet.
