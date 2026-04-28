#Requires -Version 5.1
<#
.SYNOPSIS
    Registers MsixContextMenuHandler.dll for testing outside MSIX.

.DESCRIPTION
    1. Registers the COM InprocServer32 via regsvr32 (writes HKCU, no admin required).
    2. Copies MsixContextMenuHandler.json next to the DLL so the DLL can find it.
    3. Registers file-type and folder context menu hooks in HKCU.
    4. Notifies the shell.

.PARAMETER DllPath
    Path to MsixContextMenuHandler.dll.
    Default: ..\bin\Release\MsixContextMenuHandler.dll (relative to this script).

.PARAMETER ConfigPath
    Path to the JSON config to use.
    Default: MsixContextMenuHandler.json in this script's directory.
#>
param(
    # Build-ContextMenuHandler.ps1 copies the DLL to this Test directory.
    # Relative paths in the JSON (executable, icon) are resolved against the DLL directory,
    # so place WinRAR.exe (or the target app) here for outside-MSIX testing.
    [string] $DllPath    = (Join-Path $PSScriptRoot 'MsixContextMenuHandler.dll'),
    [string] $ConfigPath = (Join-Path $PSScriptRoot 'MsixContextMenuHandler.json'),
    [switch] $RestartExplorer
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$DllPath    = [IO.Path]::GetFullPath($DllPath)
$ConfigPath = [IO.Path]::GetFullPath($ConfigPath)

if (-not (Test-Path $DllPath)) {
    Write-Error "DLL not found: $DllPath - build the project first (Release|x64)."
    exit 1
}
if (-not (Test-Path $ConfigPath)) {
    Write-Error "Config not found: $ConfigPath"
    exit 1
}

# The DLL looks for the JSON in its own directory.
$dllDir     = Split-Path $DllPath
$dllJsonDst = Join-Path $dllDir 'MsixContextMenuHandler.json'
if ($dllJsonDst -ne $ConfigPath) {
    Copy-Item $ConfigPath $dllJsonDst -Force
    Write-Verbose "Copied config to $dllJsonDst"
}

# Register COM server (writes HKCU\Software\Classes\CLSID\{...}\InprocServer32)
Write-Verbose "Registering COM server..."
$LASTEXITCODE = 0
& "$env:SystemRoot\System32\regsvr32.exe" /s $DllPath
if ($LASTEXITCODE -ne 0) {
    Write-Error "regsvr32 failed (exit $LASTEXITCODE)"
    exit 1
}

# CLSID — must match Guid.h
$handlerName = 'MsixContextMenuHandler'

# Read file types and flags from config
$cfg = Get-Content $ConfigPath -Raw | ConvertFrom-Json

# CLSID from JSON; fall back to the compiled-in default
$clsid = if ($cfg.PSObject.Properties['clsid']) { $cfg.clsid } else { '{8B2A5D73-1F4E-4A3B-9C2E-7D6F8E1A3B5C}' }

foreach ($ext in $cfg.fileTypes) {
    $key = "HKCU:\Software\Classes\$ext\shellex\ContextMenuHandlers\$handlerName"
    $null = New-Item -Path $key -Force
    Set-ItemProperty -Path $key -Name '(default)' -Value $clsid
    Write-Verbose "Registered for $ext"
}

if ($cfg.PSObject.Properties['allFiles'] -and $cfg.allFiles) {
    # New-Item has no -LiteralPath in PS 5.1; use .NET API to avoid * wildcard expansion
    $regKey = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey(
        "Software\Classes\*\shellex\ContextMenuHandlers\$handlerName")
    $regKey.SetValue('', $clsid)
    $regKey.Close()
    Write-Verbose "Registered for all files (*)"
}

if ($cfg.PSObject.Properties['folders'] -and $cfg.folders) {
    foreach ($class in 'Directory', 'Drive', 'Folder') {
        $key = "HKCU:\Software\Classes\$class\shellex\ContextMenuHandlers\$handlerName"
        $null = New-Item -Path $key -Force
        Set-ItemProperty -Path $key -Name '(default)' -Value $clsid
        Write-Verbose "Registered for $class"
    }
}

if ($cfg.PSObject.Properties['recycleBin'] -and $cfg.recycleBin) {
    $key = "HKCU:\Software\Classes\CLSID\{645FF040-5081-101B-9F08-00AA002F954E}\shellex\ContextMenuHandlers\$handlerName"
    $null = New-Item -Path $key -Force
    Set-ItemProperty -Path $key -Name '(default)' -Value $clsid
    Write-Verbose "Registered for Recycle Bin"
}

if ($cfg.PSObject.Properties['background'] -and $cfg.background) {
    $key = "HKCU:\Software\Classes\Directory\Background\shellex\ContextMenuHandlers\$handlerName"
    $null = New-Item -Path $key -Force
    Set-ItemProperty -Path $key -Name '(default)' -Value $clsid
    Write-Verbose "Registered for directory background"
}

# Notify the shell so Explorer picks up the change immediately
$sig = '[DllImport("shell32.dll")] public static extern void SHChangeNotify(int e, uint f, IntPtr a, IntPtr b);'
Add-Type -MemberDefinition $sig -Name Shell32Install -Namespace Win32CMH -PassThru | Out-Null
[Win32CMH.Shell32Install]::SHChangeNotify(0x08000000, 0x0000, [IntPtr]::Zero, [IntPtr]::Zero)

Write-Output "Installed. Right-click a registered file type or folder to test."
Write-Output "DLL:    $DllPath"
Write-Output "Config: $dllJsonDst"

if ($RestartExplorer) {
    Write-Output "Restarting Explorer..."
    Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
    Start-Process explorer
    Write-Output "Explorer restarted."
} else {
    Write-Output "Tip: if the menu does not appear, run with -RestartExplorer"
}
