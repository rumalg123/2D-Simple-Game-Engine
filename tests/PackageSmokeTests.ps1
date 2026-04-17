[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RepoRoot,
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,
    [Parameter(Mandatory = $true)]
    [string]$OutputDir,
    [Parameter(Mandatory = $true)]
    [string]$Target,
    [Parameter(Mandatory = $true)]
    [string]$PackageName,
    [Parameter(Mandatory = $true)]
    [string]$ExecutableName,
    [Parameter(Mandatory = $true)]
    [string]$GameConfig,
    [Parameter(Mandatory = $true)]
    [string]$AssetsPath,
    [Parameter(Mandatory = $true)]
    [string]$ReadmePath,
    [switch]$AllowMissingAssets
)

$ErrorActionPreference = "Stop"

function Assert-True([bool]$Condition, [string]$Message) {
    if (!$Condition) {
        throw $Message
    }
}

function Assert-PathExists([string]$Path, [string]$Message) {
    Assert-True (Test-Path -LiteralPath $Path) "$Message Missing path: $Path"
}

$RepoPath = [System.IO.Path]::GetFullPath($RepoRoot)
$BuildPath = [System.IO.Path]::GetFullPath($BuildDir)
$OutputPath = [System.IO.Path]::GetFullPath($OutputDir)
$PackagePath = Join-Path $OutputPath $PackageName
$PackageScript = Join-Path $RepoPath "tools/package.ps1"

Assert-PathExists $PackageScript "Package script should exist."

& cmake --build $BuildPath --target $Target
if ($LASTEXITCODE -ne 0) {
    throw "Build target '$Target' failed with exit code $LASTEXITCODE."
}

$packageArgs = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $PackageScript,
    "-BuildDir", $BuildPath,
    "-OutputDir", $OutputPath,
    "-PackageName", $PackageName,
    "-ExecutableName", $ExecutableName,
    "-GameConfig", $GameConfig,
    "-AssetsPath", $AssetsPath,
    "-ReadmePath", $ReadmePath,
    "-SkipBuild",
    "-NoZip"
)

if ($AllowMissingAssets) {
    $packageArgs += "-AllowMissingAssets"
}

& powershell @packageArgs
if ($LASTEXITCODE -ne 0) {
    throw "Package script failed with exit code $LASTEXITCODE."
}

Assert-PathExists $PackagePath "Package folder should be created."
Assert-PathExists (Join-Path $PackagePath $ExecutableName) "Package executable should be copied."
Assert-PathExists (Join-Path $PackagePath "launch.bat") "Package launcher should be created."
Assert-PathExists (Join-Path $PackagePath "manifest.json") "Package manifest should be created."
Assert-PathExists (Join-Path $PackagePath "game.config.json") "Package config should be created."
Assert-PathExists (Join-Path $PackagePath "assets") "Package assets directory should be created."
Assert-PathExists (Join-Path $PackagePath "README.md") "Package README should be copied."

$dlls = Get-ChildItem -LiteralPath $PackagePath -Filter "*.dll"
Assert-True ($dlls.Count -gt 0) "Package should include runtime DLLs."

$launcher = Get-Content -Raw -LiteralPath (Join-Path $PackagePath "launch.bat")
Assert-True ($launcher.Contains("`"$ExecutableName`" --config `"game.config.json`" --runtime")) "Launcher should run the packaged executable with runtime config."

$manifest = Get-Content -Raw -LiteralPath (Join-Path $PackagePath "manifest.json") | ConvertFrom-Json
Assert-True ($manifest.name -eq $PackageName) "Manifest should contain package name."
Assert-True ($manifest.executable -eq $ExecutableName) "Manifest should contain executable name."
Assert-True ($manifest.launcher -eq "launch.bat") "Manifest should contain launcher."
Assert-True ($manifest.config -eq "game.config.json") "Manifest should contain config name."
Assert-True ($manifest.assets -eq "assets") "Manifest should contain assets directory."

$config = Get-Content -Raw -LiteralPath (Join-Path $PackagePath "game.config.json") | ConvertFrom-Json
Assert-True ($config.assetRoot -eq "assets") "Packaged config should use local assets."
Assert-True ($config.prefabDirectory -eq "assets/prefabs") "Packaged config should use local prefab directory."
Assert-True ($config.editorLayoutPath -eq "assets/editor_layout.ini") "Packaged config should use local editor layout path."

Write-Host "Package smoke test passed: $PackagePath"
