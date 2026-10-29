#pragma once

#include "EinkDisplay.h"

// Set -D EPD_BENCHMARK=1 in platformio.ini to run the benchmark instead of
// the normal app. Open the serial monitor (115200) to see the results.
#ifndef EPD_BENCHMARK
#define EPD_BENCHMARK 0
#endif

// Runs every mode / waveform / frame rate / temperature combination on test
// patterns, logs the timing of each refresh and prints a summary table.
// Takes several minutes: the stock and OTP refreshes are slow.
void runEinkBenchmark(EinkDisplay& display);
