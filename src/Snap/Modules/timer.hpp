#pragma once

#include <cstdint>
namespace Timer {

// Amount of seconds to buffer for delta time averaging
const double DeltaTimeBuffering = 0.5;

const double NANO_TO_SECONDS = 1.0 / 1000000000.0;
const double SECONDS_TO_NANO = 1000000000.0;

const double MILLI_TO_SECONDS = 1.0 / 1000.0;
const double SECONDS_TO_MILLI = 1000.0;

const uint64_t BufferingTimeNS = static_cast<uint64_t>(DeltaTimeBuffering * SECONDS_TO_NANO);

struct TimerFrameInfo {
  uint64_t lastTime = 0;
  uint64_t deltaTime = 0;

  uint64_t accumulatedTime = 0;
  uint64_t sampleCount = 0;
  uint64_t lastAveragingTime = 0;

  uint64_t averageDeltaTime = 0;
};

auto GetFPS() -> double;
auto GetTime() -> double;
auto GetTimeNS() -> uint64_t;
auto GetDelta() -> double;
auto GetAverageDelta() -> double;
void Sleep(double seconds, bool precise = false);
void Step();
} // namespace Timer