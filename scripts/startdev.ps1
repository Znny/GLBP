<#
  .SYNOPSIS
    Opens the glbp dev environment: a WSL shell running Claude, a plain WSL
    shell, a PowerShell window, and CLion, all rooted in the repo - and
    records their process ids so stopdev.ps1 can close them together later.
  #>

  $ErrorActionPreference = "Stop"

  Import-Module "C:\Users\Ryan\source\repos\psmodules\devkit.psm1" -Force

  $repoRoot = Split-Path -Parent $PSScriptRoot

  $driveLetter = $repoRoot.Substring(0,1).ToLower()
  $wslCD = "/mnt/$driveLetter" + ($repoRoot.Substring(2) -replace '\\', '/')

  $claudeProc = Start-Process wsl.exe -ArgumentList "-d", "Ubuntu-20.04", "--cd", $wslCD, "--", "bash", "-ilc", "srt
  claude" -PassThru
  Write-Host "Started claude (PID $($claudeProc.Id))"

  $wslShellProc = Start-Process wsl.exe -ArgumentList "-d", "Ubuntu-20.04", "--cd", $wslCD -PassThru
  Write-Host "Started wsl shell (PID $($wslShellProc.Id))"

  $psProc = Start-Process powershell.exe -ArgumentList "-NoExit", "-Command", "`$host.UI.RawUI.WindowTitle = 'powershell';
  cd `"$repoRoot`"" -PassThru
  Write-Host "Started powershell (PID $($psProc.Id))"

  $clion = Find-SystemProgram -Name "clion64" -FallbackPaths @(
      "$env:LOCALAPPDATA\Programs\CLion\bin\clion64.exe",
      "C:\Program Files\JetBrains\CLion*\bin\clion64.exe"
  )
  $clionProc = $null
  if ($clion) {
      $clionProc = Start-Process -FilePath $clion -ArgumentList "`"$repoRoot`"" -PassThru
      Write-Host "Started clion (PID $($clionProc.Id))"
  } else {
      Write-Host "Could not locate CLion - open $repoRoot manually" -ForegroundColor Yellow
  }

  $trackedPids = @{
      claude     = $claudeProc.Id
      wslshell   = $wslShellProc.Id
      powershell = $psProc.Id
  }
  if ($clionProc) { $trackedPids["clion"] = $clionProc.Id }

  $pidFile = Join-Path $repoRoot ".dev-pids.session.json"
  @{ pids = $trackedPids; ports = @{} } | ConvertTo-Json | Set-Content -Path $pidFile

  Write-Host ""
  Write-Host "Session state: $pidFile"
  Write-Host "Run 'scripts\glbp-stopdev.ps1' to close this session."