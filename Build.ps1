#Requires -Version 5.1
<#
.SYNOPSIS
    Build, size and (optionally) test this AVR library on Windows and Linux.
    Uses PowerShell 5.1+ features only, so the same script runs locally and in CI.
.PARAMETER Mcu
    Target MCU (default: atmega328p). Use -AllMcus to loop over the matrix.
.PARAMETER FCpu
    F_CPU in Hz (default: 8000000).
.PARAMETER AllMcus
    Build for atmega32 and atmega328p.
.PARAMETER DebugBuild
    Build at -O0 in addition to -Os. Catches `inline`-related link errors.
    Named -DebugBuild rather than -Debug because -Debug is a CmdletBinding common parameter.
.PARAMETER Test
    Compile and run host-gcc Unity tests from tests/.
.PARAMETER Clean
    Remove the build/ directory and exit.
.PARAMETER RemoveTools
    After the run, uninstall every tool recorded in the shared manifest.
.PARAMETER NoInstall
    Fail with a clear message when a tool is missing instead of downloading it.
.PARAMETER ToolsDir
    Override the shared tools cache (default: see scripts/Common.psm1).
.PARAMETER CheckSync
    Verify that shared files (src/io_macros.h etc.) are byte-identical with sibling repos under -SharedRoot.
.PARAMETER SharedRoot
    Folder that holds the sibling avr-* repos; required with -CheckSync.
.PARAMETER ConfigHeader
    Force-include this header into every example (used by Simulate.ps1).
.PARAMETER OutDir
    Build output directory (default: build/<mcu>).
