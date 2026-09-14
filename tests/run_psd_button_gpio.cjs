// Execute the actual Uno bench HEX. Never uploads to a real board.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const {CPU, avrInstruction, AVRIOPort, portBConfig, portCConfig, portDConfig,
  AVRTimer, timer0Config, timer1Config, timer2Config, AVRTWI, twiConfig,
  AVRUSART, usart0Config} = require(process.env.AVR8JS_PATH || 'avr8js');
const train = false;
// PSD one-button protocol; /OE HIGH and buzzer silent throughout.
const bytes = new Uint8Array(32768);
let upper = 0;
for (const line of fs.readFileSync(process.argv[2], 'utf8').trim().split(/\r?\n/)) {
  const r = Buffer.from(line.slice(1), 'hex');
  assert.equal([...r].reduce((a, b) => (a + b) & 255, 0), 0);
  if (r[3] === 4) upper = ((r[4] << 8) | r[5]) * 65536;
  if (r[3] === 0) bytes.set(r.subarray(4, 4 + r[0]), upper + (r[1] << 8) + r[2]);
}
const cpu = new CPU(new Uint16Array(bytes.buffer), 2048);
const pb = new AVRIOPort(cpu, portBConfig), pc = new AVRIOPort(cpu, portCConfig);
const pd = new AVRIOPort(cpu, portDConfig);
for (const config of [timer0Config, timer1Config, timer2Config]) new AVRTimer(cpu, config);
const uart = new AVRUSART(cpu, usart0Config, 16000000);
const twi = new AVRTWI(cpu, twiConfig, 16000000);
const lines = [], pulses = [[], [], []], rising = [null, null, null];
let jamLcd = false;
twi.eventHandler = {
  start() { twi.completeStart(); }, stop() { twi.completeStop(); },
  connectToSlave(a, w) { twi.completeConnect(a === 0x27 && w); },
  writeByte() { if (!jamLcd) twi.completeWrite(true); },
  readByte() { twi.completeRead(255); },
};
uart.onLineTransmit = line => lines.push(line.trim());
function record(i, high) {
  if (high) rising[i] = cpu.cycles;
  else if (rising[i] !== null) {
    pulses[i].push({start: rising[i], width: (cpu.cycles - rising[i]) / 16});
    rising[i] = null;
  }
}
pb.addListener((v, old) => {
  if ((v ^ old) & 2) record(0, !!(v & 2));
  if ((v ^ old) & 4) record(1, !!(v & 4));
});
pd.addListener((v, old) => {
  if (train && ((v ^ old) & 8)) record(2, !!(v & 8));
  if (!train && (cpu.data[portDConfig.DDR] & 8)) assert.ok(v & 8, 'PSD /OE must stay HIGH');
  if (cpu.data[portDConfig.DDR] & 128) assert.equal(v & 128, 0, 'Radio CE inactive');
  if (cpu.data[portDConfig.DDR] & 64) assert.equal(v & 64, 0, 'Buzzer silent');
});
function run(ms) {
  const end = cpu.cycles + ms * 16000;
  while (cpu.cycles < end) { avrInstruction(cpu); cpu.tick(); }
}
function off() {
  assert.equal(pb.pinState(1), 0); assert.equal(pb.pinState(2), 0);
  assert.equal(cpu.data[0x80] & 0xF0, 0, 'Servo compare outputs disconnected');
  assert.equal(pd.pinState(3), train ? 0 : 1);
  if (train) assert.equal(cpu.data[0xB0] & 0x30, 0, 'Motor compare disconnected');
}

const starts = () => lines.filter(x => x.startsWith('START ')).length;
const selections = () => lines.filter(x => x.startsWith('SELECT ')).length;
function release() { pd.setPin(2, true); run(120); off(); }
function hold() { pd.setPin(2, false); run(1100); }
function tap(ms = 120) { pd.setPin(2, false); run(ms); release(); }
pd.setPin(2, false); // held at boot must not authorize movement or selection
pd.setPin(4, true); pc.setPin(3, false); // old controls deliberately held: ignored
run(2400); off(); assert.equal(starts(), 0); assert.equal(selections(), 0);
release(); assert.equal(selections(), 0);
console.log('Boot-held P4 ignored; old KW11/A3 do not control outputs: PASS');

