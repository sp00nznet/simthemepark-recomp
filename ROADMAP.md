# Roadmap

## Next
- **Play a park**: build rides and paths through the park's toolbar; Full Simulation; saving and loading.
- **The sky**: vertical blue stripes over the sea in the fly-by; check the
  sky's span routine in `cseg` against the original.
- **/O2 draws no 3D** (white background, menus fine): a difference between the
  optimized and unoptimized generated C, so something in the lift depends on
  undefined behavior. /Od is the default and what the farm builds.
- **A conformance harness** (REPO_RULES section 9): dry-run bind count, first
  presented frame, the profile screen, each checked from `--shot`.
- **A farm build**: `netlab build simthemepark-recomp` (recipe added, not run).

## Later
- **Linux and macOS**: a Wine route (clang-cl or mingw cross-build, as in
  redalert2-recomp's `setup.sh --wine`), tried and tested before `setup.sh` offers it.
- Full screen (needs a real desktop, not `--headless`), Direct3D hardware
  renderers, the 3DNow! paths (lifted as no-ops today).
- Online features (`wea*.dll`, the Bullfrog servers are long gone).
