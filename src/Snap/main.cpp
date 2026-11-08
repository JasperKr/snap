#include "Modules/console.hpp"
#include "Modules/errorhandler.hpp"
#include "loop.hpp"
#include <public/client/TracyProfiler.hpp>
#include <public/tracy/Tracy.hpp>
#include <string>
#include <vector>

#include "Modules/event.hpp"

auto main(int argCount, char **argValues) -> int {
  std::vector<std::string> args;
  args.reserve(argCount);

  // Skip the first argument (program name)
  for (int i = 1; i < argCount; ++i) {
    args.emplace_back(argValues[i]); // NOLINT
  }

#ifdef TRACY_WAIT_FOR_CLIENT
  // Wait for the Tracy client to connect before starting the main loop
  PrintInfo("Waiting for Tracy client to connect...");
  while (!tracy::GetProfiler().IsConnected()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  PrintInfo("Tracy client connected. Starting main loop.");
#endif

  auto err = MainLoop(args);
  if (Error::IsError(err)) {
    PrintFatal(ColorText(err.message, ConsoleColor::Reset) + "\nCode: " + ColorText(std::to_string(err.code), ConsoleColor::Green) + "\nTraceback:\n" +
               ColorText(err.backtrace, ConsoleColor::Cyan) + "\n");

    return Event::ExitCode != 0 ? Event::ExitCode : err.code;
  }

  return Event::ExitCode;
}