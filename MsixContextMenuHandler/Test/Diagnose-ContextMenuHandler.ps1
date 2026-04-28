#Requires -Version 5.1
<#
.SYNOPSIS
    Diagnoses the MsixContextMenuHandler registration.
#>
param(
    [string] $DllPath = (Join-Path $PSScriptRoot 'MsixContextMenuHandler.dll')
)

$clsid       = '{8B2A5D73-1F4E-4A3B-9C2E-7D6F8E1A3B5C}'
$handlerName = 'MsixContextMenuHandler'
$ok          = $true

function Check([bool]$result, [string]$label, [string]$detail = '') {
    if ($result) {
        Write-Host "  [OK]  $label" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] $label" -ForegroundColor Red
        if ($detail) { Write-Host "        $detail" -ForegroundColor Yellow }
        $script:ok = $false
    }
}

Write-Host "`n--- DLL ---"
$dllExists = Test-Path $DllPath
Check $dllExists "DLL exists" $DllPath
if ($dllExists) {
    Check ((Get-Item $DllPath).Length -gt 0) "DLL size > 0"
}

Write-Host "`n--- COM registration (HKCU) ---"
$clsidKey   = "HKCU:\Software\Classes\CLSID\$clsid"
$inprocKey  = "$clsidKey\InprocServer32"
Check (Test-Path $clsidKey)  "CLSID key exists:       $clsidKey"
Check (Test-Path $inprocKey) "InprocServer32 exists:  $inprocKey"
if (Test-Path $inprocKey) {
    $regPath = (Get-ItemProperty $inprocKey).'(default)'
    Check ($regPath -eq $DllPath) "InprocServer32 path matches DLL" "Registry: $regPath"
    $tm = (Get-ItemProperty $inprocKey -Name ThreadingModel -ErrorAction SilentlyContinue).ThreadingModel
    Check ($tm -eq 'Apartment') "ThreadingModel = Apartment" "Found: $tm"
}

Write-Host "`n--- File type associations (HKCU) ---"
$configPath = Join-Path $PSScriptRoot 'MsixContextMenuHandler.json'
if (Test-Path $configPath) {
    $cfg = Get-Content $configPath -Raw | ConvertFrom-Json
    foreach ($ext in $cfg.fileTypes) {
        $key = "HKCU:\Software\Classes\$ext\shellex\ContextMenuHandlers\$handlerName"
        $val = if (Test-Path $key) { (Get-ItemProperty $key).'(default)' } else { $null }
        Check ($val -eq $clsid) "Association for $ext" "Key missing or wrong value: $val"
    }
    if ($cfg.folders) {
        foreach ($class in 'Directory', 'Drive') {
            $key = "HKCU:\Software\Classes\$class\shellex\ContextMenuHandlers\$handlerName"
            $val = if (Test-Path $key) { (Get-ItemProperty $key).'(default)' } else { $null }
            Check ($val -eq $clsid) "Association for $class" "Key missing or wrong value: $val"
        }
    }
} else {
    Write-Host "  [SKIP] Config not found at $configPath — skipping association checks" -ForegroundColor DarkYellow
}

Write-Host "`n--- DLL load test (regsvr32 without /s) ---"
Write-Host "  Running regsvr32 interactively — close the dialog to continue..." -ForegroundColor Cyan
& "$env:SystemRoot\System32\regsvr32.exe" $DllPath
Write-Host "  regsvr32 exit code: $LASTEXITCODE"
Check ($LASTEXITCODE -eq 0) "regsvr32 DllRegisterServer succeeded"

Write-Host "`n--- Shell Extensions Approved list (HKLM) ---"
$approvedKey = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved'
$approved    = (Get-ItemProperty $approvedKey -Name $clsid -ErrorAction SilentlyContinue).$clsid
if ($approved) {
    Check $true "CLSID in Approved list: $approved"
} else {
    Write-Host "  [INFO] CLSID not in Approved list (HKLM)." -ForegroundColor Cyan
    Write-Host "         HKCU extensions normally work without it, but some Win11 configs require it." -ForegroundColor Cyan
    Write-Host "         Run as Admin to add it:" -ForegroundColor Cyan
    Write-Host "         Set-ItemProperty '$approvedKey' -Name '$clsid' -Value 'MsixContextMenuHandler'" -ForegroundColor Yellow
}

Write-Host ""
if ($ok) {
    Write-Host "All checks passed." -ForegroundColor Green
} else {
    Write-Host "One or more checks failed — see above." -ForegroundColor Red
}
