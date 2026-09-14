// PSD ONLY: D3 holds the 74HC595 outputs disabled; no propulsion output.
// One button: DIP pin 4 (Arduino D2) -> button -> GND.
// Tap to select; hold 1 second to run; release stops. No A3/KW11 needed.
// This standalone test repurposes D2; the integrated PSD E-stop is unchanged.
#define BENCH_TRAIN 0
#include "actuator_bench.h"
