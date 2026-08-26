<#
  .SYNOPSIS
    Closes the WSL/PowerShell/CLion windows opened by glbp-startdev.ps1.
  #>

  Import-Module "C:\Users\Ryan\source\repos\psmodules\devkit.psm1" -Force

  $repoRoot = Split-Path -Parent $PSScriptRoot

  Stop-DevSession -RepoRoot $repoRoot