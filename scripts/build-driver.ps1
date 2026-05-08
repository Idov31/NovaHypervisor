param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

# Some shells can inherit both Path and PATH. MSBuild/CL tasks use a
# case-insensitive environment map and fail when both names are present.
[Environment]::SetEnvironmentVariable("PATH", $null, "Process")

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$projectPath = Join-Path $repoRoot "NovaHypervisor\NovaHypervisor.vcxproj"
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

$msbuild = $null
if (Test-Path -LiteralPath $vsWhere) {
    $installPath = & $vsWhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    if ($installPath) {
        $candidate = Join-Path $installPath "MSBuild\Current\Bin\amd64\MSBuild.exe"
        if (Test-Path -LiteralPath $candidate) {
            $msbuild = $candidate
        }
    }
}

if (-not $msbuild) {
    $candidate = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
    if (Test-Path -LiteralPath $candidate) {
        $msbuild = $candidate
    }
}

if (-not $msbuild) {
    throw "Could not locate a Visual Studio MSBuild installation."
}

& $msbuild $projectPath "/p:Configuration=$Configuration" "/p:Platform=$Platform"
exit $LASTEXITCODE
