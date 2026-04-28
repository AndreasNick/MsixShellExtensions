#Requires -Version 5.1
<#
.SYNOPSIS
    Unregisters MsixModernContextMenuHandler.dll.

.PARAMETER UseHKCU
    Remove from HKCU (use if installed with -UseHKCU).
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

if (-not $UseHKCU) {
    $principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        Write-Error "HKLM removal requires admin rights. Run as Administrator, or use -UseHKCU."
        exit 1
    }
}

$clsid = '{4A7B3C1D-E2F5-4689-ABCD-EF1234567891}'
if (Test-Path $ConfigPath) {
    $cfg = Get-Content $ConfigPath -Raw | ConvertFrom-Json
    if ($cfg.PSObject.Properties['clsid'] -and $cfg.clsid) { $clsid = $cfg.clsid }
}

$subKeyName = $clsid

if ($UseHKCU) {
    $rootPath = 'HKCU:\Software\Classes'
} else {
    $rootPath = 'HKLM:\SOFTWARE\Classes'
}

function Remove-ExplorerCommandHandler {
    param([string] $KeyPath)
    $isHklm  = $KeyPath.StartsWith('HKLM:\')
    $relPath = $KeyPath -replace '^HK[LC][MU]:\\', ''
    if ($isHklm) {
        try { [Microsoft.Win32.Registry]::LocalMachine.DeleteSubKeyTree($relPath, $false) } catch { }
    } else {
        try { [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($relPath, $false) } catch { }
    }
    Write-Verbose "Removed: $KeyPath"
}

Remove-ExplorerCommandHandler "$rootPath\*\shell\$subKeyName"
Remove-ExplorerCommandHandler "$rootPath\Directory\shell\$subKeyName"
Remove-ExplorerCommandHandler "$rootPath\Drive\shell\$subKeyName"
Remove-ExplorerCommandHandler "$rootPath\Directory\Background\shell\$subKeyName"

if (Test-Path $DllPath) {
    & "$env:SystemRoot\System32\regsvr32.exe" /s /u $DllPath
    Write-Verbose "COM server unregistered from HKCU."
} else {
    try {
        [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree(
            "Software\Classes\CLSID\$clsid", $false)
    } catch { }
}
if (-not $UseHKCU) {
    try {
        [Microsoft.Win32.Registry]::LocalMachine.DeleteSubKeyTree(
            "SOFTWARE\Classes\CLSID\$clsid", $false)
        Write-Verbose "COM server unregistered from HKLM."
    } catch { }
}

$sig = '[DllImport("shell32.dll")] public static extern void SHChangeNotify(int e, uint f, IntPtr a, IntPtr b);'
Add-Type -MemberDefinition $sig -Name Shell32ModernUninstall -Namespace Win32MCMHUninstall -PassThru | Out-Null
[Win32MCMHUninstall.Shell32ModernUninstall]::SHChangeNotify(0x08000000, 0x0000, [IntPtr]::Zero, [IntPtr]::Zero)

Write-Output "Uninstalled MsixModernContextMenuHandler."

if ($RestartExplorer) {
    Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 800
    Start-Process explorer
    Write-Output "Explorer restarted."
}
