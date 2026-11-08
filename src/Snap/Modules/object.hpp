#pragma once

#include "Modules/type.hpp"
#include <atomic>

// #define DEBUG_OBJECT_LIFETIMES
// #define DEBUG_OBJECT_REFERENCES

#if defined(DEBUG_OBJECT_REFERENCES) || defined(DEBUG_OBJECT_LIFETIMES)
#include <unordered_map>
#endif

#ifdef DEBUG_OBJECT_LIFETIMES
#include <concepts>
#include <mutex>
#include <utility>

extern std::mutex RefCountsMutex;                                                             // NOLINT
extern std::unordered_map<void const *, std::pair<std::atomic<int>, const char *>> RefCounts; // NOLINT

#endif

using ObjectID = uint64_t;

struct Identifiable {
public:
  [[nodiscard]] auto getID() const -> ObjectID;

  auto operator==(const Identifiable &other) const -> bool { return id == other.id; }

private:
  static inline std::atomic<ObjectID> globalIDCounter{0};
  ObjectID id = globalIDCounter.fetch_add(1UL, std::memory_order_relaxed);
};

class Object {
protected:
  Object() = default;

public:
#ifdef DEBUG_OBJECT_REFERENCES
  mutable std::unordered_map<void *, std::string> backtraceStrings; // NOLINT
#endif

  Object(const Object &) = delete;
  Object(Object &&) = delete;

  auto operator=(const Object &) -> Object & = delete;
  auto operator=(Object &&) -> Object & = delete;

  virtual ~Object() = 0;

  [[nodiscard]] virtual auto GetInstanceType() const -> Type const * = 0;

#ifdef DEBUG_OBJECT_REFERENCES
  void retain(void *parent = nullptr) const;
  auto release(void *parent = nullptr) -> bool;
#else
  void retain() const;
  auto release() -> bool;
#endif

  [[nodiscard]] auto getReferenceCount() const -> int;

private:
  mutable std::atomic<int> count{0};
};

template <typename T> class Ref {
public:
  using element_type = T;

  Ref() = default;
  explicit Ref(T *pointer) : ptr(pointer) {
    if (ptr != nullptr) {
#ifdef DEBUG_OBJECT_REFERENCES
      ptr->retain(this);
#else
      ptr->retain();
#endif
    }
  }

  // copy
  Ref(const Ref &reference) : ptr(reference.ptr) {
    if (ptr != nullptr) {
#ifdef DEBUG_OBJECT_REFERENCES
      ptr->retain(this);
#else
      ptr->retain();
#endif
    }
  }

  auto operator=(const Ref &reference) -> Ref & {
    if (this != &reference) {
      if (reference.ptr != nullptr) {
#ifdef DEBUG_OBJECT_REFERENCES
        reference.ptr->retain(this);
#else
        reference.ptr->retain();
#endif
      }

      if (ptr != nullptr) {
#ifdef DEBUG_OBJECT_REFERENCES
        ptr->release(this);
#else
        ptr->release();
#endif
      }

      ptr = reference.ptr;
    }
    return *this;
  }

  // move
  Ref(Ref &&reference) noexcept : ptr(reference.ptr) { reference.ptr = nullptr; }

  auto operator=(Ref &&reference) noexcept -> Ref & {
    if (this != &reference) {
      if (ptr != nullptr) {
#ifdef DEBUG_OBJECT_REFERENCES
        ptr->release(this);
#else
        ptr->release();
#endif
      }
      ptr = reference.ptr;
      reference.ptr = nullptr;
    }
    return *this;
  }

  explicit Ref(std::nullptr_t) : ptr(nullptr) {}

  auto operator=(std::nullptr_t) noexcept -> Ref & {
    if (ptr != nullptr) {
#ifdef DEBUG_OBJECT_REFERENCES
      ptr->release(this);
#else
      ptr->release();
#endif
      ptr = nullptr;
    }
    return *this;
  }

  ~Ref() {
    if (ptr != nullptr) {
#ifdef DEBUG_OBJECT_REFERENCES
      ptr->release(this);
#else
      ptr->release();
#endif
      ptr = nullptr;
    }
  }

  auto operator->() const -> T * { return ptr; }
  auto get() const -> T * { return ptr; }
  auto operator*() const -> T & { return *ptr; }
  [[nodiscard]] auto isValid() const -> bool { return ptr != nullptr; }

  auto reset() -> void {
    if (ptr != nullptr) {
#ifdef DEBUG_OBJECT_REFERENCES
      ptr->release(this);
#else
      ptr->release();
#endif
      ptr = nullptr;
    }
  }

  template <typename... Args> static auto Make(Args &&...args) -> Ref<T> {
#ifdef DEBUG_OBJECT_LIFETIMES
    static_assert(std::derived_from<T, Object>, "Ref<T>::Make can only be used with types derived from Object");
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    auto *object = new T(std::forward<Args>(args)...);
    {
      std::lock_guard<std::mutex> lock(RefCountsMutex);
      RefCounts.emplace(object, std::make_pair(0, T::GetType()->GetName().c_str()));
    }
    return Ref<T>(object);
#else
    return Ref<T>(new T(std::forward<Args>(args)...));
#endif
  }

  [[nodiscard]] static auto GetType() -> Type const * { return T::GetType(); }
  [[nodiscard]] auto GetInstanceType() const -> Type const * { return T::GetType(); }

  auto operator==(Ref<T> const &other) const -> bool { return ptr == other.ptr; }
  auto operator!=(Ref<T> const &other) const -> bool { return ptr != other.ptr; }

  explicit operator bool() const { return ptr != nullptr; }

  auto operator==(std::nullptr_t) const -> bool { return ptr == nullptr; }
  auto operator!=(std::nullptr_t) const -> bool { return ptr != nullptr; }

private:
  T *ptr = nullptr;
};

struct Proxy {
  const Type *type;
  Object *object;
};