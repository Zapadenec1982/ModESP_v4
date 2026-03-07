. 'C:\Espressif\tools\Microsoft.v5.5.3.PowerShell_profile.ps1' 2>$null
Set-Location 'D:\ModESP_v4'
Write-Host "=== Starting build ==="
idf.py build
Write-Host "=== Build exit code: $LASTEXITCODE ==="
