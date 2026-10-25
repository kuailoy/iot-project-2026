#pragma once

#include <Arduino.h>

// ---------- Wiring for Seeed XIAO ESP32S3 ----------
//
//  XIAO pin      ->  Module
//  ----------------------------------------------------------
//  D8  (SCK)     ->  eInk Breakout Friend  SCK
//  D9  (MISO)    ->  eInk Breakout Friend  MISO
//  D10 (MOSI)    ->  eInk Breakout Friend  MOSI
//  D1            ->  eInk Breakout Friend  ECS
//  D2            ->  eInk Breakout Friend  D/C
//  D3            ->  eInk Breakout Friend  RST
//  D6            ->  eInk Breakout Friend  BUSY
//  3V3 / GND     ->  eInk Breakout Friend  VIN / GND
//
//  D4  (SDA)     ->  STEMMA QT: SCD-41 + rotary encoder (daisy-chained)
//  D5  (SCL)     ->  STEMMA QT: SCD-41 + rotary encoder
//
// The breakout's 32KB SRAM is too small for an 800x480 tri-color
// frame (2 x 48KB), so the ESP32-S3's own RAM is used instead
// and the SRCS pin is left unconnected.

#define EPD_CS    D1
#define EPD_DC    D2
#define EPD_RESET D3
#define EPD_BUSY  D6
#define SRAM_CS   -1

// I2C addresses
#define ENCODER_I2C_ADDR 0x36

// Rotary encoder breakout: seesaw pin of the push button
#define ENCODER_BUTTON_PIN 24
