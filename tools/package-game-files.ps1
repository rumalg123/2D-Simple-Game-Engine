[CmdletBinding()]
param(
    [string]$OutputDir = "dist"
)

$ErrorActionPreference = "Stop"
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$OutputPath = if ([System.IO.Path]::IsPathRooted($OutputDir)) {
    [System.IO.Path]::GetFullPath($OutputDir)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $OutputDir))
}

New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null

$StagingPath = Join-Path $OutputPath "GameFiles"
$ZipPath = Join-Path $OutputPath "GameFiles.zip"

if (Test-Path -LiteralPath $StagingPath) {
    Remove-Item -LiteralPath $StagingPath -Force -Recurse
}
New-Item -ItemType Directory -Path $StagingPath -Force | Out-Null

try {
    # Preserve original relative locations so developers can reuse configs/assets.
    $files = @(
        "assets"
        "demo.game.json"
        "samples/snake/README.md"
        "samples/snake/snake.game.json"
        "samples/snake/src"
        "samples/flappy_bird/README.md"
        "samples/flappy_bird/flappy_bird.game.json"
        "samples/flappy_bird/src"
        "templates/basic_game/README.md"
        "templates/basic_game/basic_game.game.json"
        "templates/basic_game/src"
    )

    foreach ($item in $files) {
        $relative = $item.Replace("/", [System.IO.Path]::DirectorySeparatorChar)
        $source = Join-Path $RepoRoot $relative
        if (!(Test-Path -LiteralPath $source)) {
            throw "Required game file missing: $source"
        }

        $destination = Join-Path $StagingPath $relative
        $parent = Split-Path -Parent $destination
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $destination -Recurse -Force
    }

    # Optional per-sample asset directories may be added in future revisions.
    foreach ($optional in @("samples/snake/assets", "samples/flappy_bird/assets", "templates/basic_game/assets")) {
        $relative = $optional.Replace("/", [System.IO.Path]::DirectorySeparatorChar)
        $source = Join-Path $RepoRoot $relative
        if (Test-Path -LiteralPath $source) {
            $destination = Join-Path $StagingPath $relative
            New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
            Copy-Item -LiteralPath $source -Destination $destination -Recurse -Force
        }
    }

    $help = @"
SimpleEngine game files and assets

This archive contains the original asset files, JSON game configurations,
sample game C++ sources, and the reusable basic game template source.

This is not a runnable Windows package. To play a game, download the
corresponding *-Windows-x64.zip archive from the same GitHub Release.
Executable ZIPs already contain their own required assets and DLLs.

The sample source projects depend on the engine source and CMake build
configuration from the full GitHub repository.
"@
    Set-Content -LiteralPath (Join-Path $StagingPath "README.txt") -Value $help -Encoding UTF8

    if (Test-Path -LiteralPath $ZipPath) {
        Remove-Item -LiteralPath $ZipPath -Force
    }
    Compress-Archive -Path (Join-Path $StagingPath "*") -DestinationPath $ZipPath -CompressionLevel Optimal
    if (!(Test-Path -LiteralPath $ZipPath)) {
        throw "Failed to generate $ZipPath"
    }
    Write-Host "Created game source/assets archive: $ZipPath"
}
finally {
    if (Test-Path -LiteralPath $StagingPath) {
        Remove-Item -LiteralPath $StagingPath -Recurse -Force
    }
}
