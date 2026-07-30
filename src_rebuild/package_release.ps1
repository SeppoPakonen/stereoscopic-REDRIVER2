Param(
    [string]$BuildDir = "build/bin",
    [string]$OutputDir = "release_dist",
    [string]$Version = "7.5-dev",
    [switch]$CreateZip = $true,
    [string]$ZipName = "REDRIVER2_Win32_Release_$Version.zip"
)

Write-Host "Creating REDRIVER2 release distribution package..." -ForegroundColor Cyan
Write-Host "Build Dir: $BuildDir" -ForegroundColor Gray
Write-Host "Output Dir: $OutputDir" -ForegroundColor Gray
Write-Host ""

# Create output directories
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$staging = "$OutputDir/staging"
Remove-Item -Recurse -Force $staging -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $staging | Out-Null

Write-Host "📦 Copying binaries..." -ForegroundColor Yellow

# Copy main executable
if (Test-Path "$BuildDir/REDRIVER2.exe") {
    Copy-Item "$BuildDir/REDRIVER2.exe" $staging/
    Write-Host "✓ REDRIVER2.exe" -ForegroundColor Green
}

# Copy all DLLs
$dllCount = (Get-ChildItem "$BuildDir/*.dll" -ErrorAction SilentlyContinue | Measure-Object).Count
if ($dllCount -gt 0) {
    Copy-Item "$BuildDir/*.dll" $staging/
    Write-Host "✓ Copied $dllCount DLL files" -ForegroundColor Green
}

# Copy resources if they exist
if (Test-Path "$BuildDir/resources") {
    Copy-Item -Recurse "$BuildDir/resources" $staging/ -ErrorAction SilentlyContinue
    Write-Host "✓ resources/" -ForegroundColor Green
}

# Copy data files
if (Test-Path "$BuildDir/data") {
    Copy-Item -Recurse "$BuildDir/data" $staging/ -ErrorAction SilentlyContinue
    Write-Host "✓ data/" -ForegroundColor Green
}

# Copy config files
if (Test-Path "$BuildDir/config") {
    Copy-Item -Recurse "$BuildDir/config" $staging/ -ErrorAction SilentlyContinue
    Write-Host "✓ config/" -ForegroundColor Green
}

# Copy any .ini, .cfg, .conf files
Get-ChildItem "$BuildDir/*.ini" -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item $_.FullName $staging/
}
Get-ChildItem "$BuildDir/*.cfg" -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item $_.FullName $staging/
}

Write-Host ""
Write-Host "📋 Release contents:" -ForegroundColor Yellow
Get-ChildItem $staging -Recurse | Measure-Object | ForEach-Object { Write-Host "Total files: $($_.Count)" }

if ($CreateZip) {
    Write-Host ""
    Write-Host "🔄 Creating archive: $ZipName..." -ForegroundColor Yellow

    $7zipPath = "C:\Program Files\7-Zip\7z.exe"
    if (-not (Test-Path $7zipPath)) {
        $7zipPath = "C:\Program Files (x86)\7-Zip\7z.exe"
    }

    if (Test-Path $7zipPath) {
        & $7zipPath a -t7z -m0=lzma2 -mx=9 "$OutputDir/$ZipName" "$staging/*" | Out-Null
        $zipSize = (Get-Item "$OutputDir/$ZipName").Length / 1MB
        Write-Host "✓ Archive created: $ZipName ($([Math]::Round($zipSize, 2)) MB)" -ForegroundColor Green
    } else {
        Write-Host "⚠ 7-Zip not found, using Windows ZIP instead..." -ForegroundColor Yellow
        Compress-Archive -Path "$staging/*" -DestinationPath "$OutputDir/${ZipName%.zip}.zip" -Force
        Write-Host "✓ ZIP archive created" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "✅ Release packaging completed successfully!" -ForegroundColor Green
Write-Host "Output location: $(Resolve-Path $OutputDir)" -ForegroundColor Gray
