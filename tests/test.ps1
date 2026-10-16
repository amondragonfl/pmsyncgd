# Smoke tests for pmsyncgd. Run after build.bat:
#     powershell -ExecutionPolicy Bypass -File tests\test.ps1 [-GameDir "<Geometry Dash folder>"]
#
#  1. live mode against tests\fake_gd.c (a pretend GeometryDash.exe)
#  2. install -> status -> uninstall on a temp COPY of the real GeometryDash.exe
#     (your actual game is never touched)
param(
    [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$tool = Join-Path $root "dist\pmsyncgd.exe"
$failed = 0

function Check($name, $ok) {
    if ($ok) { Write-Host "  PASS  $name" -ForegroundColor Green }
    else { Write-Host "  FAIL  $name" -ForegroundColor Red; $script:failed++ }
}

if (-not (Test-Path $tool)) { throw "build first: $tool not found" }

# --- 1. live mode ---------------------------------------------------------------
if (Get-Process GeometryDash -ErrorAction SilentlyContinue) {
    Write-Host "  SKIP  live test (close Geometry Dash first)" -ForegroundColor Yellow
} else {
    $fakeDir = Join-Path $root "build\test"
    New-Item -ItemType Directory -Force $fakeDir | Out-Null
    $fake = Join-Path $fakeDir "GeometryDash.exe"
    & gcc -O0 (Join-Path $PSScriptRoot "fake_gd.c") -o $fake
    if ($LASTEXITCODE -ne 0) { throw "couldn't build fake_gd.c" }

    $p = Start-Process -FilePath $fake -PassThru -NoNewWindow
    $null = $p.Handle   # PowerShell only keeps ExitCode if the handle was opened early
    & $tool live --once --fast | Out-Null
    $liveExit = $LASTEXITCODE
    $p.WaitForExit(30000) | Out-Null
    Check "live mode patches a running game" ($liveExit -eq 0)
    Check "patched code behaves (and the decoy is untouched)" ($p.ExitCode -eq 0)
}

# --- 2. file round trip ---------------------------------------------------------
$real = Join-Path $GameDir "GeometryDash.exe"
if (-not (Test-Path $real)) {
    Write-Host "  SKIP  file test (no GeometryDash.exe in $GameDir)" -ForegroundColor Yellow
} else {
    $tmp = Join-Path ([IO.Path]::GetTempPath()) ("pmsyncgd-test-" + [guid]::NewGuid())
    New-Item -ItemType Directory $tmp | Out-Null
    try {
        $copy = Join-Path $tmp "GeometryDash.exe"
        Copy-Item $real $copy
        $before = (Get-FileHash $copy).Hash

        & $tool install --path $tmp --fast | Out-Null
        Check "install succeeds" ($LASTEXITCODE -eq 0)
        Check "install changes the file" ((Get-FileHash $copy).Hash -ne $before)
        Check "install leaves a backup" (Test-Path "$copy.pmsyncgd-backup")
        Check "status reports installed" ((& $tool status --path $tmp --fast) -match "installed .*\(5/5")

        & $tool install --path $tmp --fast | Out-Null
        Check "second install is a no-op" ($LASTEXITCODE -eq 0)

        & $tool uninstall --path $tmp --fast | Out-Null
        Check "uninstall succeeds" ($LASTEXITCODE -eq 0)
        Check "uninstall restores the exact original" ((Get-FileHash $copy).Hash -eq $before)
        Check "uninstall tidies the backup" (-not (Test-Path "$copy.pmsyncgd-backup"))
    } finally {
        Remove-Item -Recurse -Force $tmp
    }
}

if ($failed) { Write-Host "`n  $failed test(s) failed" -ForegroundColor Red; exit 1 }
Write-Host "`n  all good (^o^)/" -ForegroundColor Green
