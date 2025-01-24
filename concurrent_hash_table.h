#pragma once

#include <cmath>
#include <forward_list>
#include <functional>
#include <shared_mutex>
#include <thread>
#include <vector>

template <typename Key, typename Mapped, typename Hash = std::hash<Key>,
          typename Equal = std::equal_to<Key>>
class ConcurrentHashTable {
  struct Shard {
    mutable std::shared_mutex mutex;
    std::vector<std::forward_list<std::pair<const Key, Mapped>>> buckets;
    std::size_t num_elements = 0;
    // This is the maximum load factor.
    // The actual load factor is  `1.0 * num_elements / buckets.size()`.
    static constexpr auto max_load_factor = 0.75;

    Mapped* lookup(std::size_t hash, const Key&);
    const Mapped* lookup(std::size_t hash, const Key&) const;

    bool insert(std::size_t hash, Key, Mapped);
  };

  std::vector<Shard> shards;

 public:
  ConcurrentHashTable();

  // Return whether the key was found.
  bool lookup(Mapped& destination, const Key&) const;
  // Return a pointer to the element at the specified key, or return
  // `nullptr` if no such element exists.
  Mapped* lookup(const Key&);
  const Mapped* lookup(const Key&) const;

  // Return whether the insertion took place.
  bool insert(Key, Mapped);
};

inline unsigned nproc_or_default() {
  const unsigned nproc = std::thread::hardware_concurrency();
  return nproc ? nproc : 16;
}

// class ConcurrentHashTable<...>
// ------------------------------

template <typename Key, typename Mapped, typename Hash, typename Equal>
ConcurrentHashTable<Key, Mapped, Hash, Equal>::ConcurrentHashTable()
    : shards(nproc_or_default()) {}

// Return whether the key was found.
template <typename Key, typename Mapped, typename Hash, typename Equal>
bool ConcurrentHashTable<Key, Mapped, Hash, Equal>::lookup(
    Mapped& destination, const Key& key) const {
  const std::size_t hash = Hash{}(key);
  const Shard& shard = shards[hash % shards.size()];
  std::shared_lock<std::shared_mutex> lock{shard.mutex};
  if (const Mapped* const value = shard.lookup(hash, key)) {
    destination = *value;
    return true;
  }
  return false;
}

// Return a pointer to the element at the specified key, or return
// `nullptr` if no such element exists.
template <typename Key, typename Mapped, typename Hash, typename Equal>
Mapped* ConcurrentHashTable<Key, Mapped, Hash, Equal>::lookup(const Key& key) {
  const std::size_t hash = Hash{}(key);
  Shard& shard = shards[hash % shards.size()];
  std::shared_lock<std::shared_mutex> lock{shard.mutex};
  return shard.lookup(hash, key);
}

template <typename Key, typename Mapped, typename Hash, typename Equal>
const Mapped* ConcurrentHashTable<Key, Mapped, Hash, Equal>::lookup(
    const Key& key) const {
  const std::size_t hash = Hash{}(key);
  const Shard& shard = shards[hash % shards.size()];
  std::shared_lock<std::shared_mutex> lock{shard.mutex};
  return shard.lookup(hash, key);
}

template <typename Key, typename Mapped, typename Hash, typename Equal>
bool ConcurrentHashTable<Key, Mapped, Hash, Equal>::insert(Key key,
                                                           Mapped value) {
  const std::size_t hash = Hash{}(key);
  Shard& shard = shards[hash % shards.size()];
  std::lock_guard<std::shared_mutex> lock{shard.mutex};
  return shard.insert(hash, std::move(key), std::move(value));
}

// struct ConcurrentHashTable<...>::Shard
// --------------------------------------

template <typename Key, typename Mapped, typename Hash, typename Equal>
Mapped* ConcurrentHashTable<Key, Mapped, Hash, Equal>::Shard::lookup(
    std::size_t hash, const Key& key) {
  // Note: Our caller is holding a reader or a writer lock on `mutex`.
  if (buckets.empty()) {
    return nullptr;
  }

  auto& bucket = buckets[hash % buckets.size()];
  auto found = std::find_if(
      bucket.begin(), bucket.end(),
      [&](const auto& entry) { return Equal{}(entry.first, key); });

  if (found == bucket.end()) {
    return nullptr;
  }
  return &found->second;
}

template <typename Key, typename Mapped, typename Hash, typename Equal>
const Mapped* ConcurrentHashTable<Key, Mapped, Hash, Equal>::Shard::lookup(
    std::size_t hash, const Key& key) const {
  // Note: Our caller is holding a reader or a writer lock on `mutex`.
  return const_cast<Shard*>(this)->lookup(hash, key);
}

template <typename Key, typename Mapped, typename Hash, typename Equal>
bool ConcurrentHashTable<Key, Mapped, Hash, Equal>::Shard::insert(
    std::size_t hash, Key key, Mapped value) {
  // Note: Our caller is holding a writer lock on `mutex`.
  if (lookup(hash, key)) {
    return false;
  }

  // Do we need to rebucket?
  if (buckets.empty() ||
      double(num_elements + 1) / buckets.size() > max_load_factor) {
    // We need to rebucket.
    // How many buckets do we want? At least as many as is needed to satisfy
    // the `max_load_factor`, but also enough that amortized insertions are
    // constant-time.
    constexpr auto desired_growth_factor = 1.5;
    // TODO: Show the algebra for `min_growth_factor`.
    const double min_growth_factor =
        (num_elements + 1) / (buckets.size() * max_load_factor);
    const double growth_factor =
        std::max(min_growth_factor, desired_growth_factor);
    const std::size_t min_buckets = std::ceil(1 / max_load_factor);
    // TODO: We don't need all of `vector`'s bookkeeping. We could do with
    // `(unique_ptr<Bucket[]>, size_t)`. Not a big deal, though.
    decltype(buckets) new_buckets(
        buckets.empty() ? min_buckets : growth_factor * buckets.size());

    // Rebucket every element.
    for (auto& bucket : buckets) {
      while (!bucket.empty()) {
        const std::size_t hash = Hash{}(bucket.begin()->first);
        auto& new_bucket = new_buckets[hash % new_buckets.size()];
        new_bucket.splice_after(new_bucket.before_begin(), bucket,
                                bucket.before_begin());
      }
    }

    buckets = std::move(new_buckets);
  }

  // Insert the new element.
  auto& bucket = buckets[hash % buckets.size()];
  bucket.emplace_front(std::move(key), std::move(value));
  ++num_elements;
  return true;
}
