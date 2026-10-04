#Requires -Version 5.1
<#
.SYNOPSIS
    Build the simulator demo and open it in SimulIDE (Windows only).
.PARAMETER Example
    Example name (default: demo).
.PARAMETER NoBuild
    Skip the rebuild; use the existing build/sim/<example>.hex.
.PARAMETER RemoveTools
    After SimulIDE exits, remove tools installed by this script run.
.PARAMETER NoInstall
    Fail with a clear message when SimulIDE is missing instead of downloading it.
.PARAMETER NoLaunch
    Build and install everything but do not launch SimulIDE. Useful for CI
    smoke-tests and for verifying the toolchain install path without blocking
    on the GUI.
.PARAMETER ToolsDir
    Override the shared tools cache (default: see scripts/Common.psm1).
#>
[CmdletBinding()]
param(
    [string] $Example = 'demo',
    [switch] $NoBuild,
    [switch] $RemoveTools,
    [switch] $NoInstall,
    [switch] $NoLaunch,
    [string] $ToolsDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSCommandPath
Import-Module (Join-Path $RepoRoot 'scripts/Common.psm1') -Force

$SimulIde = @{
    version = '1.1.0-SR0-Win64'
    url = 'https://launchpad.net/simulide/1.1.0/1.1.0-sr0/+download/SimulIDE_1.1.0-SR0_Win64.zip'
    sha256 = '7e497904de4616b85a83ed70fb1bdc35bbda171b4d8216e627b0072a6d9ee1d9'
    exeRel = 'SimulIDE_1.1.0-SR0_Win64/simulide.exe'
}

$toolsRoot = Get-ToolsRoot -Override $ToolsDir

try {
    $simConfig = Join-Path $RepoRoot 'sim/sim_config.h'
    $buildArgs = @{ Mcu = 'atmega328p'; OutDir = (Join-Path $RepoRoot 'build/sim') }
    if (Test-Path $simConfig) { $buildArgs['ConfigHeader'] = $simConfig }

    if (-not $NoBuild) {
        & (Join-Path $RepoRoot 'Build.ps1') @buildArgs
        if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    }

    $hex = Join-Path $RepoRoot "build/sim/$Example.hex"
    if (-not (Test-Path $hex)) { throw "Hex not found: $hex" }

    $exe = Find-OnPath 'simulide'
    if (-not $exe) {
        if ($NoInstall) { throw "simulide not on PATH and -NoInstall was given. Download: $($SimulIde.url)" }
        $root = Install-PortableZip -Name 'simulide' -Version $SimulIde.version -Url $SimulIde.url -ExpectedSha256 $SimulIde.sha256 -ToolsRoot $toolsRoot
        $exe = Join-Path $root $SimulIde.exeRel
    }

    $circuit = Join-Path $RepoRoot "sim/$Example.sim1"
    if ($NoLaunch) {
        Write-Host "NoLaunch: hex at $hex, simulide at $exe$(if (Test-Path $circuit) { ", circuit at $circuit" } else { ', no circuit file yet' })."
    }
    else {
        if (Test-Path $circuit) {
            $launchArgs = @($circuit)
            Write-Host "Launching SimulIDE with $circuit. Close the SimulIDE window to continue."
        }
        else {
            $launchArgs = @()
            Write-Warning "No sim/$Example.sim1 found. SimulIDE is opening; load $hex into an ATmega328P manually."
        }
        # Start-Process -Wait blocks until the GUI closes. Plain `& $exe` returns
        # immediately on Windows because the OS launches GUI apps detached from
        # the console, which makes -RemoveTools fire before the user is done.
        Start-Process -FilePath $exe -ArgumentList $launchArgs -Wait
    }
}
finally {
    if ($RemoveTools) {
        Write-Host "`nRemoving tools installed by this script..."
        Remove-InstalledTools -ToolsRoot $toolsRoot
    }
}
