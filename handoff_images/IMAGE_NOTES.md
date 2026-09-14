# 회로 사진 이름표 제작 기록

최종 사용 파일: `train_annotated.png`, `psd_annotated.png`. 원본은 `train_original.png`, `psd_original.png`로 보존했다. 첫 생성본의 TRAIN 무선 모듈 및 PSD LED 화살표 위치를 보정한 뒤 최종 이미지를 확인했다. 사진의 미세 배선 검증은 수행하지 않았다.

방식: 내장 image_gen 이미지 편집. 원본 PNG는 그대로 보관하고, 입력 전송 오류를 피하기 위해 동일 크기 JPEG 사본을 사용했다. 아래는 각 사진에 사용한 프롬프트다. 이름표는 부품 식별용이며 실제 배선을 검증한 결과가 아니다.

## TRAIN

```text
Use case: precise-object-edit.
Asset type: Korean engineering handoff photo with restrained identification labels.
Primary request: Add ONLY a small title and clear Korean component-name callouts over the supplied real breadboard photograph. This is identification, NOT a verified wiring diagram.
Input image: the sole supplied photo is the EDIT TARGET, not a style reference.
Invariants: preserve the entire original photograph, viewpoint, aspect ratio, all exact wires, breadboard holes, real objects, and component positions. Do not rotate, crop, redraw, improve, remove, add, rearrange, light up or reconnect any circuitry. Overlay only labels and thin leader lines. No invented sensor or buttons.
Style: sober engineering notes, small white label boxes, charcoal Korean sans-serif lettering, thin muted navy leaders ending in a dot. Readable without oversized boxes. Use empty tabletop/background where possible, do not cover components. Do not add an electrical schematic.

Title (verbatim): "TRAIN · 열차 쪽"
Place these exact labels ONCE each, point leaders to these confirmed visible regions (normalized coordinates from top-left of original photograph):
"ATmega328P · 제어" -> black horizontal main DIP IC on center-upper breadboard at x46.5%, y40%.
"LCD · 상태 표시" -> long green LCD at lower-right x70%, y86%.
"서보 · 문/기구 구동" -> visible blue servo with white horn on clear tray at right x82.5%, y35%. Only one is clearly visible in photo; do not draw another or claim this visible one is a particular pin15/16.
"DC 모터 · 주행" -> yellow geared motor with large black wheel partly cut by lower edge, x46%, y93%.
"USBasp · 코드 업로드" -> blue board with silver USB plug at lower-left x28%, y82%.
"nRF24L01 · 무선 통신" -> vertical black wireless antenna board at upper-right edge of breadboard x68%, y23%.
"시험 버튼 · 칩 4번" -> square tactile pushbutton with round black center on lower-right breadboard x49%, y73%.
"멀티미터 · 측정 도구" -> large blue handheld meter at right x83%, y59%. It is external measurement equipment, not a project component.
Do NOT identify the exact location of a Hall sensor, MOSFET, other small parts, or individual servo channels: wiring obscures those.
Do not add extra explanatory paragraphs or claim test success on the photo.
Output a single annotated photo preserving the full source, in the same wide landscape orientation.
```

## 최종 화살표 보정

TRAIN의 무선 모듈과 PSD의 LED 화살표를 실제 부품 위치로 옮기도록 다음 수정 프롬프트를 추가했다.

### TRAIN

```text
undefined
```

### PSD

```text
undefined
```


## PSD

```text
Use case: precise-object-edit.
Asset type: Korean engineering handoff photo with restrained identification labels.
Primary request: Add ONLY a small title and clear Korean component-name callouts over the supplied real breadboard photograph. This is identification, NOT a verified wiring diagram.
Input image: the sole supplied photo is the EDIT TARGET, not a style reference.
Invariants: preserve the entire original photograph, viewpoint, aspect ratio, all exact wires, breadboard holes, real objects, and component positions. Do not rotate, crop, redraw, improve, remove, add, rearrange, light up or reconnect any circuitry. Overlay only labels and thin leader lines. No invented sensor or buttons.
Style: sober engineering notes, small white label boxes, charcoal Korean sans-serif lettering, thin muted navy leaders ending in a dot. Readable without oversized boxes. Use empty tabletop/background where possible, do not cover components. Do not add an electrical schematic.

Title (verbatim): "PSD · 승강장 쪽"
Place these exact labels ONCE each, point leaders to these confirmed visible regions (normalized coordinates from top-left of original photograph):
"LCD · 상태 표시" -> long green LCD at x27.5%, y39%.
"서보 2개" -> two blue servos on transparent tray at upper right, x68%, y10%; bracket both.
"74HC595 × 2 · 표시 확장" -> pair of horizontal black 16-pin ICs near upper-left of breadboard, centered x45.5%, y52%; bracket both.
"LED · 단계 표시" -> small blue LED cluster near x56.5%, y51%.
"7세그먼트 · 숫자 표시" -> black digit display x64%, y52.5%.
"ATmega328P · 제어" -> long black horizontal IC on LOWER breadboard, center x51%, y79.5%. NOT the two short 74HC595 on the upper board.
"USBasp · 코드 업로드" -> blue board with silver USB plug at x80%, y79%.
"nRF24L01 · 무선 통신" -> black antenna board hanging at lower-right corner x79.5%, y94.5%.
Do not label tiny ambiguous Hall sensors, buzzer or individual button roles. Do not add extra explanatory paragraphs or claim test success on the photo.
Output a single annotated photo preserving the full source, in the same wide landscape orientation.
```
