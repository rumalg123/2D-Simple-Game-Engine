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
# A game with a stable asset reference must export after its texture moves.
$textureFile=Join-Path $project "Assets/Sprites/hero.ppm"
Set-Content -LiteralPath $textureFile -Value "P3`n1 1`n255`n255 0 0" -Encoding ASCII
$sourceScene=Join-Path $project "Assets/Scenes/Main.scene.json"
$sceneSource=Get-Content -Raw -LiteralPath $sourceScene | ConvertFrom-Json
$sceneSource.entities[0] | Add-Member -MemberType NoteProperty -Name sprite -Value ([pscustomobject]@{
    halfWidth=0.1
    halfHeight=0.1
    textureName="texture:sprites/hero"
    texturePath="Assets/Sprites/hero.ppm"
})
[IO.File]::WriteAllText($sourceScene,($sceneSource | ConvertTo-Json -Depth 100),
    (New-Object Text.UTF8Encoding($false)))
& $cli assets-sync $project
if($LASTEXITCODE -ne 0) {throw "Could not index test texture asset."}
& $cli assets-move $project "texture:sprites/hero" "Sprites/hero-moved.ppm"
if($LASTEXITCODE -ne 0) {throw "Could not move indexed texture asset."}
$movedOutput=Join-Path $OutputDir "moved-exports"
& $exporter -ProjectPath $project -BuildDir $BuildDir -OutputDir $movedOutput -SkipBuild -NoZip
$movedGame=Join-Path $movedOutput "SmokeGame"
if(!(Test-Path (Join-Path $movedGame "Assets/Sprites/hero-moved.ppm"))) {
    throw "Renamed texture is missing from exported game."
}
$relocatedScene=Get-Content -Raw -LiteralPath (Join-Path $movedGame "Assets/Scenes/Main.scene.json") | ConvertFrom-Json
if($relocatedScene.entities[0].sprite.texturePath -ne "Assets/Sprites/hero-moved.ppm") {
    throw "Exporter failed to rewrite outdated texture path using catalog ID."
}
& (Join-Path $movedGame "SmokeGame.exe") --verify-bundle
if($LASTEXITCODE -ne 0) {throw "A game with a moved texture cannot validate its bundle."}

