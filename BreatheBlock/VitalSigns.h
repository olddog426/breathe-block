#pragma once

#include <stdint.h>

struct VitalSigns {
  float heartRate = 0.0f;
  float breathRate = 0.0f;
  float breathPhase = 0.0f;
  float distanceCm = 0.0f;
  bool presence = false;
  bool fresh = false;
  bool breathPhaseFresh = false;
  uint32_t observedAtMs = 0;
};
