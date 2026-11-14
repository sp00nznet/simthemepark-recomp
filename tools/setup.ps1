# One-click setup for the Sim Theme Park static recompilation.
# Run it by double-clicking Setup.cmd in the repo folder; docs/building.md's
# "Step by step" is the same thing by hand, command for command.
#
# It checks the tools, links your installed copy of the game (US Gold Edition
# 2.0) as game\, catalogs tp.exe, lifts it to C, builds build\simthemepark.exe
# and leaves a shortcut to it in this folder. Each step is
# skipped when its output already exists (-Force redoes them), so a rerun after
# a failure resumes. Everything it does goes to setup.log.
#
#   -GameDir <folder with tp.exe and data>   skip the question (-Game works too)
#   -Yes                                     answer yes to every install question
#
# The toolkit is ..\pcrecomp on main (PCRECOMP
# overrides it). Under recomp-launcher (RECOMP_LAUNCHER=1) it never waits for
# Enter, and exit code 0 means the shortcut is there.
param([switch]$Force, [switch]$Yes, [Alias('Game')][string]$GameDir = "")
$Game = $GameDir

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Toolkit = $env:PCRECOMP
if (-not $Toolkit) {
  $Toolkit = Join-Path (Split-Path -Parent $Root) 'pcrecomp'
}
$env:PCRECOMP = $Toolkit   # the Python steps read it too
$Log = Join-Path $Root 'setup.log'
Set-Location $Root
function Log($t) { Add-Content -Path $Log -Value $t -Encoding UTF8 }
Log "==== setup $(Get-Date -Format s)"

function Say($t, $c = 'Gray') { Write-Host $t -ForegroundColor $c; Log $t }
function Step($n, $t) { Write-Host ""; Say "[$n/5] $t" 'Cyan' }
function Pause-End { if (-not $Yes -and $env:RECOMP_LAUNCHER -ne '1') { Read-Host "Press Enter to close" | Out-Null } }
function Fail($t) {
  Say ""; Say "Setup stopped: $t" 'Red'
  Say "The details are in $Log. Fix that and run Setup.cmd again; finished steps are skipped." 'Yellow'
  Pause-End; exit 1
}
trap { Fail "$_" }
function Ask($q) { if ($Yes) { Say "$q yes"; return $true }; $a = Read-Host "$q [Y/n]"; return -not ($a -match '^[nN]') }
function Refresh-Path {
  $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
}
# Run a program with its output in the log. Windows PowerShell turns a native
# program's stderr into errors, so 'Stop' is off while it runs.
function Exec([string[]]$cmd) {
  $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
  $rest = @($cmd | Select-Object -Skip 1)   # @(): a one-element slice would splat as characters
  & $cmd[0] @rest 2>&1 | ForEach-Object { Log "$_" }
  $code = $LASTEXITCODE
  $ErrorActionPreference = $old
  return $code
}
function Run($what, [string[]]$cmd) {
  Say "  $what..."
  $code = Exec $cmd
  if ($code -ne 0) { Fail "$what failed (exit code $code)." }
}

if ($env:RECOMP_LAUNCHER -ne '1') { Clear-Host }
Say "Sim Theme Park: static recompilation setup" 'White'
Say "You need your own installed copy of Sim Theme Park (see the README), and about 3 GB free."

# ---------------------------------------------------------------- tools
Step 1 "Checking the tools"
# A Python that answers "Python 3.x". The Store's placeholder "python" (the
# one that opens the Store) answers nothing, so asking is the reliable test.
function Find-Python {
  # The per-user install winget makes, before PATH has it.
  $user = @(Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | ForEach-Object { ,@($_.FullName) })
  foreach ($c in (@(@('py', '-3'), @('python'), @('python3')) + $user)) {
    if (-not (Get-Command $c[0] -ErrorAction SilentlyContinue)) { continue }
    $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $rest = @($c | Select-Object -Skip 1)
    $v = (& $c[0] @rest --version 2>&1 | Out-String).Trim()
    $ErrorActionPreference = $old
    if ($v -match '^Python 3\.(\d+)' -and [int]$Matches[1] -ge 10) { return ,$c }
  }
  return $null
}
$pyargs = Find-Python
if (-not $pyargs) {
  Say "  Python 3.10 or newer is not installed."
  if (-not (Ask "  Install Python 3.12 now (winget, for your user only, about 30 MB)?")) { Fail "Python 3 is required." }
  if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Fail "winget is missing. Install 'App Installer' from the Microsoft Store, or install Python yourself."
  }
  Exec @('winget', 'install', '-e', '--id', 'Python.Python.3.12', '--scope', 'user', '--accept-package-agreements', '--accept-source-agreements') | Out-Null
  Refresh-Path
  $pyargs = Find-Python
  if (-not $pyargs) { Fail "Python installed, but it can't be found. Install Python 3.12 from python.org yourself." }
}
Say "  Python: $($pyargs -join ' ')"

