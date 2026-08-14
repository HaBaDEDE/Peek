[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Exe)

$ErrorActionPreference = "Stop"
$exePath = (Resolve-Path $Exe).Path
$report = Join-Path $env:TEMP "Peek-ci-selftest-$PID.txt"
$peek = $null

try {
    $selfTest = Start-Process $exePath -ArgumentList '--self-test',('"' + $report + '"') -PassThru -Wait
    if ($selfTest.ExitCode -ne 0) { throw "Peek self-test failed with code $($selfTest.ExitCode)." }
    if (-not (Test-Path $report)) { throw "Peek did not write its self-test report." }

    $contents = Get-Content $report
    $contents
    if ($contents -match '^FAIL') { throw "Peek self-test report contains a failure." }

    $peek = Start-Process $exePath -PassThru
    Start-Sleep -Milliseconds 1000
    if ($peek.HasExited) { throw "Peek exited during startup with code $($peek.ExitCode)." }
    Write-Host "Startup smoke test passed (PID $($peek.Id))."
}
finally {
    if ($peek -and -not $peek.HasExited) {
        Stop-Process -Id $peek.Id -Force -ErrorAction SilentlyContinue
        $peek.WaitForExit(5000) | Out-Null
    }
    Remove-Item -LiteralPath $report -Force -ErrorAction SilentlyContinue
}
