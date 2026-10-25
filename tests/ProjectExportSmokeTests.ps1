[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$RepoRoot,
    [Parameter(Mandatory=$true)][string]$BuildDir,
    [Parameter(Mandatory=$true)][string]$OutputDir
)
$ErrorActionPreference="Stop"
$cli=Join-Path $BuildDir "simpleengine_project.exe"
$exporter=Join-Path $RepoRoot "tools/export-project.ps1"
$projectParent=Join-Path $OutputDir "projects"
$products=Join-Path $OutputDir "exports"
if (Test-Path -LiteralPath $OutputDir) {
    Remove-Item -LiteralPath $OutputDir -Recurse -Force
}
New-Item -ItemType Directory -Path $projectParent -Force | Out-Null

& $cli create $projectParent "SmokeGame"
if($LASTEXITCODE -ne 0) { throw "Could not create export test game." }
$project=Join-Path $projectParent "SmokeGame"
& $exporter -ProjectPath $project -BuildDir $BuildDir -OutputDir $products -SkipBuild
$game=Join-Path $products "SmokeGame"
$exe=Join-Path $game "SmokeGame.exe"
$bundle=Join-Path $products "SmokeGame.zip"
foreach($path in @($exe,$bundle,(Join-Path $game "project.simple.json"),
                    (Join-Path $game "Assets/Scenes/Main.scene.json"),
                    (Join-Path $game "Assets/Scripts/WelcomeController.lua"),
                    (Join-Path $game "Play.bat"),
                    (Join-Path $game "licenses/Lua.txt"),
                    (Join-Path $game "export-manifest.json"))) {
    if(!(Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Export missing expected file: $path"
    }
}
$projectJSON=Get-Content -LiteralPath (Join-Path $game "project.simple.json") -Raw | ConvertFrom-Json
if($projectJSON.editorEnabled -or !$projectJSON.startPlaying -or
   $projectJSON.assetRoot -ne "Assets") {
    throw "Packaged project is not configured as a portable player."
}
if(@(Get-ChildItem -LiteralPath $game -Filter "*.dll" -File).Count -eq 0) {
    throw "Missing bundled MinGW/Lua runtime DLL dependencies."
}
$hashes=Get-Content -LiteralPath (Join-Path $game "export-manifest.json") -Raw | ConvertFrom-Json
if($hashes.platform -ne "windows-x64" -or $hashes.executable -ne "SmokeGame.exe" -or
   @($hashes.files).Count -lt 5) {
    throw "Incomplete game file inventory."
}
foreach($item in $hashes.files) {
    $file=Join-Path $game $item.path
    if(!(Test-Path -LiteralPath $file -PathType Leaf)) {throw "Inventory path missing: $file"}
    $calculated=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
    if($calculated -ne $item.sha256) {
        throw "Export inventory SHA256 does not match: $file"
    }
}
& $exe --verify-bundle
if($LASTEXITCODE -ne 0) {
    throw "Standalone game executable cannot validate itself headlessly."
}
& $cli validate $game
if($LASTEXITCODE -ne 0) {
    throw "Packaged scene and scripts are not loadable by engine CLI."
}
# Reject invalid scripts, preserving a previous working export ZIP.
$existingZipHash=(Get-FileHash -LiteralPath $bundle -Algorithm SHA256).Hash
$badLua=Join-Path $project "Assets/Scripts/Invalid.lua"
Set-Content -LiteralPath $badLua -Value "function broken( this is invalid" -Encoding ASCII
$badOutput=Join-Path $OutputDir "invalid-output"
& powershell -NoProfile -ExecutionPolicy Bypass -File $exporter -ProjectPath $project -BuildDir $BuildDir -OutputDir $badOutput -SkipBuild 2>&1 | Out-Null
if($LASTEXITCODE -eq 0) { throw "Invalid Lua project was exported." }
if(Test-Path -LiteralPath (Join-Path $badOutput "SmokeGame")) {
    throw "Invalid game exported partial files."
}
if((Get-FileHash -LiteralPath $bundle -Algorithm SHA256).Hash -ne $existingZipHash) {
    throw "A failed export corrupted the existing game ZIP."
}
Remove-Item -LiteralPath $badLua -Force
# Refuse arbitrary project files, including files commonly used for secrets.
Set-Content -LiteralPath (Join-Path $project "Assets/.env") -Value "SECRET=do-not-publish" -Encoding ASCII
& powershell -NoProfile -ExecutionPolicy Bypass -File $exporter -ProjectPath $project -BuildDir $BuildDir -OutputDir $badOutput -SkipBuild 2>&1 | Out-Null
if($LASTEXITCODE -eq 0) { throw "Exporter included a hidden secrets file." }
Write-Host "Portable export passed: standalone EXE, ZIP, Lua, asset hash inventory and failure isolation."
