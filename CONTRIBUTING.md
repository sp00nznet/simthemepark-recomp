# Contributing

Issues and pull requests are welcome.

## The one rule

**Nothing from the game goes in the repository.** No game files, no lifted C
(`src/recomp/gen/`), no catalog dumps (`work/`), no disassembly listings,
nothing generated from `tp.exe`. The tool ships; its output is made on each
person's machine from their own copy. `.gitignore` covers the usual paths;
check `git status` before you commit anyway. Screenshots of the game running
are fine.

## Where your code comes from

Write it yourself, from the game's own behavior, Microsoft's documentation and
pcrecomp. Don't bring in code from GPL or LGPL projects (Wine, ReactOS,
DOSBox, ScummVM) or from wrappers such as dgVoodoo or nGlide, and don't write
code with them open. Permissively licensed code is fine with its notice kept:
add a row to [NOTICE.md](NOTICE.md) and its license text under `LICENSES/`.

## Before a pull request

1. Build from a clean lift: `py -3 run_lift.py` (expected: `errors 0`), then `build.cmd`.
2. Check it:
   `build\simthemepark.exe` (the dry run: `0 unresolved`), then the scripted
   run into a park in [docs/testing.md](docs/testing.md), which must still
   open the park. A new behavior comes with a scripted run that shows it.
3. A wall you got past goes in the docs: what it looked like, what it was,
   what moved it.

## Where a fix belongs

- A bug in the lifter, the catalog or the `native32` runtime belongs in
  [pcrecomp](https://github.com/sp00nznet/pcrecomp), not here.
- A fix to the game itself is a patch in `run_lift.py` or a shim in
  `src/runtime/`, with a comment that says what it fixes and why.
