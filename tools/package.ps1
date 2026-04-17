[CmdletBinding()]
param(
    [string]$BuildDir = "build",
    [string]$OutputDir = "dist",
    [string]$PackageName = "SimpleEngine",
    [switch]$SkipBuild,
    [switch]$NoZip
)

$ErrorActionPreference = "Stop"

function Resolve-RepoPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $RepoRoot $Path
}

function Assert-ChildPath([string]$Parent, [string]$Child) {
    $parentPath = [System.IO.Path]::GetFullPath($Parent).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $childPath = [System.IO.Path]::GetFullPath($Child).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $parentWithSeparator = $parentPath + [System.IO.Path]::DirectorySeparatorChar

    if ($childPath -ne $parentPath -and !$childPath.StartsWith($parentWithSeparator, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to write outside package output directory: $childPath"
    }
}

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$BuildPath = Resolve-RepoPath $BuildDir
$OutputPath = Resolve-RepoPath $OutputDir
$PackagePath = Join-Path $OutputPath $PackageName
$ZipPath = Join-Path $OutputPath "$PackageName.zip"

if (!$SkipBuild) {
    if (!(Test-Path -LiteralPath $BuildPath)) {
        throw "Build directory does not exist: $BuildPath. Run 'cmake -S . -B build' first."
    }

    & cmake --build $BuildPath
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }
}

$EngineExe = Get-ChildItem -LiteralPath $BuildPath -Recurse -Filter "engine.exe" |
    Sort-Object FullName |
    Select-Object -First 1

if (!$EngineExe) {
    throw "Could not find engine.exe under $BuildPath."
}

$RuntimeDir = $EngineExe.Directory.FullName
$AssetsPath = Join-Path $RepoRoot "assets"
if (!(Test-Path -LiteralPath $AssetsPath)) {
    throw "Assets directory does not exist: $AssetsPath"
}

New-Item -ItemType Directory -Force -Path $OutputPath | Out-Null
Assert-ChildPath $OutputPath $PackagePath
Assert-ChildPath $OutputPath $ZipPath

if (Test-Path -LiteralPath $PackagePath) {
    Remove-Item -LiteralPath $PackagePath -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $PackagePath | Out-Null

Copy-Item -LiteralPath $EngineExe.FullName -Destination $PackagePath -Force

Get-ChildItem -LiteralPath $RuntimeDir -Filter "*.dll" | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $PackagePath -Force
}

Copy-Item -LiteralPath $AssetsPath -Destination (Join-Path $PackagePath "assets") -Recurse -Force

$ReadmePath = Join-Path $RepoRoot "README.md"
if (Test-Path -LiteralPath $ReadmePath) {
    Copy-Item -LiteralPath $ReadmePath -Destination $PackagePath -Force
}

$LaunchBat = @"
@echo off
pushd "%~dp0"
engine.exe
popd
"@
Set-Content -LiteralPath (Join-Path $PackagePath "launch.bat") -Value $LaunchBat -Encoding ASCII

$Manifest = [ordered]@{
    name = $PackageName
    executable = "engine.exe"
    launcher = "launch.bat"
    assets = "assets"
    platform = "windows"
    builtAt = (Get-Date).ToString("o")
}
$Manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PackagePath "manifest.json") -Encoding ASCII

if (!$NoZip) {
    if (Test-Path -LiteralPath $ZipPath) {
        Remove-Item -LiteralPath $ZipPath -Force
    }

    Compress-Archive -Path (Join-Path $PackagePath "*") -DestinationPath $ZipPath -Force
}

Write-Host "Packaged $PackageName"
Write-Host "Folder: $PackagePath"
if (!$NoZip) {
    Write-Host "Zip:    $ZipPath"
}
