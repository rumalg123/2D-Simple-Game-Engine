# Builds a portable Windows game using the existing compiled editor-free player.
# A game ZIP contains only the player, its DLLs, approved project assets,
# a relative project manifest, a player-friendly launcher, and file hashes.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$ProjectPath,
    [string]$BuildDir = "build",
    [string]$OutputDir = "dist/project-exports",
    [string]$PackageName = "",
    [switch]$SkipBuild,
    [switch]$NoZip,
    [switch]$UseImportedTextures
)
$ErrorActionPreference = "Stop"
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
function Abs([string]$value) {
    if ([IO.Path]::IsPathRooted($value)) { return [IO.Path]::GetFullPath($value) }
    return [IO.Path]::GetFullPath((Join-Path $repo $value))
}
function Inside([string]$root,[string]$candidate) {
    $base=[IO.Path]::GetFullPath($root).TrimEnd('\','/')
    $child=[IO.Path]::GetFullPath($candidate)
    return $child.StartsWith($base+[IO.Path]::DirectorySeparatorChar,
                             [StringComparison]::OrdinalIgnoreCase)
}
function SafeRelative([string]$relative,[string]$root) {
    if ([string]::IsNullOrWhiteSpace($relative) -or [IO.Path]::IsPathRooted($relative)) {
        throw "Invalid project-relative path: $relative"
    }
    $candidate=[IO.Path]::GetFullPath((Join-Path $root $relative))
    if (!(Inside $root $candidate)) { throw "Path escapes project root: $relative" }
    return $candidate
}
function RequireOrdinaryFile([string]$path) {
    $item=Get-Item -LiteralPath $path -Force -ErrorAction Stop
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Symlinks/junctions are not allowed in exported game data: $path"
    }
    return $item
}
function FixTextureReferences([object]$node,[string]$root,[string]$assetRoot,[hashtable]$catalog,[hashtable]$compiled) {
    if ($null -eq $node) { return $false }
    $changed=$false
    if ($node -is [array]) {
        foreach($v in $node) { if (FixTextureReferences $v $root $assetRoot $catalog $compiled) { $changed=$true } }
    } elseif ($node -is [pscustomobject]) {
        foreach($property in $node.PSObject.Properties) {
            if ($property.Name -eq "texturePath" -and $property.Value -is [string] -and
                ![string]::IsNullOrWhiteSpace($property.Value)) {
                $nameProperty=$node.PSObject.Properties['textureName']
                $referenceName=if($nameProperty){[string]$nameProperty.Value}else{''}
                if($compiled.ContainsKey($referenceName)) {
                    $destination=[string]$compiled[$referenceName]
                    if([string]$property.Value -cne $destination) {
                        $property.Value=$destination
                        $changed=$true
                    }
                    continue
                }
                $original=[string]$property.Value
                if($nameProperty -and $catalog.ContainsKey([string]$nameProperty.Value)) {
                    # A managed move can leave old authoring texturePath strings,
                    # but the stable textureName asset ID points to the new file.
                    $original=[string]$catalog[[string]$nameProperty.Value]
                }
                if ([IO.Path]::IsPathRooted($original)) {
                    $full=[IO.Path]::GetFullPath($original)
                } else {
                    $full=SafeRelative $original $root
                }
                if (!(Inside $assetRoot $full) -or !(Test-Path -LiteralPath $full -PathType Leaf)) {
                    throw "Texture reference must be an existing project asset: $original"
                }
                $relative=$full.Substring($root.Length).TrimStart('\','/').Replace('\','/')
                if ([string]$property.Value -cne $relative) {
                    $property.Value=$relative
                    $changed=$true
                }
            } else {
                if (FixTextureReferences $property.Value $root $assetRoot $catalog $compiled) { $changed=$true }
            }
        }
    }
    return $changed
}

$projectRoot=(Resolve-Path -LiteralPath $ProjectPath -ErrorAction Stop).Path
$projectRoot=[IO.Path]::GetFullPath($projectRoot).TrimEnd('\','/')
$build=Abs $BuildDir
$output=Abs $OutputDir
if ((Inside $projectRoot $output) -or $projectRoot -eq $output) {
    throw "Export directory must be outside the source game project."
}
$manifestFile=Join-Path $projectRoot "project.simple.json"
if (!(Test-Path -LiteralPath $manifestFile -PathType Leaf)) {
    throw "Missing project.simple.json in $projectRoot"
}
[void](RequireOrdinaryFile $manifestFile)
$project=Get-Content -Raw -LiteralPath $manifestFile | ConvertFrom-Json
if ($project.formatVersion -ne 1) { throw "Unsupported game project manifest schema." }
$assetRelative=[string]$project.assetRoot
$assetRoot=SafeRelative $assetRelative $projectRoot
if (!(Test-Path -LiteralPath $assetRoot -PathType Container)) {
    throw "Project asset directory is missing: $assetRoot"
}
$sceneFile=SafeRelative ([string]$project.startupScene) $projectRoot
if (!(Inside $assetRoot $sceneFile) -or !(Test-Path -LiteralPath $sceneFile -PathType Leaf)) {
    throw "Startup scene must be an existing file within project assets."
}
$prefabRoot=SafeRelative ([string]$project.prefabDirectory) $projectRoot
if (!(Inside $assetRoot $prefabRoot)) {
    throw "Prefab folder must be inside project assets for portable builds."
}

# Validate before touching an existing output directory. Strict validation
# includes Lua compile checks and scene ScriptComponent references.
$projectCli=Join-Path $build "simpleengine_project.exe"
$playerExe=Join-Path $build "SimpleEnginePlayer.exe"
if (!$SkipBuild) {
    & cmake --build $build --target simpleengine_project SimpleEnginePlayer
    if ($LASTEXITCODE -ne 0) { throw "Building the runtime and validator failed." }
}
foreach ($binary in @($projectCli,$playerExe)) {
    if (!(Test-Path -LiteralPath $binary -PathType Leaf)) {
        throw "Missing built runtime or validator: $binary"
    }
}
& $projectCli validate $projectRoot
if ($LASTEXITCODE -ne 0) { throw "Project validation failed; game export stopped." }
if ($UseImportedTextures) {
    & $projectCli assets-build $projectRoot windows-x64
    if ($LASTEXITCODE -ne 0) {throw "Compiled asset import failed; export stopped."}
}

if (![string]::IsNullOrWhiteSpace($PackageName)) {
    $name=$PackageName
} else {
    $name=$projectRoot.Split([IO.Path]::DirectorySeparatorChar)[-1]
}
if ($name -notmatch '^[a-zA-Z0-9][a-zA-Z0-9 _-]{0,62}[a-zA-Z0-9]$' -and
    $name -notmatch '^[a-zA-Z0-9]$') {
    throw "Export name must be 1-64 letters, digits, spaces, _ or -, without leading/trailing punctuation."
}
$package=Join-Path $output $name
if (!(Inside $output $package) -or $package -eq $projectRoot) {
    throw "Invalid output folder."
}
# Reject reparse points from the source tree including every directory entry.
$assets=Get-ChildItem -LiteralPath $assetRoot -Force -Recurse
foreach ($item in @((Get-Item -LiteralPath $assetRoot -Force)) + @($assets)) {
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Game assets contain an unsafe symbolic link/junction: $($item.FullName)"
    }
}
# Only recognized runtime formats are allowed. Never ship source secrets,
# node_modules, .git folders, local environment files or editor caches.
$allowed=@(".lua",".ppm",".png",".jpg",".jpeg",".wav",".ogg",".mp3",
           ".json",".ini",".ttf",".otf",".bmp",".webp",".flac",".csv")
foreach ($item in $assets) {
    if ($item.Name.StartsWith(".") -or $item.Name -in @("node_modules","__pycache__",".git",".svn")) {
        throw "Hidden/cache paths are not allowed in game exports: $($item.FullName)"
    }
    if (!$item.PSIsContainer -and $item.Extension.ToLowerInvariant() -notin $allowed) {
        throw "Unrecognized runtime asset (review before exporting): $($item.FullName)"
    }
}
# Preflight texture references; avoid mutating the original project.
# Catalog IDs remain valid when the source asset has been moved/renamed.
# Resolve them to *current* relative paths before the old texturePath preflight.
$catalog=@{}
$catalogFile=Join-Path $assetRoot "asset_manifest.json"
if (Test-Path -LiteralPath $catalogFile -PathType Leaf) {
    $assetCatalog=Get-Content -Raw -LiteralPath $catalogFile | ConvertFrom-Json
    foreach($item in @($assetCatalog.assets)) {
        if ($item.id -and $item.path) {
            $catalog[[string]$item.id] = ($assetRelative.TrimEnd('/','\') + '/' +
                                           ([string]$item.path).Replace('\','/'))
        }
    }
}
$compiled=@{}
$compiledFiles=@{}
if($UseImportedTextures) {
    if(!(Test-Path -LiteralPath $catalogFile -PathType Leaf)) {
        throw "Compiled texture export requires an asset catalog."
    }
    $cacheDir=Join-Path $projectRoot ".simpleengine/cache/windows-x64"
    $mapFile=Join-Path $cacheDir "artifact-map.tsv"
    if(!(Test-Path -LiteralPath $mapFile -PathType Leaf)) {
        throw "Missing compiled artifact map; run assets-build."
    }
    $lines=@(Get-Content -LiteralPath $mapFile)
    if($lines.Count -lt 1 -or $lines[0] -ne "SEIMPORT1") {
        throw "Invalid compiled artifact map format."
    }
    $artifactIndex=@{}
    foreach($line in $lines | Select-Object -Skip 1) {
        $fields=@($line.Split("`t"))
        if($fields.Count -ne 4) {throw "Invalid imported artifact map row."}
        if($fields[3] -notmatch '^[0-9a-f]{32}-[0-9a-f]{16}`.se(img|blob)
$rewrites=@{}
foreach($file in $assetJson) {
    $parsed=Get-Content -Raw -LiteralPath $file.FullName | ConvertFrom-Json
    if (FixTextureReferences $parsed $projectRoot $assetRoot $catalog $compiled) {
        $rel=$file.FullName.Substring($assetRoot.Length).TrimStart('\','/')
        $rewrites[$rel]=($parsed | ConvertTo-Json -Depth 100)
    }
}

New-Item -ItemType Directory -Path $output -Force | Out-Null
if (Test-Path -LiteralPath $package) {
    throw "Export destination exists. Move/delete it first: $package"
}
$staging=Join-Path $output (".staging-" + [guid]::NewGuid().ToString("N"))
$zip=Join-Path $output ($name+".zip")
if ((Test-Path -LiteralPath $zip) -and !$NoZip) {
    throw "Export ZIP exists. Move/delete it first: $zip"
}
try {
    New-Item -ItemType Directory -Path $staging -Force | Out-Null
    $exeName=$name+".exe"
    Copy-Item -LiteralPath $playerExe -Destination (Join-Path $staging $exeName)
    Get-ChildItem -LiteralPath $build -Filter "*.dll" -File |
        ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $staging }
    $destinationAssets=Join-Path $staging $assetRelative
    New-Item -ItemType Directory -Path $destinationAssets -Force | Out-Null
    Get-ChildItem -LiteralPath $assetRoot -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $destinationAssets -Recurse -Force
    }
    if($UseImportedTextures) {
        foreach($portable in $compiledFiles.Keys) {
            $destination=Join-Path $staging $portable
            New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
            Copy-Item -LiteralPath $compiledFiles[$portable] -Destination $destination -Force
        }
    }
    foreach ($rel in $rewrites.Keys) {
        $target=Join-Path $destinationAssets $rel
        [IO.File]::WriteAllText($target,[string]$rewrites[$rel],
                                (New-Object System.Text.UTF8Encoding($false)))
    }

    # Players don't need the editor, project source or user-specific paths.
    $project.editorEnabled=$false
    $project.startPlaying=$true
    if ($project.PSObject.Properties.Name -contains "hotReload") {
        $project.hotReload=$false
    } else { $project | Add-Member -MemberType NoteProperty -Name hotReload -Value $false }
    [IO.File]::WriteAllText(
        (Join-Path $staging "project.simple.json"),
        ($project | ConvertTo-Json -Depth 100),
        (New-Object Text.UTF8Encoding($false)))

    $bat="@echo off" + [Environment]::NewLine +
         'pushd "%~dp0"' + [Environment]::NewLine +
         '"' + $exeName + '"' + [Environment]::NewLine +
         'popd' + [Environment]::NewLine
    [IO.File]::WriteAllText((Join-Path $staging "Play.bat"),$bat,
                            [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText((Join-Path $staging "PLAY.txt"),
        "Double-click $exeName or Play.bat to play. Game assets and Lua scripts must remain next to the executable.",
        [Text.Encoding]::UTF8)
    $luaLicense=Join-Path $repo "third_party_licenses/Lua.txt"
    if (Test-Path -LiteralPath $luaLicense) {
        New-Item -ItemType Directory -Path (Join-Path $staging "licenses") -Force | Out-Null
        Copy-Item -LiteralPath $luaLicense -Destination (Join-Path $staging "licenses/Lua.txt")
    }
    $files=Get-ChildItem -LiteralPath $staging -File -Recurse |
        Sort-Object { $_.FullName.Substring($staging.Length).Replace('\','/') }
    $inventory=@($files | ForEach-Object {
        [ordered]@{
            path=$_.FullName.Substring($staging.Length).TrimStart('\','/').Replace('\','/')
            size=$_.Length
            sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    })
    [ordered]@{
        formatVersion=1
        platform="windows-x64"
        game=$name
        executable=$exeName
        entrypoint="project.simple.json"
        assets=$assetRelative.Replace('\','/')
        files=$inventory
    } | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath (Join-Path $staging "export-manifest.json") -Encoding ASCII

    Move-Item -LiteralPath $staging -Destination $package
    if (!$NoZip) {
        Compress-Archive -Path (Join-Path $package "*") -DestinationPath $zip -Force
    }
    Write-Host "Game exported successfully: $package"
    if (!$NoZip) { Write-Host "Standalone ZIP: $zip" }
} finally {
    if (Test-Path -LiteralPath $staging) {
        Remove-Item -LiteralPath $staging -Recurse -Force
    }
}
) {
            throw "Invalid compiled cache filename."
        }
        $artifactIndex[$fields[0]]=@{guid=$fields[1];file=$fields[3]}
    }
    foreach($asset in @($assetCatalog.assets)) {
        if($asset.type -eq "texture") {
            $ref=[string]$asset.id
            if(!$artifactIndex.ContainsKey($ref) -or
               $artifactIndex[$ref].guid -ne [string]$asset.guid) {
                throw "Missing or mismatched cached texture: $ref"
            }
            $file=Join-Path $cacheDir $artifactIndex[$ref].file
            if(!(Test-Path -LiteralPath $file -PathType Leaf)) {
                throw "Compiled texture artifact is missing: $ref"
            }
            $portable="Assets/Imported/$($asset.guid).seimg"
            $compiled[$ref]=$portable
            $compiledFiles[$portable]=$file
        }
    }
}

$assetJson=$assets | Where-Object { !$_.PSIsContainer -and $_.Extension -eq ".json" }
$rewrites=@{}
foreach($file in $assetJson) {
    $parsed=Get-Content -Raw -LiteralPath $file.FullName | ConvertFrom-Json
    if (FixTextureReferences $parsed $projectRoot $assetRoot $catalog) {
        $rel=$file.FullName.Substring($assetRoot.Length).TrimStart('\','/')
        $rewrites[$rel]=($parsed | ConvertTo-Json -Depth 100)
    }
}

New-Item -ItemType Directory -Path $output -Force | Out-Null
if (Test-Path -LiteralPath $package) {
    throw "Export destination exists. Move/delete it first: $package"
}
$staging=Join-Path $output (".staging-" + [guid]::NewGuid().ToString("N"))
$zip=Join-Path $output ($name+".zip")
if ((Test-Path -LiteralPath $zip) -and !$NoZip) {
    throw "Export ZIP exists. Move/delete it first: $zip"
}
try {
    New-Item -ItemType Directory -Path $staging -Force | Out-Null
    $exeName=$name+".exe"
    Copy-Item -LiteralPath $playerExe -Destination (Join-Path $staging $exeName)
    Get-ChildItem -LiteralPath $build -Filter "*.dll" -File |
        ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $staging }
    $destinationAssets=Join-Path $staging $assetRelative
    New-Item -ItemType Directory -Path $destinationAssets -Force | Out-Null
    Get-ChildItem -LiteralPath $assetRoot -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $destinationAssets -Recurse -Force
    }
    foreach ($rel in $rewrites.Keys) {
        $target=Join-Path $destinationAssets $rel
        [IO.File]::WriteAllText($target,[string]$rewrites[$rel],
                                (New-Object System.Text.UTF8Encoding($false)))
    }

    # Players don't need the editor, project source or user-specific paths.
    $project.editorEnabled=$false
    $project.startPlaying=$true
    if ($project.PSObject.Properties.Name -contains "hotReload") {
        $project.hotReload=$false
    } else { $project | Add-Member -MemberType NoteProperty -Name hotReload -Value $false }
    [IO.File]::WriteAllText(
        (Join-Path $staging "project.simple.json"),
        ($project | ConvertTo-Json -Depth 100),
        (New-Object Text.UTF8Encoding($false)))

    $bat="@echo off" + [Environment]::NewLine +
         'pushd "%~dp0"' + [Environment]::NewLine +
         '"' + $exeName + '"' + [Environment]::NewLine +
         'popd' + [Environment]::NewLine
    [IO.File]::WriteAllText((Join-Path $staging "Play.bat"),$bat,
                            [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText((Join-Path $staging "PLAY.txt"),
        "Double-click $exeName or Play.bat to play. Game assets and Lua scripts must remain next to the executable.",
        [Text.Encoding]::UTF8)
    $luaLicense=Join-Path $repo "third_party_licenses/Lua.txt"
    if (Test-Path -LiteralPath $luaLicense) {
        New-Item -ItemType Directory -Path (Join-Path $staging "licenses") -Force | Out-Null
        Copy-Item -LiteralPath $luaLicense -Destination (Join-Path $staging "licenses/Lua.txt")
    }
    $files=Get-ChildItem -LiteralPath $staging -File -Recurse |
        Sort-Object { $_.FullName.Substring($staging.Length).Replace('\','/') }
    $inventory=@($files | ForEach-Object {
        [ordered]@{
            path=$_.FullName.Substring($staging.Length).TrimStart('\','/').Replace('\','/')
            size=$_.Length
            sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    })
    [ordered]@{
        formatVersion=1
        platform="windows-x64"
        game=$name
        executable=$exeName
        entrypoint="project.simple.json"
        assets=$assetRelative.Replace('\','/')
        files=$inventory
    } | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath (Join-Path $staging "export-manifest.json") -Encoding ASCII

    Move-Item -LiteralPath $staging -Destination $package
    if (!$NoZip) {
        Compress-Archive -Path (Join-Path $package "*") -DestinationPath $zip -Force
    }
    Write-Host "Game exported successfully: $package"
    if (!$NoZip) { Write-Host "Standalone ZIP: $zip" }
} finally {
    if (Test-Path -LiteralPath $staging) {
        Remove-Item -LiteralPath $staging -Recurse -Force
    }
}