#>
[CmdletBinding()]
param(
    [string] $Mcu = 'atmega328p',
    [uint32] $FCpu = 8000000,
    [switch] $AllMcus,
    [switch] $DebugBuild,
    [switch] $Test,
    [switch] $Clean,
    [switch] $RemoveTools,
    [switch] $NoInstall,
    [string] $ToolsDir,
    [switch] $CheckSync,
    [string] $SharedRoot,
    [string] $ConfigHeader,
    [string] $OutDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSCommandPath
Import-Module (Join-Path $RepoRoot 'scripts/Common.psm1') -Force

# Per-repo configuration. Only this block differs between repos.
$RepoName = 'avr-dht'
$DefaultMcu = 'atmega328p'
$McuMatrix = @('atmega32', 'atmega328p')
$ExtraIncludes = @()

# Files that are byte-identical across every sibling avr-* repo.
# -CheckSync -SharedRoot .. compares each of these against every other avr-*
# folder found alongside this one and reports mismatches.
$SharedFiles = @(
    'Simulate.ps1',
    'scripts/Common.psm1',
    '.gitattributes',
    '.gitignore',
    '.github/workflows/ci.yml',
    'src/io_macros.h',
    'tests/fake_avr/avr/io.h',
    'tests/fake_avr/avr/io.c',
    'tests/fake_avr/avr/pgmspace.h',
    'tests/fake_avr/util/delay.h',
    'tests/unity/unity.c',
    'tests/unity/unity.h',
    'tests/unity/unity_internals.h'
)

# Pinned tool versions. SHA256 placeholders left blank until the real
# first install captures them (download will fail loudly on mismatch).
$Tools = @{
    'avr-gcc' = @{
        version = '14.1.0-x64-windows'
        url = 'https://github.com/ZakKemble/avr-gcc-build/releases/download/v14.1.0-1/avr-gcc-14.1.0-x64-windows.zip'
        sha256 = 'd0efbf289004b2d700ae039aa0b592d7d34a9e797e8fe8aa1fef249e997bbae7'
        binRel = 'avr-gcc-14.1.0-x64-windows/bin'
    }
    'w64devkit' = @{
        version = '2.1.0'
        url = 'https://github.com/skeeto/w64devkit/releases/download/v2.1.0/w64devkit-x64-2.1.0.exe'
        sha256 = '3839da025a192e9cacd5caa3ed5150153ab2255800d3672213a5c66e11d6e618'
        binRel = 'w64devkit/bin'
    }
}

if ($Clean) {
    $buildDir = Join-Path $RepoRoot 'build'
    if (Test-Path $buildDir) { Remove-Item -Recurse -Force $buildDir }
    Write-Host 'Cleaned.'
    return
}

if ($CheckSync) {
    if (-not $SharedRoot) { throw '-CheckSync requires -SharedRoot pointing at the folder that holds the sibling avr-* repos.' }
    $mismatches = 0
    foreach ($rel in $SharedFiles) {
        $local = Join-Path $RepoRoot $rel
        if (-not (Test-Path $local)) { continue }
        $localHash = (Get-FileHash $local -Algorithm SHA256).Hash
        foreach ($sibling in Get-ChildItem -Directory $SharedRoot -Filter 'avr-*') {
            $siblingPath = Join-Path $sibling.FullName $rel
            if (-not (Test-Path $siblingPath)) { continue }
            $siblingHash = (Get-FileHash $siblingPath -Algorithm SHA256).Hash
            if ($siblingHash -ne $localHash) {
                Write-Warning "sync: $rel differs between $RepoName and $($sibling.Name)"
                $mismatches++
            }
        }
    }
    if ($mismatches -gt 0) { exit 1 }
    Write-Host 'Sync: shared files match across sibling repos.'
    return
}

$toolsRoot = Get-ToolsRoot -Override $ToolsDir
$installedThisRun = @()

function Resolve-Tool {
    param([string] $Name)
    $onPath = Find-OnPath $Name
    if ($onPath) { return $onPath }
    return $null
}

function Ensure-AvrToolchain {
    foreach ($tool in @('avr-gcc', 'avr-objcopy', 'avr-size')) {
        if (-not (Resolve-Tool $tool)) {
            if ($NoInstall) { throw "Missing $tool and -NoInstall was given. Download: $($Tools['avr-gcc'].url)" }
            $spec = $Tools['avr-gcc']
            $root = Install-PortableZip -Name 'avr-gcc' -Version $spec.version -Url $spec.url -ExpectedSha256 $spec.sha256 -ToolsRoot $toolsRoot
            Add-ToSessionPath (Join-Path $root $spec.binRel)
            $script:installedThisRun += 'avr-gcc'
            if (-not (Resolve-Tool $tool)) { throw "Installation did not put $tool on PATH." }
        }
    }
}

function Ensure-HostGcc {
    if (Resolve-Tool 'gcc') { return }
    if ($NoInstall) { throw "Missing host gcc and -NoInstall was given. Download: $($Tools['w64devkit'].url)" }
    $spec = $Tools['w64devkit']
    $root = Install-SelfExtracting7z -Name 'w64devkit' -Version $spec.version -Url $spec.url -ExpectedSha256 $spec.sha256 -ToolsRoot $toolsRoot
    Add-ToSessionPath (Join-Path $root $spec.binRel)
    $script:installedThisRun += 'w64devkit'
    if (-not (Resolve-Tool 'gcc')) { throw 'Installation did not put gcc on PATH.' }
}

function Compile-Example {
    param(
        [string] $ExampleFile,
        [string] $Mcu,
        [uint32] $FCpu,
        [string] $Optimization,
        [string] $OutputDir
    )
    $exampleName = [System.IO.Path]::GetFileNameWithoutExtension($ExampleFile)
    $elf = Join-Path $OutputDir "$exampleName.elf"
    $hex = Join-Path $OutputDir "$exampleName.hex"

    $sources = @($ExampleFile)
    $srcDir = Join-Path $RepoRoot 'src'
    if (Test-Path $srcDir) {
        $sources += Get-ChildItem -Path $srcDir -Filter '*.c' -Recurse | ForEach-Object FullName
    }

    $includes = @("-I$srcDir") + ($ExtraIncludes | ForEach-Object { "-I$_" })

    $cflags = @(
        "-mmcu=$Mcu",
        "-DF_CPU=${FCpu}UL",
        '-std=gnu99',
        $Optimization,
        '-Wall', '-Wextra', '-Werror',
        '-ffunction-sections', '-fdata-sections',
        '-Wl,--gc-sections'
    )
    # util/delay.h emits #warning when __OPTIMIZE__ isn't defined.
    # The -O0 pass exists only to catch inline/link issues, not to produce usable firmware.
    if ($Optimization -eq '-O0') { $cflags += '-Wno-error=cpp' }
    if ($ConfigHeader) { $cflags += @('-include', $ConfigHeader) }

    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

    & avr-gcc @cflags @includes -o $elf @sources
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $exampleName ($Mcu, $Optimization)" }

    & avr-objcopy -O ihex -R .eeprom $elf $hex
    if ($LASTEXITCODE -ne 0) { throw "objcopy failed: $exampleName" }

    # Use standard berkeley format (works on both stock binutils avr-size and Microchip's flavor).
    # On AVR: flash = text + data; ram = data + bss.
    $sizeOutput = & avr-size --format=berkeley $elf
    $dataRow = $sizeOutput | Where-Object { $_ -match '^\s*\d+\s+\d+\s+\d+\s+\d+' } | Select-Object -First 1
    if ($dataRow -match '^\s*(\d+)\s+(\d+)\s+(\d+)\s+\d+') {
        $text = [int]$Matches[1]; $data = [int]$Matches[2]; $bss = [int]$Matches[3]
        $flashBytes = $text + $data
        $ramBytes = $data + $bss
    }
    else { $flashBytes = 0; $ramBytes = 0 }

    return [pscustomobject]@{ Example = "$exampleName ($Mcu, $Optimization)"; Flash = $flashBytes; RAM = $ramBytes }
}

function Run-Build {
    param([string[]] $Mcus)
    Ensure-AvrToolchain
    $results = @()
    foreach ($m in $Mcus) {
        if ($OutDir) { $outDir = $OutDir } else { $outDir = Join-Path $RepoRoot "build/$m" }
        $examples = Get-ChildItem -Path (Join-Path $RepoRoot 'examples') -Filter '*.c' -ErrorAction SilentlyContinue
        if (-not $examples) { Write-Warning 'No examples found.'; continue }
        foreach ($ex in $examples) {
            $results += Compile-Example -ExampleFile $ex.FullName -Mcu $m -FCpu $FCpu -Optimization '-Os' -OutputDir $outDir
            if ($DebugBuild) {
                $results += Compile-Example -ExampleFile $ex.FullName -Mcu $m -FCpu $FCpu -Optimization '-O0' -OutputDir (Join-Path $outDir 'O0')
            }
        }
    }
    if ($results.Count -gt 0) {
        $table = Format-SizeTable -Rows $results
        $table | ForEach-Object { Write-Host $_ }
        $sizeFile = Join-Path $RepoRoot 'build/size.txt'
        New-Item -ItemType Directory -Force -Path (Split-Path $sizeFile) | Out-Null
        $table | Set-Content $sizeFile -Encoding UTF8
    }
}

function Run-Tests {
    $testsDir = Join-Path $RepoRoot 'tests'
    if (-not (Test-Path $testsDir)) { Write-Warning 'No tests/ directory.'; return }
    Ensure-HostGcc
    $outDir = Join-Path $RepoRoot 'build/host-tests'
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    $fakeDir = Join-Path $testsDir 'fake_avr'
    $unityDir = Join-Path $testsDir 'unity'
    $srcDir = Join-Path $RepoRoot 'src'
    $testFiles = Get-ChildItem -Path $testsDir -Filter 'test_*.c' -File
    if (-not $testFiles) { Write-Warning 'No test_*.c files.'; return }
    # Host tests set a nominal F_CPU so module code that references it compiles.
    # No actual timing happens on the host; _delay_us/_delay_ms are no-ops in fake_avr.
    # -DUNIT_TEST enables test seams in the module code (e.g. twi's wait-function
    # pointer); in production AVR builds those seams compile to zero code/data.
    $cflags = @('-std=gnu99', '-O0', '-g', '-Wall', '-Wextra', '-Werror', "-DF_CPU=${FCpu}UL", '-DUNIT_TEST', "-I$srcDir", "-I$fakeDir", "-I$unityDir")
    $sharedSources = @()
    if (Test-Path $srcDir) { $sharedSources += Get-ChildItem $srcDir -Filter '*.c' -Recurse | ForEach-Object FullName }
    if (Test-Path $unityDir) { $sharedSources += Get-ChildItem $unityDir -Filter '*.c' | ForEach-Object FullName }
    if (Test-Path (Join-Path $fakeDir 'avr/io.c')) { $sharedSources += (Join-Path $fakeDir 'avr/io.c') }
    $exeSuffix = '.exe'
    $failures = 0
    foreach ($t in $testFiles) {
        $binName = [System.IO.Path]::GetFileNameWithoutExtension($t.Name)
        $exe = Join-Path $outDir ($binName + $exeSuffix)
        $sources = @($t.FullName) + $sharedSources
        & gcc @cflags -o $exe @sources
        if ($LASTEXITCODE -ne 0) { Write-Host "Compile failed: $binName"; $failures++; continue }
        & $exe
        if ($LASTEXITCODE -ne 0) { Write-Host "Test failed: $binName"; $failures++ }
    }
    if ($failures -gt 0) { throw "$failures test(s) failed." }
    Write-Host 'All tests passed.'
}

try {
    if ($AllMcus) { $mcus = $McuMatrix } else { $mcus = @($Mcu) }
    Run-Build -Mcus $mcus
    if ($Test) { Run-Tests }
    Write-Host "`nOK."
}
finally {
    if ($RemoveTools) {
        Write-Host "`nRemoving tools installed by this script..."
        Remove-InstalledTools -ToolsRoot $toolsRoot
    }
}
