#Requires -Version 5.1
<#
.SYNOPSIS
    Registers MsixModernContextMenuHandler.dll for testing outside MSIX.

.DESCRIPTION
    Registers the COM server and IExplorerCommand shell hooks.

    Windows 11 modern context menu (top section):
    - ExplorerCommandHandler must be under HKLM (requires admin).
    - HKCU registration only reaches the classic "Show more options" flyout.
    - The shell key name must be the CLSID itself.

    Use -UseHKCU to register under HKCU only (no admin, classic menu only).

.PARAMETER DllPath
    Path to MsixModernContextMenuHandler.dll.

.PARAMETER ConfigPath
    Path to the JSON config file.

.PARAMETER UseHKCU
    Register under HKCU (no admin, classic menu only).
#>
param(
    [string] $DllPath    = (Join-Path $PSScriptRoot 'MsixModernContextMenuHandler.dll'),
    [string] $ConfigPath = (Join-Path $PSScriptRoot 'MsixModernContextMenuHandler.json'),
    [switch] $UseHKCU,
    [switch] $RestartExplorer
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$DllPath    = [IO.Path]::GetFullPath($DllPath)
$ConfigPath = [IO.Path]::GetFullPath($ConfigPath)

if (-not (Test-Path $DllPath)) {
    Write-Error "DLL not found: $DllPath"
    exit 1
}
if (-not (Test-Path $ConfigPath)) {
    Write-Error "Config not found: $ConfigPath"
    exit 1
}

if (-not $UseHKCU) {
    $principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        Write-Error "HKLM registration requires admin rights. Run as Administrator, or add -UseHKCU for classic-menu-only testing."
        exit 1
    }
}

# Copy config next to the DLL
$dllDir     = Split-Path $DllPath
$dllJsonDst = Join-Path $dllDir 'MsixModernContextMenuHandler.json'
if ($dllJsonDst -ne $ConfigPath) {
    Copy-Item $ConfigPath $dllJsonDst -Force
    Write-Verbose "Copied config to $dllJsonDst"
}

$cfg = Get-Content $ConfigPath -Raw | ConvertFrom-Json

$clsid = '{4A7B3C1D-E2F5-4689-ABCD-EF1234567891}'
if ($cfg.PSObject.Properties['clsid'] -and $cfg.clsid) {
    $clsid = $cfg.clsid
}

# Register COM InprocServer32.
# For the modern W11 context menu Explorer needs both ExplorerCommandHandler and
# InprocServer32 in HKLM. regsvr32 only writes HKCU, so we write HKLM directly
# when running with admin rights. HKCU is written as fallback for non-admin use.
$LASTEXITCODE = 0
& "$env:SystemRoot\System32\regsvr32.exe" /s $DllPath
if ($LASTEXITCODE -ne 0) {
    Write-Error "regsvr32 failed (exit $LASTEXITCODE)"
    exit 1
}
Write-Verbose "COM server registered in HKCU."

if (-not $UseHKCU) {
    $clsidKeyHklm = [Microsoft.Win32.Registry]::LocalMachine.CreateSubKey(
        "SOFTWARE\Classes\CLSID\$clsid\InprocServer32")
    $clsidKeyHklm.SetValue('', $DllPath)
    $clsidKeyHklm.SetValue('ThreadingModel', 'Apartment')
    $clsidKeyHklm.Close()
    Write-Verbose "COM server also registered in HKLM."
}

$subKeyName = $clsid

if ($UseHKCU) {
    $rootPath  = 'HKCU:\Software\Classes'
    $hiveLabel = 'HKCU'
    Write-Warning "Using HKCU: entry will appear in Show more options (classic menu) only."
} else {
    $rootPath  = 'HKLM:\SOFTWARE\Classes'
    $hiveLabel = 'HKLM'
    Write-Verbose "Using HKLM: entry will appear in the modern Windows 11 context menu."
}

function Set-ExplorerCommandHandler {
    param([string] $KeyPath, [string] $Value)
    $isHklm  = $KeyPath.StartsWith('HKLM:\')
    $relPath = $KeyPath -replace '^HK[LC][MU]:\\', ''
    if ($isHklm) {
        $key = [Microsoft.Win32.Registry]::LocalMachine.CreateSubKey($relPath)
    } else {
        $key = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($relPath)
    }
    $key.SetValue('ExplorerCommandHandler', $Value)
    $key.Close()
    Write-Verbose "Registered: $KeyPath"
}

if ($cfg.PSObject.Properties['allFiles'] -and $cfg.allFiles) {
    Set-ExplorerCommandHandler "$rootPath\*\shell\$subKeyName" $clsid
}

if ($cfg.PSObject.Properties['folders'] -and $cfg.folders) {
    Set-ExplorerCommandHandler "$rootPath\Directory\shell\$subKeyName" $clsid
    Set-ExplorerCommandHandler "$rootPath\Drive\shell\$subKeyName"     $clsid
}

if ($cfg.PSObject.Properties['background'] -and $cfg.background) {
    Set-ExplorerCommandHandler "$rootPath\Directory\Background\shell\$subKeyName" $clsid
}

foreach ($ext in $cfg.fileTypes) {
    Set-ExplorerCommandHandler "$rootPath\$ext\shell\$subKeyName" $clsid
}

$sig = '[DllImport("shell32.dll")] public static extern void SHChangeNotify(int e, uint f, IntPtr a, IntPtr b);'
Add-Type -MemberDefinition $sig -Name Shell32ModernInstall -Namespace Win32MCMH -PassThru | Out-Null
[Win32MCMH.Shell32ModernInstall]::SHChangeNotify(0x08000000, 0x0000, [IntPtr]::Zero, [IntPtr]::Zero)

Write-Output "Installed ($hiveLabel). Right-click a file or folder to test."
Write-Output "DLL:    $DllPath"
Write-Output "Config: $dllJsonDst"
Write-Output "CLSID:  $clsid"

if ($RestartExplorer) {
    Write-Output "Restarting Explorer..."
    Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 800
    Start-Process explorer
    Write-Output "Explorer restarted."
} else {
    Write-Output "Tip: if the menu does not appear, run with -RestartExplorer"
}
