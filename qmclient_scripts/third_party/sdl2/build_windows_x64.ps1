[CmdletBinding()]
param(
    [string]$WorkDirectory,
    [switch]$Install
)

$ErrorActionPreference = "Stop"

$SdlVersion = "2.32.10"
$SdlCommit = "5d249570393f7a37e037abf22cd6012a4cc56a71"
$ScriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepositoryRoot = (Resolve-Path (Join-Path $ScriptDirectory "../../..")).Path
$PatchPath = Join-Path $ScriptDirectory "patches/0001-qm-skip-uiless-uielement-processing.diff"

if(-not $WorkDirectory) {
    $WorkDirectory = Join-Path $RepositoryRoot "cmake-build-sdl2-qm"
}
$SourceDirectory = Join-Path $WorkDirectory "source"
$BuildDirectory = Join-Path $WorkDirectory "build"

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Program,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & $Program @Arguments
    if($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE"
    }
}

New-Item -ItemType Directory -Force -Path $WorkDirectory | Out-Null
if(-not (Test-Path (Join-Path $SourceDirectory ".git"))) {
    Invoke-Checked "git" @("clone", "--filter=blob:none", "--no-checkout", "--single-branch", "--branch", "release-$SdlVersion", "https://github.com/libsdl-org/SDL.git", $SourceDirectory)
    Invoke-Checked "git" @("-C", $SourceDirectory, "checkout", "--detach", $SdlCommit)
}

$ActualCommit = (& git -C $SourceDirectory rev-parse HEAD).Trim()
if($LASTEXITCODE -ne 0 -or $ActualCommit -ne $SdlCommit) {
    throw "SDL source must be at $SdlCommit (release-$SdlVersion), found $ActualCommit"
}

$PreviousErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = "SilentlyContinue"
& git -C $SourceDirectory apply --check $PatchPath 2>$null
$PatchCanApply = $LASTEXITCODE -eq 0
$ErrorActionPreference = $PreviousErrorActionPreference
if($PatchCanApply) {
    Invoke-Checked "git" @("-C", $SourceDirectory, "apply", $PatchPath)
} else {
    $ErrorActionPreference = "SilentlyContinue"
    & git -C $SourceDirectory apply --reverse --check $PatchPath 2>$null
    $PatchIsApplied = $LASTEXITCODE -eq 0
    $ErrorActionPreference = $PreviousErrorActionPreference
    if(-not $PatchIsApplied) {
        throw "SDL source has changes that do not match the QmClient IME patch"
    }
}

Invoke-Checked "cmake" @("-S", $SourceDirectory, "-B", $BuildDirectory, "-G", "Visual Studio 17 2022", "-A", "x64", "-DSDL_SHARED=ON", "-DSDL_STATIC=OFF", "-DSDL_TESTS=OFF")
Invoke-Checked "cmake" @("--build", $BuildDirectory, "--config", "Release", "--target", "SDL2", "--parallel")

$BuiltDll = Join-Path $BuildDirectory "Release/SDL2.dll"
if(-not (Test-Path $BuiltDll)) {
    throw "SDL2 build completed without the expected Win64 DLL"
}

if($Install) {
    $Destination = Join-Path $RepositoryRoot "ddnet-libs/sdl/windows/lib64"
    Copy-Item -Force $BuiltDll (Join-Path $Destination "SDL2.dll")
}

$Hash = (Get-FileHash $BuiltDll -Algorithm SHA256).Hash
Write-Output "SDL $SdlVersion Win64: $BuiltDll"
Write-Output "SHA256: $Hash"
if($Install) {
    Write-Output "Installed SDL2.dll into ddnet-libs/sdl/windows/lib64"
}
