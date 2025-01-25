#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "concurrent_hash_table.h"

#ifndef BREATHING_ITERATIONS
#define BREATHING_ITERATIONS 1000
#endif

void test_breathing() {
  std::cout << "hello\n\n";
  ConcurrentHashTable<std::string, std::vector<int>> sequences;
  const std::vector<int> value = {1, 2, 3, 4, 5};
  constexpr auto iterations = BREATHING_ITERATIONS;
  constexpr auto num_workers = 100;

  std::atomic<unsigned long long> insert_ok = 0;
  std::atomic<unsigned long long> insert_no = 0;
  std::atomic<unsigned long long> lookup_ok = 0;
  std::atomic<unsigned long long> lookup_no = 0;

  const auto worker = [&](std::string insert_key, std::string lookup_key) {
    return [&, insert_key = std::move(insert_key),
            lookup_key = std::move(lookup_key)]() {
      std::vector<int> dummy;
      for (int i = 0; i < iterations; ++i) {
        sequences.insert(insert_key, value) ? ++insert_ok : ++insert_no;
        sequences.lookup(dummy, lookup_key) ? ++lookup_ok : ++lookup_no;
      }
    };
  };

  const std::string keys[] = {"how", "now", "brown", "cow",   "grazing",
                              "in",  "the", "green", "green", "grass"};
  std::srand(0);
  const auto rand_key = [&keys]() {
    // It's biased, but so is everybody, right?
    return keys[std::rand() % std::size(keys)];
  };

  std::vector<std::thread> workers;
  for (int i = 0; i < num_workers; ++i) {
    // Pseudo-randomly select separate elements from `keys` for this worker
    // to insert and lookup, respectively.
    workers.push_back(std::thread{worker(rand_key(), rand_key())});
  }

  for (auto& thread : workers) {
    thread.join();
  }

  std::cout << "insert_ok: " << insert_ok << '\n'
            << "insert_no: " << insert_no << '\n'
            << "lookup_ok: " << lookup_ok << '\n'
            << "lookup_no: " << lookup_no << '\n';

  std::cout << "\ngoodbye\n";
}

void test_amortized_constant_lookups_one_thread() {
  ConcurrentHashTable<std::string, int> by_name;

  std::string key;
  std::ranlux48_base engine;  // seeded to zero

  long long found;
  long long not_found;

  for (long long i = 0;; ++i) {
    found = not_found = 0;
    const auto before = std::chrono::steady_clock::now();
    for (int j = 0; j < 1'000; ++j) {
      // Look up a pseudo-random key between `-i` and `i`.
      std::uniform_int_distribution<long long> rand_int(-i, i);
      key = std::to_string(rand_int(engine));
      if (by_name.lookup(key)) {
        ++found;
      } else {
        ++not_found;
      }
    }
    const auto after = std::chrono::steady_clock::now();
    std::cout << i << ' '
              << std::chrono::duration_cast<std::chrono::nanoseconds>(after -
                                                                      before)
                     .count()
              << ' ' << found << ' ' << not_found << std::endl;
    by_name.insert(std::to_string(i), i);
  }
}

int main() {
  // test_breathing();
  test_amortized_constant_lookups_one_thread();
}
