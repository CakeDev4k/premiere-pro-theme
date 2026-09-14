# Builds premiere-pro-theme.wh.cpp with the compiler Windhawk ships, the way
# Windhawk builds a mod but with -Wall -Wextra, then builds and runs the logic
# tests in harness.cpp at -O2 and at -O0. Needs Windhawk and PowerShell 7.
#
#   pwsh tests/run.ps1

$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$compiler = 'C:\Program Files\Windhawk\Compiler'
$clang = Join-Path $compiler 'bin\clang++.exe'
$lib = Get-ChildItem 'C:\Program Files\Windhawk\Engine\*\64\windhawk.lib' |
    Sort-Object { $_.Directory.Parent.Name -as [version] } -Descending |
    Select-Object -First 1
$out = Join-Path ([IO.Path]::GetTempPath()) 'premiere-pro-theme-tests'
New-Item -ItemType Directory -Force $out | Out-Null

$defines = '-DUNICODE', '-D_UNICODE', '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00',
    '-D_WIN32_IE=0x0A00', '-DNTDDI_VERSION=0x0A000008',
    '-D__USE_MINGW_ANSI_STDIO=0', '-DWH_MOD'
$failed = $false

Push-Location $compiler
try {
    & $clang -std=c++23 -O2 -shared @defines '-DWH_MOD_ID=L"premiere-pro-theme"' `
        '-DWH_MOD_VERSION=L"1.0.0"' $lib.FullName `
        -x c++ (Join-Path $repo 'premiere-pro-theme.wh.cpp') -include windhawk_api.h `
        -target x86_64-w64-mingw32 '-Wl,--export-all-symbols' `
        -o (Join-Path $out 'premiere-pro-theme.dll') -ldwmapi -lgdi32 -Wall -Wextra

    if ($LASTEXITCODE) {
        Write-Host 'mod: build failed'
        $failed = $true
    } else {
        Write-Host 'mod: built'
    }

    foreach ($opt in '-O2', '-O0') {
        $exe = Join-Path $out "harness$opt.exe"

        & $clang -std=c++23 $opt @defines -DWH_EDITING -target x86_64-w64-mingw32 `
            (Join-Path $PSScriptRoot 'harness.cpp') -o $exe -static -ldwmapi -lgdi32 `
            -Wall -Wextra

        if ($LASTEXITCODE) {
            Write-Host "harness ${opt}: build failed"
            $failed = $true
            continue
        }

        $result = & $exe
        Write-Host "harness ${opt}: $result"

        if ($LASTEXITCODE) {
            $failed = $true
        }
    }
} finally {
    Pop-Location
}

if ($failed) {
    exit 1
}
