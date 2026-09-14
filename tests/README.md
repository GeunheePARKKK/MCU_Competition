# 개발자용 모의 검사 — 보드 업로드 금지

처음 부품을 시험하는 팀원은 이 폴더를 건너뛰고 상위 폴더의 인계 요약을 읽는다. 검사용 무선 드라이버를 포함하므로 이 안의 스케치를 실물에 업로드하지 않는다.

기존 환경은 Arduino AVR 1.8.6, LiquidCrystal I2C 1.1.2, AVR8js 0.21.1이다. Arduino CLI와 Node.js는 별도 설치하고 명령을 실행할 수 있게 준비한다. 다음 명령은 **검증만 하며 업로드하지 않는다**.

전달본 루트에서 PowerShell로 실행한다.

```powershell
$projectDir = (Get-Location).Path
$checkDir = Join-Path $projectDir 'local_checks'
$uiInclude = (Join-Path $projectDir 'psd_sketch').Replace('\','/')

& '.\tests\sync_tests.ps1'
arduino-cli compile --fqbn arduino:avr:uno --build-path "$checkDir/fsm" '.\tests\fsm_test'
arduino-cli compile --fqbn arduino:avr:uno --build-property "compiler.cpp.extra_flags=-I$uiInclude" --build-path "$checkDir/ui" '.\tests\ui_test'
arduino-cli compile --fqbn arduino:avr:uno --build-property "compiler.cpp.extra_flags=-I$uiInclude" --build-path "$checkDir/ui_gpio" '.\tests\ui_gpio_test'

npm install --prefix "$checkDir/tools" --no-save avr8js@0.21.1
$env:AVR8JS_PATH = Join-Path $checkDir 'tools/node_modules/avr8js'
node '.\tests\run_avr.cjs' "$checkDir/fsm/fsm_test.ino.hex"
node '.\tests\run_avr.cjs' "$checkDir/ui/ui_test.ino.hex"
node '.\tests\run_ui_gpio.cjs' "$checkDir/ui_gpio/ui_gpio_test.ino.hex"
```

기존 기록: FSM 456개, UI 489개, 표시 GPIO 19프레임 통과. 이번 패키징에서는 소스 동일성과 ZIP 압축 해제를 검사했으며, 이 검사들을 다시 실행한 것은 아니다.

서보·모터와 홀센서 시험은 해당 스케치를 먼저 컴파일한 뒤 생성한 HEX로 검사한다.

```powershell
arduino-cli compile --fqbn arduino:avr:uno --build-path "$checkDir/train_actuator" '.\train_actuator_test'
arduino-cli compile --fqbn arduino:avr:uno --build-path "$checkDir/psd_actuator" '.\psd_actuator_test'
arduino-cli compile --fqbn arduino:avr:uno --build-path "$checkDir/hall" '.\train_hall_test'
node '.\tests\run_actuator_gpio.cjs' "$checkDir/train_actuator/train_actuator_test.ino.hex" train
node '.\tests\run_actuator_gpio.cjs' "$checkDir/psd_actuator/psd_actuator_test.ino.hex" psd
node '.\tests\run_dual_hall_gpio.cjs' "$checkDir/hall/train_hall_test.ino.hex"
```

모의 검사는 실제 자기장·무선 잡음·전원·문 위치·서보 각도·제동 거리를 검증하지 않는다. 예전 LED 극성의 배선 시험과 관련 검사는 전달본에서 제외했다.