if ((Exec ($pyargs + @('-c', 'import pefile, capstone'))) -ne 0) {
  Say "  The Python packages pefile and capstone are missing (about 20 MB)."
  if (-not (Ask "  Install them now (pip, for your user only)?")) { Fail "pefile and capstone are required." }
  Run "Installing pefile and capstone" ($pyargs + @('-m', 'pip', 'install', '--user', 'pefile', 'capstone'))
}

if (-not (Test-Path (Join-Path $Toolkit 'tools\lift\lift32.py'))) {
  Say "  The pcrecomp toolkit is not beside this folder ($Toolkit)."
  if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    if (-not (Ask "  git is needed to fetch it. Install Git for Windows now (winget, about 60 MB)?")) { Fail "git is needed to fetch pcrecomp." }
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) { Fail "winget is missing. Install Git for Windows yourself." }
    Exec @('winget', 'install', '-e', '--id', 'Git.Git', '--accept-package-agreements', '--accept-source-agreements') | Out-Null
    Refresh-Path
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Fail "Git did not install. Install Git for Windows yourself." }
  }
  if (-not (Ask "  Download it now (git, about 20 MB)?")) { Fail "pcrecomp is required at $Toolkit." }
  Run "Cloning pcrecomp" @('git', 'clone', '--depth', '1', 'https://github.com/sp00nznet/pcrecomp', $Toolkit)
}
# The toolkit changes this game needs (all on main),
# each found by a line it adds.
$need = @(@('the headless desktop', 'runtime\native32\native32.c', 'native32_headless'),
          @('the shared module loader', 'runtime\native32\modules32.c', 'native32_modules_map'),
          @('the guest registry', 'runtime\native32\reg32.c', 'native32_registry'),
          @('synthetic DirectInput', 'runtime\native32\dinput32.c', 'native32_input_mouse'),
          @('the DirectDraw fixes', 'runtime\native32\native32.c', 'native32_on_present'),
          @('the shared lift driver', 'tools\lift\drive32.py', 'lift_image'),
          @('buffered synthetic DirectInput', 'runtime\native32\dinput32.c', 'g_di_ring'))
function Missing { @($need | Where-Object {
  $f = Join-Path $Toolkit $_[1]
  -not ((Test-Path $f) -and (Select-String -Path $f -Pattern $_[2] -SimpleMatch -Quiet)) } | ForEach-Object { $_[0] }) }
$missing = Missing
if ($missing) {
  Fail "your pcrecomp at $Toolkit is missing $($missing -join ', '). Update it to main (git -C `"$Toolkit`" checkout main; git -C `"$Toolkit`" pull), or point PCRECOMP at a checkout that has it."
}
Say "  pcrecomp: $Toolkit"

# Visual Studio 2022 or its Build Tools, with the C++ x86 compiler; CMake and
# Ninja ship inside that workload, so build.cmd needs nothing else.
function Find-VS {
  foreach ($p in @("${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
                   "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe")) {
    if (Test-Path $p) {
      $vs = & $p -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
      if ($vs) { return $vs }
    }
  }
  foreach ($d in (Get-ChildItem "$env:ProgramFiles\Microsoft Visual Studio\2022", "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022" -Directory -ErrorAction SilentlyContinue)) {
    if (Test-Path "$($d.FullName)\VC\Auxiliary\Build\vcvarsall.bat") { return $d.FullName }
  }
  return $null
}
if (-not (Find-VS)) {
  Say "  Visual Studio 2022 with the C++ tools is not installed. The recompiled game is C, and this is the compiler."
  if (-not (Ask "  Install the Visual Studio 2022 Build Tools with the C++ workload now (winget, about 3 GB)?")) { Fail "a C compiler (Visual Studio 2022 C++) is required." }
  if (-not (Get-Command winget -ErrorAction SilentlyContinue)) { Fail "winget is missing. Install Visual Studio 2022 Build Tools with 'Desktop development with C++' yourself." }
  Exec @('winget', 'install', '-e', '--id', 'Microsoft.VisualStudio.2022.BuildTools', '--accept-package-agreements', '--accept-source-agreements',
         '--override', '--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended') | Out-Null
  if (-not (Find-VS)) { Fail "the Build Tools did not install. Install Visual Studio 2022 with 'Desktop development with C++' yourself." }
}
Say "  C compiler: $(Find-VS)"

