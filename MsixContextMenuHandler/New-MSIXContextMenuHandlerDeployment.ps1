#Requires -Version 5.1
<#
.SYNOPSIS
    Creates a new deployment folder for MsixContextMenuHandler.dll with a unique CLSID.

.DESCRIPTION
    Copies MsixContextMenuHandler.dll and a JSON config to a new target directory.
    A fresh CLSID is generated and written into the JSON so that multiple deployments
    (e.g. WinRAR + 7-Zip) can coexist without CLSID conflicts.

    The same DLL binary is reused — no rebuild required.

.PARAMETER TargetDir
    Directory to create the deployment in. Created if it does not exist.

.PARAMETER JsonTemplate
    Path to the JSON config to use as template.
    The "clsid" field will be set to the newly generated GUID.

.PARAMETER DllSource
    Path to MsixContextMenuHandler.dll.
    Default: Test\MsixContextMenuHandler.dll (built by Build-ContextMenuHandler.ps1).

.PARAMETER Install
    After creating the deployment, run Install-ContextMenuHandler.ps1 from TargetDir.

.PARAMETER RestartExplorer
    Passed through to Install-ContextMenuHandler.ps1 when -Install is used.

.EXAMPLE
    .\New-MSIXContextMenuHandlerDeployment.ps1 `
        -TargetDir 'C:\Deploy\7Zip' `
        -JsonTemplate '.\Samples\7zip.json' `
        -Install

.EXAMPLE
    # Create deployment only, install manually later
    .\New-MSIXContextMenuHandlerDeployment.ps1 `
        -TargetDir 'C:\Deploy\WinRAR' `
        -JsonTemplate '.\Test\MsixContextMenuHandler.json'
#>
param(
    [Parameter(Mandatory)]
    [string] $TargetDir,

    [Parameter(Mandatory)]
    [string] $JsonTemplate,

    [string] $DllSource = (Join-Path $PSScriptRoot 'Test\MsixContextMenuHandler.dll'),

    [switch] $Install,
    [switch] $RestartExplorer
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$DllSource    = [IO.Path]::GetFullPath($DllSource)
$JsonTemplate = [IO.Path]::GetFullPath($JsonTemplate)
$TargetDir    = [IO.Path]::GetFullPath($TargetDir)

if (-not (Test-Path $DllSource)) {
    Write-Error "DLL not found: $DllSource`nRun Build-ContextMenuHandler.ps1 first."
    exit 1
}
if (-not (Test-Path $JsonTemplate)) {
    Write-Error "JSON template not found: $JsonTemplate"
    exit 1
}

# Create target directory
if (-not (Test-Path $TargetDir)) {
    $null = New-Item -Path $TargetDir -ItemType Directory -Force
    Write-Verbose "Created: $TargetDir"
}

# Generate a new CLSID
$newGuid  = [Guid]::NewGuid()
$newClsid = ('{' + $newGuid.ToString('D').ToUpper() + '}')

# Read template JSON, set/replace "clsid" field, write to target
$cfg = Get-Content $JsonTemplate -Raw | ConvertFrom-Json
$cfg | Add-Member -MemberType NoteProperty -Name 'clsid' -Value $newClsid -Force
$json = $cfg | ConvertTo-Json -Depth 10

$dstJson = Join-Path $TargetDir 'MsixContextMenuHandler.json'
[IO.File]::WriteAllText($dstJson, $json, [Text.Encoding]::UTF8)

# Copy DLL
$dstDll = Join-Path $TargetDir 'MsixContextMenuHandler.dll'
Copy-Item -Path $DllSource -Destination $dstDll -Force

Write-Output "Deployment created in: $TargetDir"
Write-Output "CLSID: $newClsid"
Write-Output "DLL:   $dstDll"
Write-Output "JSON:  $dstJson"

# Copy install/uninstall scripts so the deployment folder is self-contained
foreach ($script in 'Install-ContextMenuHandler.ps1', 'Uninstall-ContextMenuHandler.ps1') {
    $src = Join-Path $PSScriptRoot "Test\$script"
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination (Join-Path $TargetDir $script) -Force
        Write-Verbose "Copied: $script"
    }
}

if ($Install) {
    $installScript = Join-Path $TargetDir 'Install-ContextMenuHandler.ps1'
    Write-Output "`nRunning installer..."
    $splat = @{ DllPath = $dstDll; ConfigPath = $dstJson }
    if ($RestartExplorer) { $splat['RestartExplorer'] = $true }
    & $installScript @splat
}
