[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$EditorPackage,
    [Parameter(Mandatory=$true)][string]$BuildDir,
    [string]$RepoRoot = (Join-Path $PSScriptRoot "..")
)
$ErrorActionPreference="Stop"
$editor=[IO.Path]::GetFullPath($EditorPackage)
$build=[IO.Path]::GetFullPath($BuildDir)
$repo=[IO.Path]::GetFullPath($RepoRoot)
if (!(Test-Path -LiteralPath (Join-Path $editor "SimpleEngineEditor.exe") -PathType Leaf)) {
    throw "Not an editor release folder: $editor"
}
foreach($name in @("SimpleEnginePlayer.exe","simpleengine_project.exe")) {
    $source=Join-Path $build $name
    if(!(Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Missing required developer tool: $source"
    }
    Copy-Item -LiteralPath $source -Destination $editor -Force
}
New-Item -ItemType Directory -Path (Join-Path $editor "tools") -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repo "tools/export-project.ps1") -Destination (Join-Path $editor "tools/export-project.ps1") -Force
New-Item -ItemType Directory -Path (Join-Path $editor "third_party_licenses") -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repo "third_party_licenses/Lua.txt") -Destination (Join-Path $editor "third_party_licenses/Lua.txt") -Force
# All runtime DLLs must remain beside the binaries. package.ps1 already
# copies them; this check guards missing Lua/MinGW redistributables.
if(@(Get-ChildItem -LiteralPath $editor -Filter "*.dll" -File).Count -eq 0) {
    throw "Editor package has no runtime DLLs."
}
Write-Host "Prepared portable editor and game export toolkit: $editor"
