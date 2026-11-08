#include "channel.hpp"

namespace Threading {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<Ref<Channel>> Channels{};

auto AddReferences(const LuaWrap::Data::LuaType &data) -> void {
  if (std::holds_alternative<Proxy>(data)) {
    const auto &proxy = std::get<Proxy>(data);
    if (proxy.object != nullptr) {
      proxy.object->retain();
    }
  } else if (std::holds_alternative<std::vector<LuaWrap::Data::LuaData>>(data)) {
    const auto &tableData = std::get<std::vector<LuaWrap::Data::LuaData>>(data);
    for (const auto &entry : tableData) {
      AddReferences(entry.key);
      AddReferences(entry.value);
    }
  }
}

auto RemoveReferences(const LuaWrap::Data::LuaType &data) -> void {
  if (std::holds_alternative<Proxy>(data)) {
    const auto &proxy = std::get<Proxy>(data);
    if (proxy.object != nullptr) {
      proxy.object->release();
    }
  } else if (std::holds_alternative<std::vector<LuaWrap::Data::LuaData>>(data)) {
    const auto &tableData = std::get<std::vector<LuaWrap::Data::LuaData>>(data);
    for (const auto &entry : tableData) {
      RemoveReferences(entry.key);
      RemoveReferences(entry.value);
    }
  }
}

auto UnloadChannelModule() -> void {
  for (auto &channel : Channels) {
    channel->DestroyImmediately();
  }

  Channels.clear();
}

auto Channel::Push(const LuaWrap::Data::LuaType &message) -> void {
  if (IsDestroyed()) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mutex);
    messages.push(message);
  }

  condition.notify_one();
}

auto Channel::Pop() -> std::optional<LuaWrap::Data::LuaType> {
  if (IsDestroyed()) {
    return LuaWrap::Data::LuaType{std::monostate{}};
  }

  std::lock_guard<std::mutex> lock(mutex);
  if (messages.empty()) {
    return LuaWrap::Data::LuaType{std::monostate{}};
  }
  LuaWrap::Data::LuaType message = messages.front();
  messages.pop();
  return message;
}

auto Channel::Peek() const -> std::optional<LuaWrap::Data::LuaType> {
  if (IsDestroyed()) {
    return LuaWrap::Data::LuaType{std::monostate{}};
  }

  std::lock_guard<std::mutex> lock(mutex);
  if (messages.empty()) {
    return LuaWrap::Data::LuaType{std::monostate{}};
  }
  return messages.front();
}

auto Channel::GetCount() const -> size_t {
  if (IsDestroyed()) {
    return 0;
  }

  std::lock_guard<std::mutex> lock(mutex);
  return messages.size();
}

auto Channel::Demand(double timeout) -> std::optional<LuaWrap::Data::LuaType> {
  std::unique_lock<std::mutex> lock(mutex);

  if (!condition.wait_for(lock, std::chrono::duration<double>(timeout), [this]() -> bool { return !messages.empty() || IsDestroyed(); })) {
    return LuaWrap::Data::LuaType{std::monostate{}};
  }

  if (IsDestroyed()) {
    return LuaWrap::Data::LuaType{std::monostate{}};
  }

  LuaWrap::Data::LuaType message = messages.front();
  messages.pop();
  return message;
}

auto Channel::Demand() -> std::optional<LuaWrap::Data::LuaType> {
  std::unique_lock<std::mutex> lock(mutex);

  condition.wait(lock, [this]() -> bool { return !messages.empty() || IsDestroyed(); });

  if (IsDestroyed()) {
    return LuaWrap::Data::LuaType{std::monostate{}};
  }

  LuaWrap::Data::LuaType message = messages.front();
  messages.pop();
  return message;
}

auto Channel::Clear() -> void {
  if (IsDestroyed()) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex);
  while (!messages.empty()) {
    // Remove references to the message being cleared
    ::Threading::RemoveReferences(messages.front());
    messages.pop();
  }
}

} // namespace Threading