#include <functional>
#include <list>
#include <type_traits>
#include <unordered_map>
#include <utility>

// Least Recently Used (LRU) Cache implementation.

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LRUCache {
public:
  using EvictionCallback = std::function<void(const Key &, Value &)>;

  LRUCache(const LRUCache &) = default;
  LRUCache(LRUCache &&) = default;
  auto operator=(const LRUCache &) -> LRUCache & = default;
  auto operator=(LRUCache &&) -> LRUCache & = default;
  ~LRUCache() = default;

  explicit LRUCache(size_t capacity, EvictionCallback onEvict = {})
      : m_capacity(capacity), m_onEvict(std::move(onEvict)) {}

  struct Entry {
    Key key;
    Value value;
  };

private:
  using iterator = typename std::list<Entry>::iterator;
  using const_iterator = typename std::list<Entry>::const_iterator;
  using ListIt = typename std::list<Entry>::iterator;

public:
  void setEvictionCallback(EvictionCallback onEvict) {
    m_onEvict = std::move(onEvict);
  }

  auto contains(const Key &key) const -> bool {
    return m_map.find(key) != m_map.end();
  }

  auto get(const Key &key) -> Value * {

    auto iterator = m_map.find(key);
    if (iterator == m_map.end()) {
      return nullptr;
    }
    touch(iterator);
    return &iterator->second->value;
  }

  auto operator[](const Key &key) -> Value & {
    auto iterator = m_map.find(key);
    if (iterator != m_map.end()) {
      touch(iterator);
      return iterator->second->value;
    }
    // insert new
    m_list.emplace_front(Entry{key, Value{}});
    m_map[key] = m_list.begin();
    if (m_map.size() > m_capacity) {
      evict();
    }
    return m_list.front().value;
  }

  // Callable version
  template <typename F>
  auto emplace(const Key &key, F &&constructor) -> Value &
    requires(std::is_invocable_v<F>)
  {
    auto iterator = m_map.find(key);
    if (iterator != m_map.end()) {
      touch(iterator);
      return iterator->second->value;
    }
    m_list.emplace_front(Entry{key, std::forward<F>(constructor)()});
    m_map[key] = m_list.begin();
    if (m_map.size() > m_capacity) {
      evict();
    }
    return m_list.front().value;
  }

  // Value version
  auto emplace(const Key &key, Value &&value) -> Value & {
    auto iterator = m_map.find(key);
    if (iterator != m_map.end()) {
      touch(iterator);
      return iterator->second->value;
    }
    m_list.emplace_front(Entry{key, std::move(value)});
    m_map[key] = m_list.begin();
    if (m_map.size() > m_capacity) {
      evict();
    }
    return m_list.front().value;
  }

  auto emplace(const Key &key, const Value &value) -> Value & {
    auto iterator = m_map.find(key);
    if (iterator != m_map.end()) {
      touch(iterator);
      return iterator->second->value;
    }
    m_list.emplace_front(Entry{key, value});
    m_map[key] = m_list.begin();
    if (m_map.size() > m_capacity) {
      evict();
    }
    return m_list.front().value;
  }

  void clear() {
    m_map.clear();
    m_list.clear();
  }

  [[nodiscard]] auto size() const -> size_t { return m_map.size(); }

  // Begin iterator (most recently used)
  auto begin() -> iterator { return m_list.begin(); }
  auto begin() const -> const_iterator { return m_list.begin(); }
  auto cbegin() const -> const_iterator { return m_list.cbegin(); }

  // End iterator
  auto end() -> iterator { return m_list.end(); }
  auto end() const -> const_iterator { return m_list.end(); }
  auto cend() const -> const_iterator { return m_list.cend(); }

private:
  void
  touch(typename std::unordered_map<Key, ListIt, Hash>::iterator iterator) {
    // move to front (most recently used)
    m_list.splice(m_list.begin(), m_list, iterator->second);
    iterator->second = m_list.begin();
  }

  void evict() {
    auto &back = m_list.back();
    if (m_onEvict) {
      m_onEvict(back.key, back.value);
    }
    m_map.erase(back.key);
    m_list.pop_back();
  }

  size_t m_capacity;
  std::list<Entry> m_list; // MRU front, LRU back
  std::unordered_map<Key, ListIt, Hash> m_map;
  EvictionCallback m_onEvict;
};