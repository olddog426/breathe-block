#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

#include "LD6002.h"

DebugSerial Serial;

namespace {
uint8_t checksum(const uint8_t* data, int length) {
  uint8_t value = 0;
  for (int index = 0; index < length; ++index) value ^= data[index];
  return static_cast<uint8_t>(~value);
}

void appendFloat(std::vector<uint8_t>& frame, float value) {
  uint8_t bytes[sizeof(float)];
  std::memcpy(bytes, &value, sizeof(value));
  frame.insert(frame.end(), bytes, bytes + sizeof(value));
}
}  // namespace

int main() {
  HardwareSerial serial;
  LD6002 parser(serial);

  std::vector<uint8_t> frame = {
      0x01, 0x00, 0x01, 0x00, 0x0C, 0x0A, 0x13,
  };
  frame.push_back(checksum(frame.data(), 7));
  appendFloat(frame, -2.25f);  // Total phase.
  appendFloat(frame, 0.625f);  // Respiratory phase.
  appendFloat(frame, 0.01f);   // Heart phase.
  frame.push_back(checksum(frame.data() + 8, 12));

  for (uint8_t byte : frame) serial.push(byte);
  parser.update();

  assert(parser.hasNewBreathPhase());
  assert(std::fabs(parser.getBreathPhase() - 0.625f) < 0.0001f);
  parser.clearBreathPhaseFlag();
  assert(!parser.hasNewBreathPhase());

  std::cout << "LD6002 respiratory-phase test passed\n";
  return 0;
}
