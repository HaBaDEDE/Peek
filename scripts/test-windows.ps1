[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Exe)

$ErrorActionPreference = "Stop"
$exePath = (Resolve-Path $Exe).Path
$report = Join-Path $env:TEMP "Peek-selftest.txt"
if (Test-Path $report) { Remove-Item -LiteralPath $report -Force }
$selfTest = Start-Process $exePath -ArgumentList '--self-test',('"' + $report + '"') -PassThru -Wait
if (-not (Test-Path $report)) { throw "Peek did not write its self-test report." }
Get-Content $report
if ($selfTest.ExitCode -ne 0) { throw "Peek self-test failed with code $($selfTest.ExitCode)." }

$peek = Start-Process $exePath -PassThru
Start-Sleep -Milliseconds 800
if ($peek.HasExited) { throw "Peek exited during startup with code $($peek.ExitCode)." }

$short = Start-Process powershell.exe -ArgumentList '-NoProfile','-Command','Start-Sleep -Milliseconds 120' -PassThru
$short.WaitForExit()

$lockPath = Join-Path $env:TEMP "peek-unlock-test.txt"
Set-Content -Path $lockPath -Value "Peek Unlock acceptance test"
$holder = Start-Process powershell.exe -ArgumentList '-NoProfile','-Command',"`$f=[IO.File]::Open('$lockPath','Open','Read','None'); Start-Sleep -Seconds 120; `$f.Dispose()" -PassThru

Write-Host "Peek PID: $($peek.Id)"
Write-Host "Short process generated: $($short.Id)"
Write-Host "Lock holder PID: $($holder.Id)"
Write-Host "Locked file: $lockPath"
Write-Host "Use Ctrl+Shift+G, then test Unlock with the locked file."
Write-Host "Stop the lock holder with: Stop-Process -Id $($holder.Id)"
