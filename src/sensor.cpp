// SHT40 driver for the PaperColor's onboard sensor.
//
// M5Unified has no SHT4x class, so this talks to the chip directly over the
// M5Unified I2C wrapper. The SHT4x protocol is minimal: write one command
// byte, wait for the conversion, then read 6 bytes (temp MSB/LSB/CRC,
// humidity MSB/LSB/CRC).

#include "sensor.h"

#include <M5Unified.h>

RoomSensor room;

static const uint8_t SHT40_ADDR = 0x44;
static const uint8_t SHT40_MEASURE_HIGH_PRECISION = 0xFD;  // ~8.2 ms conversion
static const uint32_t I2C_FREQ = 100000;

// Which bus the sensor was found on. PaperColor wires the SHT40 to the
// internal bus (shared with the RTC and power IC), but probe both so a
// Grove-attached SHT40 on another board would also work.
static m5::I2C_Class *bus = nullptr;

// CRC-8, polynomial 0x31, init 0xFF (Sensirion datasheet).
static uint8_t sht4xCrc(const uint8_t *data, size_t len) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

void sensorInit() {
  m5::I2C_Class *candidates[] = {&M5.In_I2C, &M5.Ex_I2C};
  for (auto *c : candidates) {
    if (!c->isEnabled()) continue;
    if (c->scanID(SHT40_ADDR, I2C_FREQ)) {
      bus = c;
      room.present = true;
      Serial.printf("[m5weather] SHT40 found on %s I2C (sda=%d scl=%d)\n",
                    c == &M5.In_I2C ? "internal" : "external",
                    c->getSDA(), c->getSCL());
      return;
    }
  }
  Serial.println("[m5weather] SHT40 not found; indoor readings disabled");
}

bool sensorRead() {
  room.valid = false;
  if (!bus) return false;

  if (!bus->start(SHT40_ADDR, false, I2C_FREQ) ||
      !bus->write(SHT40_MEASURE_HIGH_PRECISION)) {
    bus->stop();
    Serial.println("[m5weather] SHT40 command write failed");
    return false;
  }
  bus->stop();
  delay(12);  // high-precision conversion is 8.2 ms max; leave margin

  uint8_t buf[6];
  bool ok = bus->start(SHT40_ADDR, true, I2C_FREQ) &&
            bus->read(buf, sizeof(buf), true);
  bus->stop();
  if (!ok) {
    Serial.println("[m5weather] SHT40 read failed");
    return false;
  }
  if (sht4xCrc(buf, 2) != buf[2] || sht4xCrc(buf + 3, 2) != buf[5]) {
    Serial.println("[m5weather] SHT40 CRC mismatch");
    return false;
  }

  uint16_t rawT = (buf[0] << 8) | buf[1];
  uint16_t rawH = (buf[3] << 8) | buf[4];
  // Conversion formulas from the SHT4x datasheet.
  room.tempC = -45.0f + 175.0f * rawT / 65535.0f;
  float rh = -6.0f + 125.0f * rawH / 65535.0f;
  room.humidity = rh < 0 ? 0 : (rh > 100 ? 100 : rh);
  room.readAtMs = millis();
  room.valid = true;
  Serial.printf("[m5weather] room: %.1f C, %.0f%% RH\n", room.tempC, room.humidity);
  return true;
}
