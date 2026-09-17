param(
    [string]$Compiler = "gcc"
)

$ErrorActionPreference = "Stop"
$TestRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$CodeRoot = Split-Path -Parent $TestRoot
$Output = Join-Path $TestRoot "fcw_unit_tests.exe"
$UcrtBin = "C:\msys64\ucrt64\bin"

# MSYS2版gccをWindows PowerShellから使う場合に必要な実行時DLLを見つける。
if (Test-Path -LiteralPath $UcrtBin)
{
    $env:Path = "$UcrtBin;$env:Path"
}

& $Compiler -std=c99 -Wall -Wextra `
    "-I$TestRoot\mock_include" "-I$CodeRoot" `
    (Join-Path $TestRoot "test_fcw_modules.c") `
    (Join-Path $CodeRoot "fcw_types.c") `
    (Join-Path $CodeRoot "avp_fcw_roi.c") `
    (Join-Path $CodeRoot "fcw_tracker.c") `
    (Join-Path $CodeRoot "fcw_ttc.c") `
    (Join-Path $CodeRoot "fcw_alert.c") `
    (Join-Path $CodeRoot "fcw_alarm.c") `
    -o $Output

& $Output
