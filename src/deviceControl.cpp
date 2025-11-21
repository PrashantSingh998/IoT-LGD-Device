// deviceControl.cpp
#include "configControl.h"
#include "deviceControl.h"

#include <Arduino.h>
#include <HardwareSerial.h>
#include <vector>
#include <algorithm>

// Defaults (can be overwritten by NVS via updateTankParams)
static uint8_t  g_uart_port      = 2;
static int      g_tx_pin         = 17;
static int      g_rx_pin         = 16;
static uint32_t g_baud           = 9600;
static uint8_t  g_num_samples    = 3;   // small median smoothing for responsiveness

// Tank + sensor params (updated at runtime via updateTankParams)
static float    g_blind_cm       = 3.0f;
static float    g_tank_volume_l  = 500.0f;   // Vmax
static float    g_loft_height_cm = 48.8f;    // empty height

static HardwareSerial* g_uart = nullptr;

void initSensors() {
  // reserved
}

bool initUltrasonicSensor() {
  // Select UART instance by port; default to Serial2 for port=2 else Serial1
  g_uart = (g_uart_port == 2) ? &Serial2 : &Serial1;
  g_uart->begin(g_baud, SERIAL_8N1, g_rx_pin, g_tx_pin);
  g_uart->setTimeout(50);

  // flush any old bytes
  while (g_uart->available()) g_uart->read();

  Serial.printf("[ULTRA] port=%u rx=%d tx=%d baud=%lu samples=%u blind=%.2f\n",
                (unsigned)g_uart_port, g_rx_pin, g_tx_pin, (unsigned long)g_baud, (unsigned)g_num_samples, g_blind_cm);
  Serial.printf("[ULTRA] Vmax=%.2f L, loftH=%.2f cm\n", g_tank_volume_l, g_loft_height_cm);
  return true;
}

/*
  Sensor behaviour:
  - Many DYP-A02YYUW variants stream continuous 4-byte frames:
      [0xFF][HIGH][LOW][CHK]
    where distance_mm = (HIGH<<8 | LOW)
  - We must *sync* to 0xFF and then read the next 3 bytes. This code
    searches for header and validates checksum when available.
*/

static bool readDistanceOnce(float &cm) {
  if (!g_uart) return false;

  // search for header 0xFF within timeout
  uint32_t t0 = millis();
  const uint32_t header_timeout = 200; // ms
  int b = -1;

  while ((millis() - t0) < header_timeout) {
    if (g_uart->available()) {
      b = g_uart->read();
      if (b == 0xFF) break;
      // else continue scanning (discard byte)
    } else {
      delay(1);
    }
  }

  if (b != 0xFF) {
    // no header found
    return false;
  }

  // we saw header; read next 3 bytes (hi, lo, chk) but be tolerant
  uint8_t hi=0, lo=0, chk=0;
  if (g_uart->readBytes(&hi, 1) != 1) return false;
  if (g_uart->readBytes(&lo, 1) != 1) return false;

  // attempt to read a checksum byte if available very quickly
  uint32_t t1 = millis();
  while (!g_uart->available() && (millis() - t1) < 8) { /* wait small window */ }
  if (g_uart->available()) {
    g_uart->readBytes(&chk, 1);
  } else {
    chk = 0;
  }

  // If chk != 0, validate — but don't fail if mismatch (some modules vary)
  if (chk != 0) {
    uint8_t calc = (uint8_t)(0xFF + hi + lo);
    if (calc != chk) {
      // optional debug, but keep minimal to avoid spamming:
      Serial.printf("[ULTRA] checksum mismatch (ignored) %02X %02X %02X (calc %02X)\n", hi, lo, chk, calc);
      // continue — many modules do not match or use different scheme
    }
  }

  uint16_t mm = ((uint16_t)hi << 8) | lo;
  float dcm = mm / 10.0f;

  // sanity range check (sensor spec 3..450 cm typical; use 2..1000 for safety)
  if (dcm < 2.0f || dcm > 1000.0f) {
    return false;
  }

  cm = dcm;
  return true;
}

float averageDistanceCm() {
  std::vector<float> samples;
  samples.reserve(g_num_samples);

  for (uint8_t i = 0; i < g_num_samples; ++i) {
    float d;
    if (readDistanceOnce(d)) {
      samples.push_back(d);
    } else {
      // small wait and continue reading next frame - sensor streams at ~10Hz
      delay(30);
    }
  }

  if (samples.empty()) return NAN;

  std::sort(samples.begin(), samples.end());
  // median
  float median = samples[samples.size()/2];

  // keep last valid value if desired (optional)
  static float last_valid = NAN;
  if (!isnan(median)) last_valid = median;
  else if (!isnan(last_valid)) median = last_valid;

  return median;
}

float calculateVolumeLiters(float distance_cm) {
  float span = g_loft_height_cm - g_blind_cm;
  if (span <= 0.0f) {
    Serial.println("[VOL] Invalid calibration: loft_height must be > blind_spot");
    return 0.0f;
  }

  float d = distance_cm;
  if (d < g_blind_cm)       d = g_blind_cm;
  if (d > g_loft_height_cm) d = g_loft_height_cm;

  float m = -g_tank_volume_l / span;
  float c = g_tank_volume_l - (m * g_blind_cm);
  float vol = m * d + c;
  if (vol < 0.0f) vol = 0.0f;

  Serial.printf("[LinearMap] d=%.2f blind=%.2f maxH=%.2f -> %.2f L\n",
                distance_cm, g_blind_cm, g_loft_height_cm, vol);
  return vol;
}

void updateTankParams(float tank_volume_l, float loft_height_cm, float blind_spot_cm) {
  g_tank_volume_l  = tank_volume_l;
  g_loft_height_cm = loft_height_cm;
  g_blind_cm       = blind_spot_cm;
  Serial.printf("[TANK] Updated: Vmax=%.2f L, loftH=%.2f cm, blind=%.2f cm\n",
                g_tank_volume_l, g_loft_height_cm, g_blind_cm);
}
