$ErrorActionPreference = "Stop"
$cli = "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$root = $PSScriptRoot
$build = Join-Path $root "build"
New-Item -ItemType Directory -Force -Path $build | Out-Null

& $cli compile `
  --fqbn arduino:avr:mega:cpu=atmega2560 `
  --output-dir $build `
  --libraries (Join-Path $root "libraries") `
  --libraries "$env:LOCALAPPDATA\Arduino15\libraries" `
  --libraries "$env:USERPROFILE\Documents\Arduino\libraries" `
  $root
