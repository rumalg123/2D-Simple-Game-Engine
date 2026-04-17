[CmdletBinding()]
param(
    [string]$BuildDir = "build",
    [string]$OutputDir = "dist",
    [string]$PackageName = "SimpleEngine",
    [string]$ExecutableName = "engine.exe",
    [string]$BuildTarget = "",
    [string]$GameConfig = "demo.game.json",
    [string]$PackageConfigName = "game.config.json",
    [string]$AssetsPath = "assets",
    [string]$ReadmePath = "README.md",
    [switch]$AllowMissingAssets,
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
$ResolvedAssetsPath = Resolve-RepoPath $AssetsPath
$ResolvedConfigPath = if ($GameConfig) { Resolve-RepoPath $GameConfig } else { "" }
$ResolvedReadmePath = if ($ReadmePath) { Resolve-RepoPath $ReadmePath } else { "" }
$PackagedConfigFileName = if ($GameConfig -and $PackageConfigName) { [System.IO.Path]::GetFileName($PackageConfigName) } else { "" }

if (!$SkipBuild) {
    if (!(Test-Path -LiteralPath $BuildPath)) {
        throw "Build directory does not exist: $BuildPath. Run 'cmake -S . -B build' first."
    }

    if ($BuildTarget) {
        & cmake --build $BuildPath --target $BuildTarget
    } else {
        & cmake --build $BuildPath
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }
}

$GameExe = Get-ChildItem -LiteralPath $BuildPath -Recurse -Filter $ExecutableName |
    Sort-Object FullName |
    Select-Object -First 1

if (!$GameExe) {
    throw "Could not find $ExecutableName under $BuildPath."
}

$RuntimeDir = $GameExe.Directory.FullName
if (!(Test-Path -LiteralPath $ResolvedAssetsPath) -and !$AllowMissingAssets) {
    throw "Assets directory does not exist: $ResolvedAssetsPath"
}

if ($ResolvedConfigPath -and !(Test-Path -LiteralPath $ResolvedConfigPath)) {
    throw "Game config does not exist: $ResolvedConfigPath"
}

New-Item -ItemType Directory -Force -Path $OutputPath | Out-Null
Assert-ChildPath $OutputPath $PackagePath
Assert-ChildPath $OutputPath $ZipPath

if (Test-Path -LiteralPath $PackagePath) {
    Remove-Item -LiteralPath $PackagePath -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $PackagePath | Out-Null

Copy-Item -LiteralPath $GameExe.FullName -Destination $PackagePath -Force

Get-ChildItem -LiteralPath $RuntimeDir -Filter "*.dll" | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $PackagePath -Force
}

$PackageAssetsPath = Join-Path $PackagePath "assets"
if (Test-Path -LiteralPath $ResolvedAssetsPath) {
    Copy-Item -LiteralPath $ResolvedAssetsPath -Destination $PackageAssetsPath -Recurse -Force
} else {
    New-Item -ItemType Directory -Force -Path $PackageAssetsPath | Out-Null
}

if ($ResolvedConfigPath) {
    $PackageConfigPath = Join-Path $PackagePath $PackagedConfigFileName
    $ConfigJson = Get-Content -Raw -LiteralPath $ResolvedConfigPath | ConvertFrom-Json
    if ($ConfigJson.PSObject.Properties.Name -contains "assetRoot") {
        $ConfigJson.assetRoot = "assets"
    }
    if ($ConfigJson.PSObject.Properties.Name -contains "prefabDirectory") {
        $ConfigJson.prefabDirectory = "assets/prefabs"
    }
    if ($ConfigJson.PSObject.Properties.Name -contains "editorLayoutPath") {
        $ConfigJson.editorLayoutPath = "assets/editor_layout.ini"
    }
    $ConfigJson | ConvertTo-Json | Set-Content -LiteralPath $PackageConfigPath -Encoding ASCII
}

if ($ResolvedReadmePath -and (Test-Path -LiteralPath $ResolvedReadmePath)) {
    Copy-Item -LiteralPath $ResolvedReadmePath -Destination (Join-Path $PackagePath "README.md") -Force
}

$LaunchCommand = "`"$ExecutableName`""
if ($PackagedConfigFileName) {
    $LaunchCommand = "`"$ExecutableName`" --config `"$PackagedConfigFileName`" --runtime"
}

$LaunchBat = @"
@echo off
pushd "%~dp0"
$LaunchCommand
popd
"@
Set-Content -LiteralPath (Join-Path $PackagePath "launch.bat") -Value $LaunchBat -Encoding ASCII

$Manifest = [ordered]@{
    name = $PackageName
    executable = $ExecutableName
    launcher = "launch.bat"
    config = $PackagedConfigFileName
    assets = "assets"
    platform = "windows"
    builtAt = (Get-Date).ToString("o")
    sourceExecutable = $GameExe.FullName
    sourceConfig = $ResolvedConfigPath
    sourceAssets = $ResolvedAssetsPath
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