# ---------------------------------------------------------------- the game
# The game is linked, not copied: game\ is a junction to your install.
Step 2 "Linking your copy of the game as game\"
function Is-Stp($d) { (Test-Path (Join-Path $d 'tp.exe')) -and (Test-Path (Join-Path $d 'data\sound.sam')) }
if ((Is-Stp 'game') -and -not $Force) {
  Say "  Already there (skipping)."
} else {
  $Game = $Game.Trim('"', ' ')
  if (-not $Game) {
    foreach ($d in @("${env:ProgramFiles(x86)}\SimTheme Park", "$env:ProgramFiles\SimTheme Park", 'C:\Games\Sim Theme Park', 'C:\SimTheme Park')) {
      if (Is-Stp $d) { $Game = $d; Say "  Found the game in $Game"; break }
    }
  }
  if (-not $Game -and $Yes) { Fail "Sim Theme Park wasn't found. Give its folder with -GameDir." }
  while (-not $Game) {
    $Game = (Read-Host "  Where is Sim Theme Park? The folder with tp.exe and data").Trim('"', ' ')
  }
  if (-not (Is-Stp $Game)) { Fail "$Game has no tp.exe and data\sound.sam. Point it at the installed game folder." }
  foreach ($w in @('DDraw.dll', 'D3DImm.dll')) {
    if (Test-Path (Join-Path $Game $w)) {
      Say "  Note: $Game has a $w (a wrapper such as dgVoodoo). The recompiled game uses Windows' own DirectDraw; move it aside if the game misbehaves." 'Yellow'
    }
  }
  if (Test-Path 'game') { Run "Removing the old game link" @('cmd', '/d', '/c', 'rmdir', 'game') }
  Run "Linking game -> $Game" @('cmd', '/d', '/c', 'mklink', '/J', 'game', $Game)
}

# ---------------------------------------------------------------- catalog
Step 3 "Finding every function (about 3 minutes)"
if ((Test-Path 'work\tp.functions.json') -and -not $Force) { Say "  Already done (skipping)." }
else { Run "Disassembling tp.exe" ($pyargs + @('tools\catalog.py', '--force')) }

# ---------------------------------------------------------------- lift
Step 4 "Recompiling to C, then building build\simthemepark.exe (several minutes)"
if ((Test-Path 'src\recomp\gen\recomp_dispatch.c') -and -not $Force) { Say "  Lift already done (skipping)." }
else { Run "Lifting" ($pyargs + @('run_lift.py')) }
if ((Test-Path 'build\simthemepark.exe') -and -not $Force) { Say "  Already built (skipping)." }
else {
  $env:CMAKE_ARGS = "-DPCRECOMP=$($Toolkit -replace '\\', '/')"
  Run "Compiling" @('cmd', '/d', '/c', "`"$Root\build.cmd`"")
}

# ---------------------------------------------------------------- shortcut
Step 5 "Making the shortcut"
$lnk = Join-Path $Root 'Sim Theme Park.lnk'
$sh = New-Object -ComObject WScript.Shell
$s = $sh.CreateShortcut($lnk)
$s.TargetPath = Join-Path $Root 'build\simthemepark.exe'
$s.Arguments = '--run'
$s.WorkingDirectory = $Root
$s.IconLocation = (Join-Path $Root 'game\tp.exe') + ',0'
$s.Save()
Say "  $lnk"

if (-not (Test-Path $lnk)) { Fail "the shortcut was not made." }
Write-Host ""
Say "Done. Double-click 'Sim Theme Park' in this folder to play." 'Green'
Pause-End
exit 0
