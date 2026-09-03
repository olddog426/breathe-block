// Minimal Arduino surface so the device UI code can be compiled and run on a
// host for verification. Not used on the ESP32.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define LOW 0
#define HIGH 1
#define INPUT_PULLUP 2
#define OUTPUT 1
struct HostSerial {
  void begin(unsigned long) {}
  template <typename... A> void printf(const char* f, A... a) { ::printf(f, a...); }
  void println(const char* s = "") { ::printf("%s\n", s); }
  int available() { return 0; }
  int read() { return -1; }
};
extern HostSerial Serial;
extern uint32_t hostNowMs;
inline uint32_t millis() { return hostNowMs; }
// Wall-clock, so blend timings reported on the host mean something (a host
// microsecond is of course not an ESP32 microsecond).
uint32_t hostMicros();
inline uint32_t micros() { return hostMicros(); }
inline void delay(uint32_t) {}
inline void pinMode(int, int) {}
inline int digitalRead(int) { return HIGH; }
class HardwareSerial { public: explicit HardwareSerial(int) {} };
