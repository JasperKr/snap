#include "timer.hpp"
#include "Modules/console.hpp"
#include <SDL3/SDL_timer.h>
#include <cstdint>

namespace Timer {

auto GetTimerInfo() -> TimerFrameInfo & {
  static TimerFrameInfo timerInfo;
  return timerInfo;
}

inline auto NanoToSeconds(uint64_t nanoseconds) -> double { return static_cast<double>(nanoseconds) * NANO_TO_SECONDS; }

inline auto SecondsToNano(double seconds) -> uint64_t { return static_cast<uint64_t>(seconds * SECONDS_TO_NANO); }

inline auto MilliToSeconds(uint64_t milliseconds) -> double { return static_cast<double>(milliseconds) * MILLI_TO_SECONDS; }

inline auto SecondsToMilli(double seconds) -> uint64_t { return static_cast<uint64_t>(seconds * SECONDS_TO_MILLI); }

auto GetTime() -> double { return NanoToSeconds(SDL_GetTicksNS()); }
auto GetTimeNS() -> uint64_t { return SDL_GetTicksNS(); }
auto GetDelta() -> double { return NanoToSeconds(GetTimerInfo().deltaTime); }
auto GetAverageDelta() -> double { return NanoToSeconds(GetTimerInfo().averageDeltaTime); }
auto GetFPS() -> double {
  if (GetTimerInfo().averageDeltaTime == 0) {
    return 0.0;
  }

  return 1.0 / NanoToSeconds(GetTimerInfo().averageDeltaTime);
}
void Sleep(double seconds, bool precise) {
  auto sleepTime = SecondsToNano(seconds);

  if (precise) {
    SDL_DelayPrecise(sleepTime);
  } else {
    SDL_DelayNS(sleepTime);
  }
}

void Step() {
  uint64_t currentTime = SDL_GetTicksNS();
  TimerFrameInfo &timerInfo = GetTimerInfo();

  if (timerInfo.lastTime == 0) {
    timerInfo.lastTime = currentTime;
    timerInfo.deltaTime = 0;
    return;
  }

  timerInfo.deltaTime = currentTime - timerInfo.lastTime;
  timerInfo.lastTime = currentTime;

  // Store delta time for averaging
  timerInfo.accumulatedTime += timerInfo.deltaTime;
  timerInfo.sampleCount += 1;

  if ((currentTime - timerInfo.lastAveragingTime) >= BufferingTimeNS) {
    timerInfo.lastAveragingTime = currentTime;

    timerInfo.averageDeltaTime = timerInfo.accumulatedTime / timerInfo.sampleCount;

    timerInfo.sampleCount = 0;
    timerInfo.accumulatedTime = 0;
  }
}
} // namespace Timer