#
# Common.psm1 — shared helpers used by Build.ps1 and Simulate.ps1 across every
# avr-* repo. Byte-identical copy per repo; Build.ps1 -CheckSync verifies it.
#
# Design rules (see plan §9.3):
#   - No admin rights, no system-wide PATH changes.
#   - Portable zip or self-extracting archive tools, extracted to a shared cache.
#   - Pinned version + SHA256 for every download.
#   - A manifest (tools/installed.json) records what this script installed.
#   - -RemoveTools only deletes entries from the manifest.
#

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-ToolsRoot {
    param([string] $Override)
    if ($Override) { return $Override }
    return (Join-Path $env:LOCALAPPDATA 'avr-libs\tools')
}

function ConvertTo-HashtableFromJson {
    param($Object)
    if ($null -eq $Object) { return @{} }
    $result = @{}
    foreach ($p in $Object.PSObject.Properties) {
        $value = $p.Value
        if ($value -is [pscustomobject]) { $value = ConvertTo-HashtableFromJson $value }
        $result[$p.Name] = $value
    }
    return $result
}

function Get-Manifest {
    param([string] $ToolsRoot)
    $path = Join-Path $ToolsRoot 'installed.json'
    if (-not (Test-Path $path)) { return @{} }
    $raw = Get-Content -Raw $path | ConvertFrom-Json
    return ConvertTo-HashtableFromJson $raw
}

function Save-Manifest {
    param([string] $ToolsRoot, [hashtable] $Manifest)
    $path = Join-Path $ToolsRoot 'installed.json'
    $Manifest | ConvertTo-Json -Depth 6 | Set-Content -Path $path -Encoding UTF8
}

