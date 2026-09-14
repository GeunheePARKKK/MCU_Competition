const fs=require('node:fs');
const assert=require('node:assert/strict');
const {CPU,avrInstruction,AVRIOPort,portBConfig,portCConfig,portDConfig,
    AVRTimer,timer0Config,timer2Config,AVRUSART,usart0Config}=require(process.env.AVR8JS_PATH||'avr8js');
const bytes=new Uint8Array(32768);
let upper=0;
for(const line of fs.readFileSync(process.argv[2],'utf8').trim().split(/\r?\n/)) {
    const r=Buffer.from(line.slice(1),'hex');
    assert.equal([...r].reduce((a,b)=>(a+b)&255,0),0);
    if(r[3]===4) upper=((r[4]<<8)|r[5])*65536;
    if(r[3]===0) bytes.set(r.subarray(4,4+r[0]),upper+(r[1]<<8)+r[2]);
}
const cpu=new CPU(new Uint16Array(bytes.buffer),2048);
const pb=new AVRIOPort(cpu,portBConfig), pc=new AVRIOPort(cpu,portCConfig), pd=new AVRIOPort(cpu,portDConfig);
new AVRTimer(cpu,timer0Config); new AVRTimer(cpu,timer2Config);
const uart=new AVRUSART(cpu,usart0Config,16000000);
let shifted=0,latched=null,done=false,frames=0,edges=0,previousEdges=0,previousHz=0;
pc.addListener((value,old)=>{
    if(!(old&2)&&(value&2)) shifted=((shifted<<1)|(value&1))&65535;
    if(!(old&4)&&(value&4)) latched=shifted;
});
pd.addListener((value,old)=>{
    if((value^old)&64) ++edges;
    if((cpu.data[portDConfig.DDR]&8)&&!(value&8)) assert.notEqual(latched,null);
});
pb.addListener(value=>assert.equal(value&6,0,'Servo pins must not receive pulses'));
uart.onLineTransmit=line=>{
    line=line.trim();
    if(line==='BOOT') { assert.equal(latched,0x0000); assert.equal(pd.pinState(3),0); }
    if(line.startsWith('FRAME ')) {
        const [,word,hz]=line.split(' ');
        assert.equal(latched,parseInt(word,16),'U1/U2 serial order or output mismatch');
        assert.equal(pd.pinState(3),0,'Timer2 tone must not change /OE on D3');
        if(previousHz) assert.ok(edges-previousEdges>20,'Missing D6 tone transitions');
        if(Number(hz)===0) assert.equal(pd.pinState(6),0);
        previousEdges=edges; previousHz=Number(hz); ++frames;
    }
    if(line==='GPIO DONE') {
        assert.equal(frames,19); assert.equal(latched,0x0000);
        assert.equal(pd.pinState(6),0); assert.ok(edges>100);
        console.log(`GPIO TESTS PASS: ${frames} frames, real 595 clocks/latch, Timer2 D6, /OE D3 stable`);
        done=true;
    }
};
while(!done && cpu.cycles<160000000) { avrInstruction(cpu); cpu.tick(); }
assert.ok(done,'Emulator timeout');
