$ErrorActionPreference = 'Stop'

$platformio = (Get-Command pio.exe -ErrorAction SilentlyContinue).Source
if (-not $platformio) {
    $platformio = Join-Path $HOME '.platformio/penv/Scripts/pio.exe'
}
if (-not (Test-Path -LiteralPath $platformio -PathType Leaf)) {
    throw 'Install PlatformIO Core and add pio.exe to PATH, then rerun this script.'
}

$npm = (Get-Command npm.cmd -ErrorAction SilentlyContinue).Source
if (-not $npm) {
    throw 'Install Node.js with npm and add npm.cmd to PATH, then rerun this script.'
}

Push-Location (Join-Path $PSScriptRoot '..')
try {
    & $platformio pkg install
    if ($LASTEXITCODE -ne 0) { throw "PlatformIO dependency installation failed ($LASTEXITCODE)." }

    & $npm --prefix frontend ci --include=dev
    if ($LASTEXITCODE -ne 0) { throw "Frontend dependency installation failed ($LASTEXITCODE)." }

    & $platformio run -t compiledb
    if ($LASTEXITCODE -ne 0) { throw "Compilation database generation failed ($LASTEXITCODE)." }
} finally {
    Pop-Location
}
