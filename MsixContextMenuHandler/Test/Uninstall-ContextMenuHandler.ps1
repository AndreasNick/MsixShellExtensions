#Requires -Version 5.1
<#
.SYNOPSIS
    Unregisters MsixContextMenuHandler.dll.

.PARAMETER DllPath
    Path to MsixContextMenuHandler.dll.
    Default: MsixContextMenuHandler.dll in this script's directory (copied there by Build-ContextMenuHandler.ps1).

.PARAMETER RestartExplorer
    Restart Explorer.exe after unregistering so the shell picks up the change immediately.
#>
param(
    [string] $DllPath = (Join-Path $PSScriptRoot 'MsixContextMenuHandler.dll'),
    [switch] $RestartExplorer
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$DllPath     = [IO.Path]::GetFullPath($DllPath)
$handlerName = 'MsixContextMenuHandler'

# CLSID from JSON; fall back to the compiled-in default
$configPath = Join-Path $PSScriptRoot 'MsixContextMenuHandler.json'
$clsidFromJson = if (Test-Path $configPath) {
    $parsedCfg = Get-Content $configPath -Raw | ConvertFrom-Json
    if ($parsedCfg.PSObject.Properties['clsid']) { $parsedCfg.clsid } else { $null }
} else { $null }
$clsid = if ($clsidFromJson) { $clsidFromJson } else { '{8B2A5D73-1F4E-4A3B-9C2E-7D6F8E1A3B5C}' }

# Unregister COM server (removes HKCU\Software\Classes\CLSID\{...})
if (Test-Path $DllPath) {
    Write-Verbose "Unregistering COM server..."
    & "$env:SystemRoot\System32\regsvr32.exe" /u /s $DllPath
} else {
    # DLL missing — remove the CLSID key directly so nothing is left behind
    $clsidKey = "HKCU:\Software\Classes\CLSID\$clsid"
    if (Test-Path $clsidKey) {
        Remove-Item -Path $clsidKey -Recurse -Force
        Write-Verbose "Removed orphaned CLSID key: $clsidKey"
    }
}

# Remove all file-type and folder associations from HKCU\Software\Classes
$classesRoot = 'HKCU:\Software\Classes'
$removed     = 0

if (Test-Path $classesRoot) {
    Get-ChildItem $classesRoot -ErrorAction SilentlyContinue | ForEach-Object {
        $handlerKey = Join-Path $_.PSPath "shellex\ContextMenuHandlers\$handlerName"
        if (Test-Path $handlerKey) {
            Remove-Item -Path $handlerKey -Recurse -Force
            Write-Verbose "Removed: $handlerKey"
            $removed++
        }
    }
}

# allFiles (*) handler — must be handled explicitly because * is a wildcard in PS paths
$allFilesKey = "HKCU:\Software\Classes\*\shellex\ContextMenuHandlers\$handlerName"
if (Test-Path -LiteralPath $allFilesKey) {
    Remove-Item -LiteralPath $allFilesKey -Recurse -Force
    Write-Verbose "Removed: $allFilesKey"
    $removed++
}

# Background handler lives one level deeper
$bgKey = "HKCU:\Software\Classes\Directory\Background\shellex\ContextMenuHandlers\$handlerName"
if (Test-Path $bgKey) {
    Remove-Item -Path $bgKey -Recurse -Force
    Write-Verbose "Removed: $bgKey"
    $removed++
}

# Notify the shell
$sig = '[DllImport("shell32.dll")] public static extern void SHChangeNotify(int e, uint f, IntPtr a, IntPtr b);'
Add-Type -MemberDefinition $sig -Name Shell32Uninstall -Namespace Win32CMHUninst -PassThru | Out-Null
[Win32CMHUninst.Shell32Uninstall]::SHChangeNotify(0x08000000, 0x0000, [IntPtr]::Zero, [IntPtr]::Zero)

Write-Output "Uninstalled ($removed association(s) removed)."

if ($RestartExplorer) {
    Write-Output "Restarting Explorer..."
    Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
    Start-Process explorer
    Write-Output "Explorer restarted."
} else {
    Write-Output "Tip: if the menu still appears, run with -RestartExplorer"
}