for (let index = 0; index < 6; ++index) {
  const before = pulses.map(p => p.length), previousStarts = starts();
  pd.setPin(2, false); run(900); off(); assert.equal(starts(), previousStarts);
  run(200); assert.equal(starts(), previousStarts + 1);
  assert.equal(lines.filter(x => x.startsWith('START ')).at(-1), 'START ' + index);
  run(1100); off(); // timed output must expire while still held
  run(400); off(); assert.equal(starts(), previousStarts + 1, 'No held repeat');
  const output = index < 3 ? 1 : index < 6 ? 0 : 2; // P16 then P15 then motor
  for (let ch = 0; ch < 3; ++ch) {
    const measured = pulses[ch].slice(before[ch]);
    if (ch !== output) assert.equal(measured.length, 0, 'Only selected actuator may pulse');
    else {
      assert.ok(measured.length > 20, 'Missing real timer output');
      const expected = index < 6 ? [1500, 1400, 1600][index % 3] : [80, 120, 160, 200][index - 6] * 4;
      const settled = measured.slice(2, -2);
      assert.ok(settled.every(p => Math.abs(p.width - expected) <= 4), 'Pulse width ' + index);
      const period = index < 6 ? 20000 : 1024;
      assert.ok(settled.slice(1).every((p,i) => Math.abs((p.start - settled[i].start) / 16 - period) <= 1));
      assert.ok(measured.every(p => p.width <= expected + 4), 'No oversized first/last pulse');
      assert.ok((measured.at(-1).start - measured[0].start) / 16000 <= (index < 6 ? 1000 : 700));
    }
  }
  const beforeRelease = selections();
  release(); assert.equal(selections(), beforeRelease, 'Long-press release must not select');
  const beforeTap = starts();
  tap(); assert.equal(starts(), beforeTap, 'Short press selects only');
  assert.equal(lines.filter(x => x.startsWith('SELECT ')).at(-1), 'SELECT ' + ((index + 1) % 6));
}
console.log('P16/P15 six entries, pulse widths, no motor menu, /OE HIGH, hold lease: PASS');

// Ignore chatter and ambiguous medium presses; do not turn a cancelled hold into a click.
const baselineSelect = selections(), baselineStart = starts();
tap(15); assert.equal(selections(), baselineSelect);
tap(600); assert.equal(selections(), baselineSelect);
tap(950); assert.equal(selections(), baselineSelect);
assert.equal(starts(), baselineStart);
console.log('Bounce, medium hold, release just before run threshold: PASS');

hold(); const afterRun = starts();
pd.setPin(2, true); run(15); off(); // immediate raw release, not debounced stop
pd.setPin(2, false); run(1600); off();
assert.equal(starts(), afterRun, 'Release bounce cannot restart the same long press');
release(); assert.equal(selections(), baselineSelect);
hold(); release(); assert.equal(starts(), afterRun + 1);
console.log('Early release stops; fresh release needed; no accidental selection after run: PASS');

// LCD timeout while active must not prevent stopping from the control button.
hold(); jamLcd = true; run(180); pd.setPin(2, true); run(25); off();
jamLcd = false; run(120);

// Foreground parked with interrupts enabled: first check raw release in ISR.
hold(); const parkedAt = 0x3F00, resumeAt = cpu.pc;
cpu.progMem[parkedAt] = 0xCFFF; cpu.pc = parkedAt;
pd.setPin(2, true); run(25); off();
cpu.pc = resumeAt; run(180); off();

// Foreground parked while held: output still expires by Timer1.
hold(); cpu.pc = parkedAt; run(1200); off();
console.log('LCD timeout, foreground-stall release and lease: PASS');
console.log('PSD ONE-BUTTON GPIO TESTS PASS; physical P4 control, no hardware upload');
