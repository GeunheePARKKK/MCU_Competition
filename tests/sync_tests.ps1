$ErrorActionPreference = 'Stop'
$bbRoot = Split-Path -Parent $PSScriptRoot
$bbTestFolder = Join-Path $PSScriptRoot 'fsm_test'
foreach ($name in @('train_fsm.c','train_fsm.h','train_config.h','hall_detector.c',
    'hall_detector.h','protocol.h','packet.c','packet.h','comm.c','comm.h',
    'nrf24.h','radio_config.h','timebase.h','timebase.cpp','motor.c','motor.h',
    'debounce.c','debounce.h','door_toggle.c','door_toggle.h')) {
    Copy-Item -LiteralPath (Join-Path $bbRoot ('train_sketch\'+$name)) -Destination $bbTestFolder
}
foreach ($name in @('psd_fsm.c','psd_fsm.h','psd_config.h')) {
    Copy-Item -LiteralPath (Join-Path $bbRoot ('psd_sketch\'+$name)) -Destination $bbTestFolder
}
Write-Output 'Test source copies synchronized.'
