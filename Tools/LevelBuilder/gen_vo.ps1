# Generate radio squad-callout VO via Windows SAPI (built-in TTS, no external tool).
# Autonomous + CC0 (our own). Outputs WAVs to Tools/incoming for the import pipeline.
# Run: powershell -File Tools/LevelBuilder/gen_vo.ps1
$dir = Join-Path $PSScriptRoot "..\incoming"
if (-not (Test-Path $dir)) { New-Item -ItemType Directory $dir | Out-Null }
Add-Type -AssemblyName System.Speech
$s = New-Object System.Speech.Synthesis.SpeechSynthesizer
try { $s.SelectVoice("Microsoft David Desktop") } catch {}
$s.Rate = 2
$lines = @{
    "sh_vo_contact"    = "Contact front!"
    "sh_vo_reloading"  = "Reloading, cover me!"
    "sh_vo_mandown"    = "Man down, man down!"
    "sh_vo_movingup"   = "Moving up, on me!"
    "sh_vo_fragout"    = "Frag out!"
    "sh_vo_clear"      = "Area clear!"
    "sh_vo_takingfire" = "Taking fire, get down!"
    "sh_vo_enemydown"  = "Enemy down!"
}
foreach ($k in $lines.Keys) {
    $s.SetOutputToWaveFile((Join-Path $dir "$k.wav"))
    $s.Speak($lines[$k])
}
$s.Dispose()
Write-Host "generated $($lines.Count) VO lines into $dir"
