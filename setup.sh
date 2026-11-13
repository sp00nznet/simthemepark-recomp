#!/bin/sh
# Linux and macOS: not yet. The host is built on pcrecomp's native32 runtime,
# which runs the lifted game against the real Windows DLLs in a 32-bit Windows
# process; there is no native Linux or macOS build of it, and the Wine route
# (as in redalert2-recomp's setup.sh --wine) has not been tried for this game.
# Rather than a script that fails halfway, this one says so and stops.
echo "Sim Theme Park: the recompiled game builds and runs on Windows only for now."
echo "On Windows, double-click Setup.cmd. Linux and macOS are on the ROADMAP."
exit 1
