const fs = require('node:fs');
const path = require('node:path');
const {CPU, avrInstruction, AVRUSART, usart0Config, AVRTimer, timer0Config}
    = require(process.env.AVR8JS_PATH || 'avr8js');
const bytes = new Uint8Array(32768);
let upper = 0;
for (const line of fs.readFileSync(process.argv[2], 'utf8').trim().split(/\r?\n/)) {
    if (!line.startsWith(':')) continue;
    const n = parseInt(line.slice(1,3),16), at = parseInt(line.slice(3,7),16);
    const type = parseInt(line.slice(7,9),16);
    const record = Buffer.from(line.slice(1), 'hex');
    if ([...record].reduce((sum,x)=>(sum+x)&255,0)) throw Error('Bad HEX checksum');
    if (type === 4) upper = parseInt(line.slice(9,13),16) * 65536;
    if (type === 0)
        for (let i=0;i<n;i++) bytes[upper+at+i] = parseInt(line.slice(9+2*i,11+2*i),16);
}
const cpu = new CPU(new Uint16Array(bytes.buffer), 2048);
new AVRTimer(cpu, timer0Config);
const uart = new AVRUSART(cpu, usart0Config, 16000000);
let done = false, pass = false;
uart.onLineTransmit = line => {
    console.log(line.trim());
    if (line.includes('TESTS PASS')) { done = true; pass = true; }
    if (line.includes('TESTS FAIL')) done = true;
};
while (!done && cpu.cycles < 160000000) { avrInstruction(cpu); cpu.tick(); }
if (!done) console.error('Emulator timed out at PC', cpu.pc);
process.exit(pass ? 0 : 1);

