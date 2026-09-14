// Execute actual Uno bench HEX; never upload to hardware.
const role = process.argv[3];
if (role === 'train') require('./run_train_button_gpio.cjs');
else if (role === 'psd') require('./run_psd_button_gpio.cjs');
else throw new Error('Usage: node run_actuator_gpio.cjs <hex> train|psd');