# Compiled export replaces shipped sprite paths with the versioned RGBA
# artifacts, but does not modify the source project or its saved scene.
$compiledOutput=Join-Path $OutputDir "compiled-exports"
& $exporter -ProjectPath $project -BuildDir $BuildDir -OutputDir $compiledOutput -SkipBuild -NoZip -UseImportedTextures
$compiledGame=Join-Path $compiledOutput "SmokeGame"
$compiledScene=Get-Content -Raw -LiteralPath (Join-Path $compiledGame "Assets/Scenes/Main.scene.json") | ConvertFrom-Json
$texturePath=[string]$compiledScene.entities[0].sprite.texturePath
if($texturePath -notmatch '^Assets/Imported/[a-fA-F0-9]{32}[.]seimg
$existingZipHash=(Get-FileHash -LiteralPath $bundle -Algorithm SHA256).Hash
$badLua=Join-Path $project "Assets/Scripts/Invalid.lua"
Set-Content -LiteralPath $badLua -Value "function broken( this is invalid" -Encoding ASCII
$badOutput=Join-Path $OutputDir "invalid-output"
# Native stderr is expected here: the child exporter must fail.
$ErrorActionPreference="Continue"
& powershell -NoProfile -ExecutionPolicy Bypass -File $exporter -ProjectPath $project -BuildDir $BuildDir -OutputDir $badOutput -SkipBuild 2>&1 | Out-Null
$negativeExit=$LASTEXITCODE
$ErrorActionPreference="Stop"
if($negativeExit -eq 0) { throw "Invalid Lua project was exported." }
if(Test-Path -LiteralPath (Join-Path $badOutput "SmokeGame")) {
    throw "Invalid game exported partial files."
}
if((Get-FileHash -LiteralPath $bundle -Algorithm SHA256).Hash -ne $existingZipHash) {
    throw "A failed export corrupted the existing game ZIP."
}
Remove-Item -LiteralPath $badLua -Force
# Refuse arbitrary project files, including files commonly used for secrets.
Set-Content -LiteralPath (Join-Path $project "Assets/.env") -Value "SECRET=do-not-publish" -Encoding ASCII
# Native stderr is expected here: the child exporter must fail.
$ErrorActionPreference="Continue"
& powershell -NoProfile -ExecutionPolicy Bypass -File $exporter -ProjectPath $project -BuildDir $BuildDir -OutputDir $badOutput -SkipBuild 2>&1 | Out-Null
$negativeExit=$LASTEXITCODE
$ErrorActionPreference="Stop"
if($negativeExit -eq 0) { throw "Exporter included a hidden secrets file." }
Write-Host "Portable export passed: standalone EXE, ZIP, Lua, asset hash inventory and failure isolation."
) {
    throw "Compiled export did not remap the catalog-backed sprite texture: $texturePath"
}
$compiledFile=Join-Path $compiledGame $texturePath
if(!(Test-Path -LiteralPath $compiledFile -PathType Leaf)) {
    throw "Compiled texture binary missing from the exported game."
}
$fileBytes=[IO.File]::ReadAllBytes($compiledFile)
if($fileBytes.Length -lt 20 -or
   [Text.Encoding]::ASCII.GetString($fileBytes,0,8) -ne "SEIMG001") {
    throw "Invalid binary texture format in exported game."
}
& (Join-Path $compiledGame "SmokeGame.exe") --verify-bundle
if($LASTEXITCODE -ne 0) {
    throw "Compiled artifact game is invalid for standalone player."
}
$original=Get-Content -Raw -LiteralPath $sourceScene | ConvertFrom-Json
if($original.entities[0].sprite.texturePath -ne "Assets/Sprites/hero.ppm") {
    throw "Export changed source authoring scene instead of only package scene."
}

# Reject invalid scripts, preserving a previous working export ZIP.
$existingZipHash=(Get-FileHash -LiteralPath $bundle -Algorithm SHA256).Hash
$badLua=Join-Path $project "Assets/Scripts/Invalid.lua"
Set-Content -LiteralPath $badLua -Value "function broken( this is invalid" -Encoding ASCII
$badOutput=Join-Path $OutputDir "invalid-output"
# Native stderr is expected here: the child exporter must fail.
$ErrorActionPreference="Continue"
& powershell -NoProfile -ExecutionPolicy Bypass -File $exporter -ProjectPath $project -BuildDir $BuildDir -OutputDir $badOutput -SkipBuild 2>&1 | Out-Null
$negativeExit=$LASTEXITCODE
$ErrorActionPreference="Stop"
if($negativeExit -eq 0) { throw "Invalid Lua project was exported." }
if(Test-Path -LiteralPath (Join-Path $badOutput "SmokeGame")) {
    throw "Invalid game exported partial files."
}
if((Get-FileHash -LiteralPath $bundle -Algorithm SHA256).Hash -ne $existingZipHash) {
    throw "A failed export corrupted the existing game ZIP."
}
Remove-Item -LiteralPath $badLua -Force
# Refuse arbitrary project files, including files commonly used for secrets.
Set-Content -LiteralPath (Join-Path $project "Assets/.env") -Value "SECRET=do-not-publish" -Encoding ASCII
# Native stderr is expected here: the child exporter must fail.
$ErrorActionPreference="Continue"
& powershell -NoProfile -ExecutionPolicy Bypass -File $exporter -ProjectPath $project -BuildDir $BuildDir -OutputDir $badOutput -SkipBuild 2>&1 | Out-Null
$negativeExit=$LASTEXITCODE
$ErrorActionPreference="Stop"
if($negativeExit -eq 0) { throw "Exporter included a hidden secrets file." }
Write-Host "Portable export passed: standalone EXE, ZIP, Lua, asset hash inventory and failure isolation."
