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
$luaNotice = Join-Path $packagePath "licenses/Lua.txt"
if (!(Test-Path -LiteralPath $luaNotice)) { throw "Packaged editor must include Lua license." }
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
# The downloadable editor ZIP must include the player, project CLI and
# exporter so non-developers can build games from the editor UI.
& (Join-Path $RepoRoot "tools/bundle-export-toolkit.ps1") -EditorPackage $packagePath -BuildDir $BuildDir -RepoRoot $RepoRoot
foreach($required in @("SimpleEnginePlayer.exe", "simpleengine_project.exe",
                        "tools/export-project.ps1", "third_party_licenses/Lua.txt")) {
    if(!(Test-Path -LiteralPath (Join-Path $packagePath $required) -PathType Leaf)) {
        throw "Editor authoring/export toolkit is missing: $required"
    }
}

# The editor ZIP must export games *without* access to this source checkout.
$projectBase=Join-Path $OutputDir "toolkit_projects"
$exportRoot=Join-Path $OutputDir "toolkit_exports"
foreach ($folder in @($projectBase,$exportRoot)) {
    if (Test-Path -LiteralPath $folder) {
        Remove-Item -LiteralPath $folder -Recurse -Force
    }
}
New-Item -ItemType Directory -Path $projectBase -Force | Out-Null
& (Join-Path $packagePath "simpleengine_project.exe") create $projectBase "EditorBuiltGame"
if($LASTEXITCODE -ne 0) { throw "Packaged project CLI could not create a game." }
$sourceGame=Join-Path $projectBase "EditorBuiltGame"
& (Join-Path $packagePath "tools/export-project.ps1") -ProjectPath $sourceGame -BuildDir $packagePath -OutputDir $exportRoot -SkipBuild -NoZip
$exportedExe=Join-Path $exportRoot "EditorBuiltGame/EditorBuiltGame.exe"
if(!(Test-Path -LiteralPath $exportedExe -PathType Leaf)) {
    throw "Prebuilt editor toolkit did not export a playable EXE."
}
& $exportedExe --verify-bundle
if($LASTEXITCODE -ne 0) {
    throw "Prebuilt editor toolkit exported a broken game."
}

Write-Host "Editor Project Hub packaging smoke test passed: $packagePath"
