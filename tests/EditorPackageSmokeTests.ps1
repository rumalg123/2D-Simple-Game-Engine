[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$RepoRoot,
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [Parameter(Mandatory = $true)][string]$OutputDir
)

$ErrorActionPreference = "Stop"
$packageName = "SimpleEngineEditorProjectHubSmoke"
$executableName = "SimpleEngineEditor.exe"
$packagePath = Join-Path $OutputDir $packageName
$packageScript = Join-Path $RepoRoot "tools/package.ps1"

& $packageScript -BuildDir $BuildDir -OutputDir $OutputDir `
    -PackageName $packageName -ExecutableName $executableName `
    -GameConfig '' -AssetsPath (Join-Path $RepoRoot "assets") `
    -ReadmePath (Join-Path $RepoRoot "README.md") `
    -LaunchMode editor -SkipBuild -NoZip
if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    throw "Editor packaging failed with exit code $LASTEXITCODE."
}

$exe = Join-Path $packagePath $executableName
$launcher = Join-Path $packagePath "launch.bat"
$manifestPath = Join-Path $packagePath "manifest.json"
foreach ($required in @($exe, $launcher, $manifestPath)) {
    if (!(Test-Path -LiteralPath $required)) {
        throw "Editor package is missing: $required"
    }
}
if (Test-Path -LiteralPath (Join-Path $packagePath "game.config.json")) {
    throw "Project Hub launcher must not package demo.game.json."
}
$launcherContents = Get-Content -Raw -LiteralPath $launcher
if (!$launcherContents.Contains('"' + $executableName + '"') -or
    $launcherContents.Contains('--config') -or
    $launcherContents.Contains('--editor')) {
    throw "Editor launcher must start directly, without demo config flags."
}
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
if ($manifest.config -ne '' -or $manifest.executable -ne $executableName) {
    throw "Editor package manifest incorrectly references a config file."
}
Write-Host "Editor Project Hub packaging smoke test passed: $packagePath"
