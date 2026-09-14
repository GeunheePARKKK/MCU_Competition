// Execute the compiled Uno HEX only; never uploads to hardware.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const {CPU, avrInstruction, AVRIOPort, portBConfig, portCConfig, portDConfig,
  AVRTimer, timer0Config, timer1Config, timer2Config, AVRADC, adcConfig,
  AVRTWI, twiConfig, AVRUSART, usart0Config} = require(process.env.AVR8JS_PATH || 'avr8js');
const bytes = new Uint8Array(32768);
let upper = 0;
for (const line of fs.readFileSync(process.argv[2], 'utf8').trim().split(/\r?\n/)) {
  const r = Buffer.from(line.slice(1), 'hex');
  assert.equal([...r].reduce((a,b) => (a+b)&255, 0), 0);
  if (r[3] === 4) upper = ((r[4]<<8)|r[5])*65536;
  if (r[3] === 0) bytes.set(r.subarray(4,4+r[0]), upper+(r[1]<<8)+r[2]);
}
const cpu = new CPU(new Uint16Array(bytes.buffer), 2048);
const pb = new AVRIOPort(cpu, portBConfig), pc = new AVRIOPort(cpu, portCConfig);
const pd = new AVRIOPort(cpu, portDConfig);
for (const cfg of [timer0Config,timer1Config,timer2Config]) new AVRTimer(cpu,cfg);
const adc = new AVRADC(cpu,adcConfig), uart = new AVRUSART(cpu,usart0Config,16000000);
const twi = new AVRTWI(cpu,twiConfig,16000000), lines = [];
uart.onLineTransmit = line => lines.push(line.trim());
pb.addListener(v => assert.equal(v & 6, 0, 'No servo pulses'));
pd.addListener(v => assert.equal(v & 0x88, 0, 'No motor output or radio CE'));

// Minimal HD44780 decoder for the installed PCF8574 LCD library.
let oldExpander=0, fourBit=false, highNibble=null, address=0, jamLcd=false;
const ddram = new Uint8Array(128).fill(32);
function lcdByte(data,rs) {
  if (rs) ddram[address++ & 127]=data;
  else if (data & 128) address=data & 127;
  else if (data === 1) { ddram.fill(32); address=0; }
  else if (data === 2) address=0;
}
function expander(value) {
  if ((oldExpander & 4) && !(value & 4)) {
    const nibble=value >>> 4, rs=value & 1;
    if (!fourBit) { if (!rs && nibble===2) fourBit=true; }
    else if (highNibble===null) highNibble=nibble;
    else { lcdByte((highNibble<<4)|nibble,rs); highNibble=null; }
  }
  oldExpander=value;
}
twi.eventHandler={
  start(){twi.completeStart();}, stop(){twi.completeStop();},
  connectToSlave(a,w){twi.completeConnect(a===0x27 && w);},
  writeByte(v){if(!jamLcd){expander(v);twi.completeWrite(true);}},
  readByte(){twi.completeRead(255);},
};
function run(ms) {
  const end=cpu.cycles+ms*16000;
  while(cpu.cycles<end){avrInstruction(cpu);cpu.tick();}
}
function off() {
  assert.equal(pb.pinState(1),0); assert.equal(pb.pinState(2),0);
  assert.equal(pd.pinState(3),0); assert.equal(pd.pinState(7),0);
  assert.equal(pb.pinState(0),1,'Radio CSN deselected');
  assert.equal(cpu.data[0x80]&0xF0,0,'Servo compare outputs disconnected');
  assert.equal(cpu.data[0xB0]&0x30,0,'Motor compare output disconnected');
  assert.equal(cpu.data[portCConfig.DDR]&3,0,'Both Hall inputs remain inputs');
  assert.equal(cpu.data[portCConfig.PORT]&3,0,'No Hall input pull-ups');
}
function values(h1,h2) {
  adc.channelValues[0]=(h1+0.25)*5/1024;
  adc.channelValues[1]=(h2+0.25)*5/1024;
}
function check(h1,h2,ms=800) {
  values(h1,h2);run(ms);off();
  assert.equal(lines.at(-1),`H1=${h1},H2=${h2}`,'Independent ADC values');
  for(const [row,value] of [h1,h2].entries()) {
    const actual=String.fromCharCode(...ddram.slice(row*64,row*64+16));
    const rail=value<=5 || value>=1018;
    const expected=`H${row+1} P${row+23} A=${String(value).padStart(4)}${rail?'!':' '}`.padEnd(16);
    assert.equal(actual,expected,'LCD row/pin labels, value and rail indication');
  }
}
check(508,512,2200);
check(508,555);check(508,462);check(508,512);
check(587,512);check(434,512);check(508,512);
check(600,400);check(0,1023);check(1023,0);check(508,512);
console.log('PASS: A0/A1 independent, both polarities, baseline return, rail markers, LCD rows');
// Physical pin 4 is not a control in this diagnostic.
pd.setPin(2,false);check(529,489);pd.setPin(2,true);check(508,512);
jamLcd=true;values(555,443);run(1000);off();
assert.equal(lines.at(-1),'H1=555,H2=443','ADC sampling continues after LCD timeout');
values(462,587);run(800);off();assert.equal(lines.at(-1),'H1=462,H2=587');
console.log('PASS: button ignored, LCD timeout bounded, motor/servos OFF and radio inactive throughout');
console.log('DUAL HALL TEST PASS (emulation only, no hardware upload)');
