# =============================================================================
#  Shattered Horizon 2032 — Game Eval Suite
#  One command -> a pass/fail scorecard across the testing tiers:
#    1. Unit tests        (52 game-logic automation tests, headless)
#    2. Smoke playtest    (boots, spawns, equips, fires, mission loads, no crash)
#    3. Behavioral checks (combat runs, ambience plays, no sound errors)
#    4. Performance        (avg FPS >= floor)
#  Exit code 0 = GREEN (all pass), 1 = RED (any fail). Scorecard written to
#  Tools/LevelBuilder/output/eval_scorecard.txt.
#
#  Run:  powershell -File Tools/run_evals.ps1
#        (CI/regression gate — run after any change to confirm it still plays.)
# =============================================================================
param([int]$SmokeRows = 7, [int]$SmokeTimeoutSec = 240, [double]$FpsFloor = 4.0)
$ErrorActionPreference = "Continue"

$root   = "C:\Users\moder\OneDrive\Desktop\ShatteredHorizon2032-2"
$edcmd  = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$ed     = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
$proj   = "$root\ShatteredHorizon2032.uproject"
$logs   = "$root\Saved\Logs"
$out    = "$root\Tools\LevelBuilder\output\eval_scorecard.txt"

$results = New-Object System.Collections.ArrayList
$script:pass = 0; $script:fail = 0
function Assert([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { $script:pass++; [void]$results.Add(("PASS  {0,-20} {1}" -f $name, $detail)) }
    else     { $script:fail++; [void]$results.Add(("FAIL  {0,-20} {1}" -f $name, $detail)) }
}
function KillEditors {
    Get-Process -Name UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep 2
}

Write-Host "[evals] starting - this takes a few minutes (two headless UE runs)..."
KillEditors

# ---- Tier 1: unit tests --------------------------------------------------------
$tl = "$logs\EvalTests.log"
Write-Host "[evals] (1/2) running 52 unit tests..."
Start-Process -FilePath $edcmd -ArgumentList @(
    "`"$proj`"", "-ExecCmds=`"Automation RunTests SH2032; Quit`"",
    "-unattended", "-nopause", "-nullrhi", "-nosplash",
    "-log=EvalTests.log", "-TestExit=`"Automation Test Queue Empty`"") -Wait
$usPass = if (Test-Path $tl) { (Select-String -Path $tl -Pattern 'Result={Success}' -SimpleMatch).Count } else { 0 }
$usFail = if (Test-Path $tl) { (Select-String -Path $tl -Pattern 'Result={Fail}' -SimpleMatch).Count } else { 0 }
Assert "unit_tests" (($usFail -eq 0) -and ($usPass -ge 40)) "$usPass passed / $usFail failed"

# ---- Tier 2-4: smoke playtest + perf ------------------------------------------
Write-Host "[evals] (2/2) running smoke playtest..."
if (Test-Path "$logs\perf_report.csv") { Remove-Item "$logs\perf_report.csv" -Force -ErrorAction SilentlyContinue }
$sl = "$logs\EvalSmoke.log"
$smoke = Start-Process -FilePath $ed -ArgumentList @(
    "`"$proj`"", "-game", "-windowed", "-resx=1280", "-resy=720", "-d3d11", "-nosplash",
    "-log", "LOG=EvalSmoke.log",
    "-ini:Game:[/Script/ShatteredHorizon2032.SHGameMode]:bPerfReport=True",
    "-ini:Game:[/Script/ShatteredHorizon2032.SHGameMode]:PerfReportInterval=5",
    "-ini:Game:[/Script/ShatteredHorizon2032.SHPlayerCharacter]:bAutoPlaytest=True") -PassThru

$elapsed = 0
while ($elapsed -lt $SmokeTimeoutSec) {
    Start-Sleep 5; $elapsed += 5
    $rows = 0
    if (Test-Path "$logs\perf_report.csv") { $rows = (Get-Content "$logs\perf_report.csv" | Measure-Object -Line).Lines }
    if ($rows -ge $SmokeRows) { break }
    if ((Test-Path $sl) -and (Select-String -Path $sl -Pattern 'Fatal error', 'appError' -Quiet)) { break }
}
Stop-Process -Id $smoke.Id -Force -ErrorAction SilentlyContinue
KillEditors

function LogHas([string]$pat)   { if (Test-Path $sl) { return [bool](Select-String -Path $sl -Pattern $pat -SimpleMatch -Quiet) } return $false }
function LogCount([string]$pat) { if (Test-Path $sl) { return (Select-String -Path $sl -Pattern $pat -SimpleMatch).Count } return 0 }

Assert "boot_input_live"  (LogHas 'INPUT OK')                     "input mapping context added"
Assert "weapon_equipped"  (LogHas 'Equipped weapon')             "loadout applied on spawn"
Assert "mission_loaded"   (LogHas 'Loaded code-authored mission') "M01 mission + phases"
Assert "ambience_playing" (LogHas 'default ambience beds')        "ambient soundscape active"
$fires = LogCount 'firing burst'
Assert "combat_runs"      ($fires -ge 1)                          "$fires player fire bursts"
$fatals = (LogCount 'Fatal error') + (LogCount 'appError')
Assert "no_crashes"       ($fatals -eq 0)                          "$fatals fatal errors"
$snderr = if (Test-Path $sl) { (Select-String -Path $sl -Pattern 'Failed to load.*Sound').Count } else { 1 }
Assert "no_sound_errors"  ($snderr -eq 0)                          "$snderr sound load failures"

$avg = 0.0
if (Test-Path "$logs\perf_report.csv") {
    $csv = Import-Csv "$logs\perf_report.csv"
    if ($csv) { $avg = [double]($csv | Measure-Object -Property avg_fps -Average).Average }
}
Assert "perf_floor" ($avg -ge $FpsFloor) ("avg {0:N1} fps (floor {1})" -f $avg, $FpsFloor)

# ---- Scorecard ----------------------------------------------------------------
$sb = @()
$sb += "==================== SH2032 EVAL SCORECARD ===================="
$sb += (Get-Date).ToString("u")
$sb += "--------------------------------------------------------------"
$sb += $results
$sb += "--------------------------------------------------------------"
$p = $script:pass
$f = $script:fail
$sb += "TOTAL: $p passed, $f failed"
if ($f -eq 0) { $sb += "RESULT: GREEN (all checks passed)" } else { $sb += "RESULT: RED (regression detected)" }
$sb | Tee-Object -FilePath $out
if ($f -eq 0) { exit 0 } else { exit 1 }