function Find-OnPath {
    param([string] $Name)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

function Test-Sha256 {
    param([string] $Path, [string] $Expected)
    $actual = (Get-FileHash -Path $Path -Algorithm SHA256).Hash
    return ($actual -ieq $Expected)
}

function Test-ZipMagic {
    param([string] $Path)
    $fs = [System.IO.File]::OpenRead($Path)
    try {
        $bytes = New-Object byte[] 4
        $read = $fs.Read($bytes, 0, 4)
        if ($read -lt 4) { return $false }
        return ($bytes[0] -eq 0x50 -and $bytes[1] -eq 0x4B -and ($bytes[2] -eq 0x03 -or $bytes[2] -eq 0x05 -or $bytes[2] -eq 0x07))
    }
    finally { $fs.Close() }
}

function Invoke-ToolDownload {
    param(
        [string] $Url,
        [string] $Destination,
        [string] $ExpectedSha256,
        [string] $ExpectedKind = 'zip'
    )
    $tmp = [System.IO.Path]::GetTempFileName()
    try {
        Write-Host "  Downloading $Url"
        $prev = $ProgressPreference
        $ProgressPreference = 'SilentlyContinue'
        try { Invoke-WebRequest -Uri $Url -OutFile $tmp -UseBasicParsing -MaximumRedirection 5 }
        finally { $ProgressPreference = $prev }
        # Verify the response is really an archive, not an HTML error page.
        # Launchpad returns 200 with an HTML "page not found" when a release is pulled.
        if ($ExpectedKind -eq 'zip' -and -not (Test-ZipMagic -Path $tmp)) {
            throw "Download from $Url did not return a zip. Likely a stale URL or an upstream redirect. Check the URL in the script."
        }
        if ($ExpectedSha256 -and -not (Test-Sha256 -Path $tmp -Expected $ExpectedSha256)) {
            $actual = (Get-FileHash -Path $tmp -Algorithm SHA256).Hash
            throw "SHA256 mismatch for $Url (got $actual, expected $ExpectedSha256)"
        }
        Move-Item -Force -Path $tmp -Destination $Destination
    }
    finally {
        if (Test-Path $tmp) { Remove-Item -Force $tmp }
    }
}

function New-ToolManifestEntry {
    param([string] $ToolsRoot, [string] $Name, [string] $Version, [string] $Path)
    $manifest = Get-Manifest -ToolsRoot $ToolsRoot
    $manifest[$Name] = @{ version = $Version; path = $Path; installedAt = (Get-Date).ToString('o') }
    Save-Manifest -ToolsRoot $ToolsRoot -Manifest $manifest
}

function Install-PortableZip {
    param([string] $Name, [string] $Version, [string] $Url, [string] $ExpectedSha256, [string] $ToolsRoot)
    New-Item -ItemType Directory -Force -Path $ToolsRoot | Out-Null
    $extractDir = Join-Path $ToolsRoot "$Name-$Version"
    if (Test-Path $extractDir) { return $extractDir }

    Write-Host "Installing $Name $Version to $extractDir"
    $stagingParent = Join-Path $ToolsRoot ('.staging-' + [guid]::NewGuid())
    New-Item -ItemType Directory -Force -Path $stagingParent | Out-Null
    try {
        $zip = Join-Path $stagingParent "$Name.zip"
        Invoke-ToolDownload -Url $Url -Destination $zip -ExpectedSha256 $ExpectedSha256
        Expand-Archive -Path $zip -DestinationPath $extractDir -Force
        Remove-Item $zip
    }
    finally {
        if (Test-Path $stagingParent) { Remove-Item -Recurse -Force $stagingParent }
    }

    New-ToolManifestEntry -ToolsRoot $ToolsRoot -Name $Name -Version $Version -Path $extractDir
    return $extractDir
}

function Install-SelfExtracting7z {
    # For installers like w64devkit that ship as a 7z self-extracting .exe.
    # Runs the exe with -y -o<dir> which the makeself/7z stub honors silently.
    param([string] $Name, [string] $Version, [string] $Url, [string] $ExpectedSha256, [string] $ToolsRoot)
    New-Item -ItemType Directory -Force -Path $ToolsRoot | Out-Null
    $extractDir = Join-Path $ToolsRoot "$Name-$Version"
    if (Test-Path $extractDir) { return $extractDir }

    Write-Host "Installing $Name $Version to $extractDir"
    $stagingParent = Join-Path $ToolsRoot ('.staging-' + [guid]::NewGuid())
    New-Item -ItemType Directory -Force -Path $stagingParent | Out-Null
    try {
        $installer = Join-Path $stagingParent "$Name.exe"
        Invoke-ToolDownload -Url $Url -Destination $installer -ExpectedSha256 $ExpectedSha256 -ExpectedKind 'exe'
        New-Item -ItemType Directory -Force -Path $extractDir | Out-Null
        # The 7-Zip SFX stub accepts -o<dir> and -y for silent extraction.
        & $installer -y "-o$extractDir" | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Self-extracting installer failed with exit $LASTEXITCODE for $Url" }
    }
    finally {
        if (Test-Path $stagingParent) { Remove-Item -Recurse -Force $stagingParent }
    }

    New-ToolManifestEntry -ToolsRoot $ToolsRoot -Name $Name -Version $Version -Path $extractDir
    return $extractDir
}

function Remove-InstalledTools {
    param([string] $ToolsRoot)
    $manifest = Get-Manifest -ToolsRoot $ToolsRoot
    foreach ($name in @($manifest.Keys)) {
        $entry = $manifest[$name]
        if ($entry.path -and (Test-Path $entry.path)) {
            Write-Host "  Removing $name ($($entry.version))"
            Remove-Item -Recurse -Force $entry.path
        }
    }
    Save-Manifest -ToolsRoot $ToolsRoot -Manifest @{}
}

function Add-ToSessionPath {
    param([string] $Directory)
    if ($env:PATH -notlike "*$Directory*") {
        $env:PATH = "$Directory;$env:PATH"
    }
}

function Format-SizeTable {
    param([object[]] $Rows)
    $widths = @{}
    foreach ($col in @('Example', 'Flash', 'RAM')) {
        $widths[$col] = ($Rows + @([pscustomobject]@{ Example = $col; Flash = $col; RAM = $col }) |
                        ForEach-Object { "$($_.$col)".Length } |
                        Measure-Object -Maximum).Maximum
    }
    $sep = '  '
    $header = ('Example'.PadRight($widths.Example) + $sep + 'Flash'.PadLeft($widths.Flash) + $sep + 'RAM'.PadLeft($widths.RAM))
    $rule = ('-' * $widths.Example) + $sep + ('-' * $widths.Flash) + $sep + ('-' * $widths.RAM)
    $lines = @($header, $rule)
    foreach ($r in $Rows) {
        $lines += ($r.Example.PadRight($widths.Example) + $sep + "$($r.Flash)".PadLeft($widths.Flash) + $sep + "$($r.RAM)".PadLeft($widths.RAM))
    }
    return $lines
}

Export-ModuleMember -Function Get-ToolsRoot, Get-Manifest, Save-Manifest,
    Find-OnPath, Test-Sha256, Invoke-ToolDownload, Install-PortableZip, Install-SelfExtracting7z,
    Remove-InstalledTools, Add-ToSessionPath, Format-SizeTable
