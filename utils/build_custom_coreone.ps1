# Builds the personal CORE One firmware with the repository's pinned toolchain.
# Bootstrap utils/bootstrap.py first. No administrator rights are required.
[CmdletBinding()]
param([string]$VersionSuffix = '-custom+16383')

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repoPrefix = $repoRoot + [IO.Path]::DirectorySeparatorChar
$python = Join-Path $repoRoot '.venv/Scripts/python.exe'
if (!(Test-Path -LiteralPath $python -PathType Leaf)) {
    throw 'Run utils/bootstrap.py to create the local Python environment first.'
}

# Git on Windows may check out symlinks as small text files. Materialize only
# these known build inputs temporarily, preserving their exact original bytes.
$linkPaths = @(
    'src/can/data_types/uavcan',
    'src/can/data_types/reg',
    'lib/Prusa-Error-Codes/prusaerrors/buddy/errors.yaml',
    'lib/Prusa-Error-Codes/prusaerrors/mmu/errors.yaml',
    'lib/Prusa-Error-Codes/prusaerrors/sl1/errors.yaml'
)
$changed = [Collections.Generic.List[object]]::new()
$previousPath = $env:PATH
$previousUtf8 = $env:PYTHONUTF8
Push-Location -LiteralPath $repoRoot
try {
    $gettextBin = Join-Path $repoRoot '.dependencies/msys2-gettext/clang64/bin'
    if (Test-Path -LiteralPath (Join-Path $gettextBin 'msgfmt.exe')) {
        $env:PATH = $gettextBin + ';' + $env:PATH
    }
    if (!(Get-Command msgfmt -ErrorAction SilentlyContinue)) {
        throw 'Install GNU gettext (msgfmt) and add its bin directory to PATH.'
    }
    foreach ($relative in $linkPaths) {
        $path = [IO.Path]::GetFullPath((Join-Path $repoRoot $relative))
        $item = Get-Item -LiteralPath $path -Force
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
        if ($item.PSIsContainer) { throw "Expected a Git symlink or stub: $relative" }
        $tracked = git ls-files --stage -- $relative
        if ($LASTEXITCODE -ne 0 -or $tracked -notmatch '^120000 ') {
            throw "Not a tracked Git symlink: $relative"
        }
        $bytes = [IO.File]::ReadAllBytes($path)
        $link = [Text.Encoding]::UTF8.GetString($bytes).Trim()
        $target = [IO.Path]::GetFullPath((Join-Path (Split-Path $path -Parent) $link))
        if (!$path.StartsWith($repoPrefix, [StringComparison]::OrdinalIgnoreCase) -or
            !$target.StartsWith($repoPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Build link must stay inside the repository: $relative"
        }
        $targetItem = Get-Item -LiteralPath $target -Force
        $changed.Add(@{ Path = $path; Bytes = $bytes; Directory = $targetItem.PSIsContainer; Target = $target })
        Remove-Item -LiteralPath $path
        if ($targetItem.PSIsContainer) {
            New-Item -ItemType Junction -Path $path -Target $target | Out-Null
        } else {
            Copy-Item -LiteralPath $target -Destination $path
        }
    }
    $env:PATH = (Split-Path $python -Parent) + ';' + $env:PATH
    $env:PYTHONUTF8 = '1'
    & $python utils/build.py --preset coreone --build-type release --bootloader yes "--version-suffix=$VersionSuffix" --skip-bootstrap --no-store-output
    if ($LASTEXITCODE -ne 0) { throw "Firmware build failed (exit $LASTEXITCODE)." }
} finally {
    foreach ($entry in $changed) {
        if ($entry.Directory -and (Test-Path -LiteralPath $entry.Path)) {
            $item = Get-Item -LiteralPath $entry.Path -Force
            if (!$entry.Path.StartsWith($repoPrefix, [StringComparison]::OrdinalIgnoreCase) -or
                !($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
                [IO.Path]::GetFullPath([string]@($item.Target)[0]) -ne $entry.Target) {
                throw "Refusing to remove an unexpected build junction: $($entry.Path)"
            }
            # Remove the verified junction itself, never its target contents.
            [IO.Directory]::Delete($entry.Path, $false)
        }
        [IO.File]::WriteAllBytes($entry.Path, $entry.Bytes)
    }
    $env:PATH = $previousPath
    $env:PYTHONUTF8 = $previousUtf8
    Pop-Location
}
