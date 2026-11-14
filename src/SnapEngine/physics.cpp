#include "physics.hpp"
#include "Jolt/Jolt.h" // IWYU pragma: keep

#include "Jolt/Core/Factory.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Core/Memory.h"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Physics/PhysicsSettings.h"
#include "Jolt/RegisterTypes.h"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "physicsSetup.hpp"
#include <cassert>

namespace Engine::Physics {

JPH::TempAllocatorImpl *GlobalPhysicsData::TemporaryAllocator = nullptr;
JPH::JobSystemThreadPool *GlobalPhysicsData::JobSystemThreadPool = nullptr;

auto Initialize() -> Error {
  JPH::RegisterDefaultAllocator();
  PrintInfo("Registered default allocator");

  // NOLINTBEGIN(cppcoreguidelines-owning-memory)
  JPH::Factory::sInstance = new JPH::Factory();

  GlobalPhysicsData::TemporaryAllocator = new JPH::TempAllocatorImpl(10UL * 1024 * 1024);

  int threadCount = static_cast<int>(JPH::thread::hardware_concurrency()) - 1;
  GlobalPhysicsData::JobSystemThreadPool = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, threadCount);
  //NOLINTEND(cppcoreguidelines-owning-memory)

  JPH::RegisterTypes();

  return {};
}

auto DeInitialize() -> void {
  JPH::UnregisterTypes();

  // NOLINTBEGIN(cppcoreguidelines-owning-memory)
  delete JPH::Factory::sInstance;
  delete GlobalPhysicsData::TemporaryAllocator;
  delete GlobalPhysicsData::JobSystemThreadPool;

  //NOLINTEND(cppcoreguidelines-owning-memory)
  JPH::Factory::sInstance = nullptr;
}

} // namespace Engine::Physics