#!/bin/sh
# dev loop: build, then one headless run. tools/br.sh NAME [host args...]
cd "$(dirname "$0")/.." || exit 1
cmd.exe //c "G:\recomp\pc\simthemepark\build.cmd > build.log 2>&1" || { grep -E " error |error LNK" build.log | head; exit 1; }
n=$1; shift
PCRECOMP_MUTE=1 timeout 300 build/simthemepark.exe --headless --run "$@" > work/$n.log 2>&1; echo "rc=$?"
