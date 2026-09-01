#pragma once

#include <Arduino.h>

// Onboard SHT40 temperature/humidity sensor (room conditions).
struct RoomSensor {
  bool present = false;   // sensor answered on the I2C bus at boot
  bool valid = false;     // last read succeeded (CRC ok)
  float tempC = 0;
  float humidity = 0;     // percent RH
  unsigned long readAtMs = 0;
};

extern RoomSensor room;

// Probe the I2C buses for the SHT40. Call after M5.begin().
void sensorInit();

// Take a fresh measurement. Returns false (and leaves `room.valid` false)
// if the sensor is missing or the read failed.
bool sensorRead();
