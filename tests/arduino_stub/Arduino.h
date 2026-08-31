#pragma once

#include <cstdint>
#include <cstring>
#include <deque>

using uint8_t = std::uint8_t;
using uint16_t = std::uint16_t;
using uint32_t = std::uint32_t;
constexpr int HEX = 16;

class HardwareSerial {
 public:
  int available() const { return static_cast<int>(bytes_.size()); }
  int read() {
    const uint8_t value = bytes_.front();
    bytes_.pop_front();
    return value;
  }
  void push(uint8_t value) { bytes_.push_back(value); }

 private:
  std::deque<uint8_t> bytes_;
};

class DebugSerial {
 public:
  template <typename T>
  void print(const T&) {}
  template <typename T>
  void print(const T&, int) {}
  void println() {}
};

extern DebugSerial Serial;
